#pragma once

#include "helpers.cxx"

#define SESSION_WRITER_IDLE_SLEEP_US 50000
#define SESSION_QUEUE_MAX_DEPTH 20000
// Bounds how long the writer waits on a wedged producer before flushing regardless. Must stay
// comfortably under the unit's TimeoutStopSec, since a SIGKILL there would discard the tail.
#define SESSION_WRITER_DRAIN_GRACE_US 5000000

// One record per line, appended, never rewritten. NDJSON rather than CSV for two reasons that both
// matter here: the record types are heterogeneous (session header, thermal sample, supply
// telemetry, enrollment event, settling window) and would otherwise need a sparse union of every
// column; and a hard power cut at ignition-off truncates mid-line, which costs exactly one
// discardable line instead of corrupting a fixed-width row.
void enqueueRecord(std::string&& line) {
    auto* queued = new std::string(std::move(line));

    // Bounded rather than unbounded: if the writer thread ever wedges, the queue must not consume
    // memory until the OOM killer takes the logger down. Dropping is counted and reported.
    if (g_async_queue_length(appData.session.queue) > SESSION_QUEUE_MAX_DEPTH) {
        appData.session.recordsDropped++;
        delete queued;
        return;
    }

    g_async_queue_push(appData.session.queue, queued);
}

std::string getRecordPrefix(const gchar* type) {
    auto taiUs = getCurrentTimeUs();
    auto bootUs = getBootTimeUs();
    return std::format(
        "{{\"t\":\"{}\",\"taiUs\":{},\"bootUs\":{},\"sessionUs\":{}",
        type, taiUs, bootUs, bootUs - appData.sessionStartBootUs);
}

void writeSessionRecord(const gchar* type, const std::string& fields) {
    enqueueRecord(std::format("{},{}}}\n", getRecordPrefix(type), fields));
}

void writeEventRecord(const gchar* severity, const std::string& message) {
    writeSessionRecord("event", std::format(
        "\"severity\":\"{}\",\"message\":\"{}\"", severity, escapeJson(message)));
}

gboolean openSession() {
    std::error_code dirError;
    std::filesystem::create_directories(appConfig.sessionsDirPath, dirError);
    if (dirError) {
        g_critical("SessionWriter: could not create '%s': %s",
            appConfig.sessionsDirPath.c_str(), dirError.message().c_str());
        return FALSE;
    }

    auto stamp = getIsoTimestamp(getCurrentTimeUs());
    std::replace(stamp.begin(), stamp.end(), ':', '-');
    auto modeTag = appData.mode == ModeEnroll ? "enroll" : "log";
    appData.session.filePath = std::format("{}/{}-{}{}",
        appConfig.sessionsDirPath, stamp, modeTag, SESSION_FILE_EXTENSION);

    appData.session.fd = open(appData.session.filePath.c_str(),
        O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (appData.session.fd < 0) {
        g_critical("SessionWriter: could not open '%s': %s",
            appData.session.filePath.c_str(), strerror(errno));
        return FALSE;
    }

    // fsync on the file does not make a NEWLY CREATED file's directory entry durable, so a hard
    // cut seconds after session start could lose the whole file rather than its tail. One fsync on
    // the containing directory closes that, and it is only needed once.
    auto dirFd = open(appConfig.sessionsDirPath.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirFd < 0) {
        g_warning("SessionWriter: could not open sessions dir to fsync it: %s", strerror(errno));
    }
    else {
        if (fsync(dirFd) != 0) {
            g_warning("SessionWriter: could not fsync sessions dir: %s", strerror(errno));
        }
        close(dirFd);
    }

    g_message("SessionWriter: session file '%s'", appData.session.filePath.c_str());
    return TRUE;
}

gboolean writeLine(const std::string& line) {
    auto remaining = line.size();
    auto* cursor = line.data();

    while (remaining > 0) {
        auto written = write(appData.session.fd, cursor, remaining);
        if (written < 0) {
            if (errno == EINTR) { continue; }
            g_critical("SessionWriter: write failed: %s", strerror(errno));
            return FALSE;
        }
        remaining -= written;
        cursor += written;
    }

    appData.session.recordsWritten++;
    return TRUE;
}

// The 1 s bound on durable loss only holds if the queue is drained to empty before each fsync —
// queue depth would otherwise add to the loss window invisibly.
guint drainQueue() {
    guint drained = 0;
    while (auto* queued = (std::string*)g_async_queue_try_pop(appData.session.queue)) {
        if (!writeLine(*queued)) { requestShutdown("session write failed"); }
        delete queued;
        drained++;
    }
    return drained;
}

void fsyncIfDue(gboolean force) {
    auto nowBootUs = getBootTimeUs();
    auto intervalUs = (guint64)appConfig.fsyncIntervalMs * 1000;

    if (!force && nowBootUs - appData.session.lastFsyncBootUs < intervalUs) { return; }

    // fdatasync rather than fsync: for a file only ever appended to, the size is metadata needed
    // to retrieve the data, so fdatasync flushes it, and it skips the timestamp updates fsync
    // would also force. Same durability for this access pattern, less work per second.
    if (fdatasync(appData.session.fd) != 0) {
        g_critical("SessionWriter: fdatasync failed: %s", strerror(errno));
        requestShutdown("session fdatasync failed");
        return;
    }

    appData.session.lastFsyncBootUs = nowBootUs;
}

gpointer sessionWriterLoop(gpointer _) {
    g_message("SessionWriter: starting");

    appData.session.lastFsyncBootUs = getBootTimeUs();
    appData.session.isRunning = true;

    // Deliberately NOT `while (!shutdownRequested)`. Every worker sees the shutdown flag at the
    // same moment, so a writer that exits on the flag alone can finish draining before another
    // worker enqueues its closing record — and that record is then lost with no trace. The writer
    // therefore keeps serving until the last producer has decremented itself away.
    guint64 shutdownSeenBootUs = 0;

    while (!appData.shutdownRequested || appData.producersRunning > 0) {
        auto drained = drainQueue();
        fsyncIfDue(FALSE);
        if (drained == 0) { g_usleep(SESSION_WRITER_IDLE_SLEEP_US); }

        if (!appData.shutdownRequested) { continue; }

        // A wedged producer must not hold the writer open until systemd's TimeoutStopSec expires
        // and SIGKILLs the process, because that would discard the very tail this worker exists
        // to protect. Past the grace period the writer stops waiting and flushes what it has.
        auto nowBootUs = getBootTimeUs();
        if (shutdownSeenBootUs == 0) { shutdownSeenBootUs = nowBootUs; }

        if (nowBootUs - shutdownSeenBootUs > SESSION_WRITER_DRAIN_GRACE_US) {
            g_warning("SessionWriter: %d producer(s) still running after %.1f s, flushing anyway",
                (int)appData.producersRunning, (nowBootUs - shutdownSeenBootUs) / 1000000.0);
            break;
        }
    }

    // The tail matters more than anything else this worker does: the record most worth having is
    // whatever was written just before the supply died.
    drainQueue();
    fsyncIfDue(TRUE);

    appData.session.isRunning = false;
    g_message("SessionWriter: shutting down, %lu records written, %lu dropped",
        (unsigned long)appData.session.recordsWritten, (unsigned long)appData.session.recordsDropped);

    return NULL;
}

void writeSessionHeader() {
    auto bootWallUs = appData.bootTimeUs;

    writeSessionRecord("session", std::format(
        "\"appVersion\":\"{}\",\"mode\":\"{}\","
        "\"isoTime\":\"{}\",\"bootWallTaiUs\":{},"
        "\"clockSynchronised\":{},"
        "\"fsyncIntervalMs\":{},\"tempIntervalMs\":{},\"supplyIntervalMs\":{},"
        "\"clockWarning\":\"{}\"",
        APP_VERSION,
        appData.mode == ModeEnroll ? "enroll" : "log",
        getIsoTimestamp(getCurrentTimeUs()),
        bootWallUs,
        // Recorded, not trusted. The car has no network at all, so in the field this is expected
        // to read false and the wall clock is then wrong by however long since the last bench
        // sync, while looking entirely plausible. sessionUs and bootUs are the trustworthy axes.
        readWholeFile("/run/systemd/timesync/synchronized").has_value() ? "true" : "false",
        appConfig.fsyncIntervalMs, appConfig.tempIntervalMs, appConfig.supplyIntervalMs,
        "wall clock is unsynchronised in the car; use bootUs/sessionUs for intra-session timing"));
}
