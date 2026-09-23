#pragma once

#include "blePackets.cxx"

// The DS18B20 enrollment and sampling worker. It owns `temp0`-`temp3`, packet 0x602 and packet
// 0x603, and the binding store that makes a channel mean anything at all.
//
// A DS18B20 has no positional anchor: four identical parts share one bus and are addressed only by
// 64-bit ROM ID, so the ROM ID *is* the channel definition. This worker is therefore the only
// thing standing between the rig and silently mislabelled temperature data - which is worse than
// no data, because dT_preheat rests on the DIFFERENCES between these four probes and a swapped
// pair inverts the quantity the whole thermal workstream exists to produce.
//
// Five rules make that safe, and all five come from the plan rather than from taste:
//
//   1. `28-*` and nothing else, at enumeration, at binding and at every read. The bare bus
//      invented churning `00-*` phantoms on a 10 s cycle while GPIO4 floated; anything that
//      auto-binds "the next device that appears" binds noise within about ten seconds. The
//      phantoms stopped once R11 terminated the line, so this filter can no longer be exercised
//      on the bench and its correctness now rests on the parser.
//   2. Binding happens ONLY under --enroll. Outside enrollment an unknown ROM ID is invalid data
//      and a logged event, never a new channel, so a dropout and reconnect mid-session cannot
//      re-label a channel.
//   3. One probe per enrollment step. If two unbound `28-*` IDs turn up in the same scan the
//      arrival order between them is unknowable - sysfs order is not arrival order - so the step
//      is refused out loud rather than guessed.
//   4. The binding carries provenance: the ROM ID, the channel and the bind timestamp. It lives in
//      KnurLogger.ini beside the binary, in the same [thermal] section as the calibration offsets
//      (owner decision, 2026-09-10, replacing a separate channels.ini in the data directory). One
//      file holds the whole logger configuration, and the slot-keyed offsets sit next to the
//      bindings that decide which probe each slot holds - which is where the "re-check the offsets
//      after any re-enrollment" consequence is visible rather than filed elsewhere.
//   5. Enrollment reports which ROM ID it bound, so the binding can be checked against the lead
//      being plugged in.
//
// Two further properties are not in that list and are just as load-bearing:
//
//   - A BOUND channel whose ROM ID goes absent is logged as present-but-invalid, never omitted.
//     An omitted channel makes a mid-session dropout indistinguishable from the logger not having
//     run, and that distinction is the entire reason the SD record exists.
//   - Enrollment does NOT stop after the fourth bind. With all four bound, warming one probe by
//     hand and watching one channel move is what confirms the map in situ, and that check needs a
//     logger that is still sampling and still publishing to the phone.

#define ONE_WIRE_IDLE_SLEEP_US 50000
#define ONE_WIRE_SLAVE_FILE "w1_slave"
#define ONE_WIRE_BULK_READ_FILE "therm_bulk_read"
// w1_therm accepts this command only at its exact `sizeof()`, newline included. See the write below.
#define ONE_WIRE_BULK_TRIGGER_CMD "trigger\n"
#define ONE_WIRE_BULK_POLL_US 20000
#define ONE_WIRE_BULK_MAX_WAIT_US 1500000

// How much of an unparseable read is kept in the record. Enough to see what the kernel actually
// returned, bounded so that a bus producing garbage cannot fill the card with it.
#define ONE_WIRE_UNPARSED_SAMPLE_MAX 120

typedef struct {
    gboolean isBusPresent;
    std::vector<std::string> romIds;
    guint32 nonProbeEntries;
} OneWireScan;

std::string getChannelName(gint index) { return std::format("temp{}", index); }

std::string getRomIdListJson(const std::vector<std::string>& romIds) {
    std::string list;
    for (const auto& romId : romIds) {
        list += std::format("{}\"{}\"", list.empty() ? "" : ",", romId);
    }
    return list;
}

// The family filter, and the only place it may live. Everything downstream of this function sees
// `28-*` devices or nothing.
OneWireScan scanOneWireBus() {
    OneWireScan scan = {};

    auto* dir = opendir(ONE_WIRE_DEVICES_DIR);
    if (dir == NULL) {
        if (appConfig.verboseMode) {
            g_warning("OneWire: cannot open %s: %s", ONE_WIRE_DEVICES_DIR, strerror(errno));
        }
        return scan;
    }

    while (auto* entry = readdir(dir)) {
        if (g_str_equal(entry->d_name, ".") || g_str_equal(entry->d_name, "..")) { continue; }

        // A working bus always registers its master, so the master's presence is the bus's
        // liveness signal and an EMPTY directory is the real failure. Zero probes is a perfectly
        // good result - at the bench there are none, because they are installed on the car.
        if (g_str_equal(entry->d_name, ONE_WIRE_MASTER_NAME)) { scan.isBusPresent = TRUE; continue; }

        if (g_str_has_prefix(entry->d_name, ONE_WIRE_DS18B20_PREFIX)
            && strlen(entry->d_name) == ONE_WIRE_ROM_ID_LENGTH) {
            scan.romIds.push_back(entry->d_name);
            continue;
        }

        scan.nonProbeEntries++;
    }

    closedir(dir);

    // Sorted so that a scan's order is reproducible. This is NOT arrival order and nothing may
    // treat it as such - it is only here so that two consecutive scans of an unchanged bus produce
    // identical records.
    std::sort(scan.romIds.begin(), scan.romIds.end());

    return scan;
}

// `therm_bulk_read` is a master attribute that w1_therm only registers once a slave of its family
// is attached, so it does not exist on a probe-less bench box and cannot be tested there. It is
// the documented way to convert every probe at once, which is what makes 1 Hz reachable with four
// of them; without it each read costs its own ~750 ms conversion and a four-probe cycle takes ~3 s.
std::string getBulkReadPath() {
    return std::format("{}/{}/{}", ONE_WIRE_DEVICES_DIR, ONE_WIRE_MASTER_NAME, ONE_WIRE_BULK_READ_FILE);
}

// `outErrno` is the whole point of this being a function rather than three inline calls: a refused
// write to a sysfs attribute is uninformative without it, and this one is refused on a box where
// nobody can watch it happen.
gboolean writeSysfsValue(const std::string& path, const gchar* value, int* outErrno) {
    *outErrno = 0;

    auto fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) { *outErrno = errno; return FALSE; }

    auto length = (ssize_t)strlen(value);
    auto written = write(fd, value, length);
    if (written != length) { *outErrno = errno; }

    // After close(), errno no longer describes the write.
    close(fd);

    return written == length;
}

// `therm_bulk_read` reads back "-1" while a conversion is running, "1" when the results are ready
// and unread, and "0" when no bulk conversion is pending - which after a fresh trigger means the
// trigger did nothing. **"0" and "1" are not the same answer and treating them alike is what hid a
// dead optimisation for a day** (measured 2026-09-10).
//
// A successful WRITE proves nothing at all: `therm_bulk_read_store` returns the write size
// unconditionally and reports its refusal only to the kernel log, so the only trustworthy evidence
// is the readback. That is why `didConvert` and not the write result decides what gets reported.
//
// **The steady-state answer is "1", not "-1".** The kernel sleeps for the whole conversion inside
// the write - `trigger_bulk_read()` issues SKIP ROM + CONVERT T, then `msleep_interruptible(t_conv)`
// before marking every slave ready - so by the time `write()` returns the results are already there.
// "-1" is only observable by a concurrent reader, and this worker is the only one, so a poll loop
// that recognised a conversion solely by seeing "-1" could never recognise one.
typedef struct {
    gboolean isAvailable;
    gboolean didConvert;
    std::string state;
    guint32 waitMs;
} BulkReadResult;

// A failure here is not an error: the per-probe reads that follow are correct either way, they
// just each pay for their own conversion. That is exactly why this optimisation is safe to ship
// on a box where it cannot be exercised - its worst case is the behaviour without it.
BulkReadResult triggerBulkConversion() {
    BulkReadResult result = {};

    auto path = getBulkReadPath();
    if (!std::filesystem::exists(path)) { return result; }

    // Timed from BEFORE the write, because the kernel does the waiting inside it: the poll loop
    // below normally finds the results already there, so a clock started after `write()` returns
    // measures nothing and would put a 0 on 0x603 bytes 6-7 for the most expensive part of the
    // cycle. It also separates a real conversion from a bulk path that ran and failed - a failed
    // bus reset still marks every slave ready, but it never sleeps, so it costs ~0 ms.
    auto startUs = getBootTimeUs();

    // The trailing newline is load-bearing and cost a day. `therm_bulk_read_store` gates on
    // `size == sizeof("trigger")`, which is 8 - a 7-byte write misses by one, never reaches
    // `trigger_bulk_read()`, and is answered with the write size anyway, so the failure is visible
    // only as `err=-22` in the kernel log and as a readback of "0" here. Measured at the car
    // 2026-09-10: every cycle of the session logged it, and no probe was ever bulk-converted.
    int writeErrno = 0;
    if (!writeSysfsValue(path, ONE_WIRE_BULK_TRIGGER_CMD, &writeErrno)) {
        // Reported once and into the session file, not once per cycle into the console: the
        // refusal is a permanent property of the box, so the first occurrence is the evidence and
        // the next 863 are noise. EACCES means the master attribute is root-owned and the logger
        // is not root, which is the expected reason and the one a udev rule would answer.
        if (!appData.thermal.isBulkTriggerRefusalReported) {
            appData.thermal.isBulkTriggerRefusalReported = TRUE;
            g_warning("OneWire: bulk read trigger refused on '%s': %s (errno %d)."
                " Falling back to per-probe reads, which each pay their own ~750 ms conversion.",
                path.c_str(), g_strerror(writeErrno), writeErrno);
            writeEventRecord("warning", std::format(
                "bulk read trigger refused on '{}': {} (errno {}), per-probe reads in use",
                path, g_strerror(writeErrno), writeErrno));
        }
        return result;
    }

    appData.thermal.isBulkTriggerRefusalReported = FALSE;
    result.isAvailable = TRUE;

    while (getBootTimeUs() - startUs < ONE_WIRE_BULK_MAX_WAIT_US) {
        if (appData.shutdownRequested) { break; }

        auto state = readWholeFile(path);
        if (!state.has_value()) { break; }

        result.state = trim(*state);

        // "1" is the normal answer and means the kernel converted every supporting probe and is
        // holding the results - the per-probe reads below then fetch the scratchpad instead of
        // starting a conversion each. "-1" is a conversion still running, which only happens if
        // the write was interrupted, and is worth waiting out.
        if (result.state != "-1") {
            result.didConvert = result.state == "1";
            break;
        }

        result.didConvert = TRUE;
        g_usleep(ONE_WIRE_BULK_POLL_US);
    }

    result.waitMs = (guint32)((getBootTimeUs() - startUs) / 1000);

    // The write was accepted and the kernel still reports nothing pending. Something is stopping
    // the bulk path from doing anything, and the per-probe reads below will each pay a full
    // conversion - so this is the whole optimisation silently not happening, and it says so once
    // rather than leaving a zero in the record to be puzzled over. Check the kernel log for
    // `therm_bulk_read_store`: it prints the errno this interface refuses to return.
    if (!result.didConvert && !appData.thermal.isBulkNoOpReported) {
        appData.thermal.isBulkNoOpReported = TRUE;
        g_warning("OneWire: bulk read accepted the trigger but reported no conversion"
            " (therm_bulk_read read back '%s'). '0' means nothing is pending, so the trigger was"
            " rejected or no device on the bus supports bulk reading - `journalctl -k | grep"
            " therm_bulk_read_store` has the errno. Per-probe reads are being used, ~800 ms each.",
            result.state.c_str());
        writeEventRecord("warning", std::format(
            "bulk read trigger accepted but no conversion reported, therm_bulk_read='{}',"
            " per-probe reads in use", result.state));
    }

    return result;
}

// w1_therm exposes per-probe attributes that say WHY a bulk conversion might do nothing, and they
// only exist once a probe is attached - so they cannot be read at worker start on a bench box, and
// are dumped once on the first cycle that sees any probe. `ext_power` is the first suspect: a
// bulk conversion of parasite-powered probes needs a strong pullup this bus does not have, and
// `w1-gpio`'s `pullup` parameter is ignored on this firmware.
void reportProbeCapabilities(const OneWireScan& scan) {
    if (appData.thermal.isProbeInfoReported || scan.romIds.empty()) { return; }
    appData.thermal.isProbeInfoReported = TRUE;

    static const gchar* attributeNames[] = { "ext_power", "resolution", "conv_time", "features" };

    std::string probes;
    for (const auto& romId : scan.romIds) {
        std::string fields;
        for (const auto* name : attributeNames) {
            auto value = readWholeFile(std::format("{}/{}/{}", ONE_WIRE_DEVICES_DIR, romId, name));
            fields += std::format("{}\"{}\":{}", fields.empty() ? "" : ",", name,
                value.has_value() ? std::format("\"{}\"", escapeJson(trim(*value))) : "null");
        }
        probes += std::format("{}{{\"romId\":\"{}\",{}}}", probes.empty() ? "" : ",", romId, fields);
    }

    auto masterFeatures = readWholeFile(
        std::format("{}/{}/features", ONE_WIRE_DEVICES_DIR, ONE_WIRE_MASTER_NAME));
    auto bulkState = readWholeFile(getBulkReadPath());

    writeSessionRecord("probeCapabilities", std::format(
        "\"masterFeatures\":{},\"bulkReadState\":{},\"probes\":[{}]",
        masterFeatures.has_value() ? std::format("\"{}\"", escapeJson(trim(*masterFeatures))) : "null",
        bulkState.has_value() ? std::format("\"{}\"", escapeJson(trim(*bulkState))) : "null",
        probes));
}

// w1_therm's `w1_slave` attribute in preference to its `temperature` attribute, because it carries
// the nine scratchpad bytes and an explicit CRC verdict alongside the value. Commissioning item 4
// asks for the raw reading and a per-sample validity flag, and `temperature` gives neither - it
// returns a bare number or an error, which throws away the evidence that separates a marginal bus
// from a hot probe.
ProbeReading readProbe(const std::string& romId) {
    ProbeReading reading = {};
    reading.centiC = TEMP_CENTI_C_INVALID;
    reading.invalidReason = "absent";

    auto path = std::format("{}/{}/{}", ONE_WIRE_DEVICES_DIR, romId, ONE_WIRE_SLAVE_FILE);

    auto startUs = getBootTimeUs();
    auto content = readWholeFile(path);
    reading.readMs = (guint32)((getBootTimeUs() - startUs) / 1000);

    if (!content.has_value()) {
        reading.invalidReason = "readFailed";
        reading.isReadError = TRUE;
        return reading;
    }

    reading.isPresent = TRUE;

    auto crcPos = content->find("crc=");
    auto valuePos = content->find("t=");

    // w1_therm prints `crc=xx YES|NO` and `t=<millidegrees>` whenever the probe answered at all -
    // a CRC failure still prints both. Content carrying neither means the kernel got nothing back,
    // which on this rig is overwhelmingly a lead that has been pulled: a removed probe keeps its
    // sysfs directory for up to ~100 s (w1_slave_ttl of 10 missed searches at w1_master_timeout =
    // 10 s) and reads back empty for all of it. That is the same fault as `absent`, arriving
    // through the one path that still had a directory to read, so it is held to the same rule -
    // it must NOT reach the read-error counter, because a dropped lead and a marginal bus send you
    // to different parts of the car. Measured 2026-09-10 at the car: enrolling by unplugging each
    // probe as the next went in charged 186 read errors to a bus that had not failed once.
    if (crcPos == std::string::npos && valuePos == std::string::npos) {
        reading.invalidReason = "notAnswering";
        reading.unparsedContent = content->substr(0, ONE_WIRE_UNPARSED_SAMPLE_MAX);
        return reading;
    }

    // Half a reading is a different animal: the probe answered and the answer is malformed, which
    // is a bus-quality fact and is counted as one.
    if (crcPos == std::string::npos || valuePos == std::string::npos) {
        reading.invalidReason = "unparseable";
        reading.isReadError = TRUE;
        reading.unparsedContent = content->substr(0, ONE_WIRE_UNPARSED_SAMPLE_MAX);
        return reading;
    }

    auto crcLineEnd = content->find('\n', crcPos);
    auto crcLine = crcLineEnd == std::string::npos
        ? content->substr(crcPos)
        : content->substr(crcPos, crcLineEnd - crcPos);
    reading.crcOk = crcLine.find("YES") != std::string::npos;

    auto separator = content->find(':');
    if (separator != std::string::npos) { reading.scratchpadHex = trim(content->substr(0, separator)); }

    try { reading.milliC = (gint32)std::stol(content->substr(valuePos + 2)); }
    catch (const std::exception&) {
        reading.invalidReason = "unparseable";
        reading.isReadError = TRUE;
        reading.unparsedContent = content->substr(0, ONE_WIRE_UNPARSED_SAMPLE_MAX);
        return reading;
    }

    if (!reading.crcOk) { reading.invalidReason = "crc"; reading.isReadError = TRUE; return reading; }

    auto centiC = (reading.milliC >= 0 ? reading.milliC + 5 : reading.milliC - 5) / 10;

    // 85.00 C is the DS18B20's power-on scratchpad default: the probe answered but never
    // converted, which points at power or a marginal pull-up rather than at a hot probe. Flagged
    // with its own reason rather than lumped in with a CRC failure, because the two send whoever
    // reads the record to different parts of the board.
    if (centiC == TEMP_POWER_ON_DEFAULT_CENTI_C) {
        reading.invalidReason = "powerOnDefault";
        reading.isReadError = TRUE;
        return reading;
    }

    if (centiC < TEMP_VALID_MIN_CENTI_C || centiC > TEMP_VALID_MAX_CENTI_C) {
        reading.invalidReason = "outOfRange";
        reading.isReadError = TRUE;
        return reading;
    }

    reading.centiC = (gint16)centiC;
    reading.isValid = TRUE;
    reading.invalidReason = NULL;

    return reading;
}

// The bindings live in the [thermal] section of KnurLogger.ini beside the binary, alongside the
// calibration offsets, so that one file holds the whole logger configuration (owner decision,
// 2026-09-10, replacing a separate machine-written channels.ini in the data directory). A missing
// or empty `temp<N>RomId` is an unbound channel and a legitimate state, unlike a missing offset,
// which is a startup failure: a logger that has never been to the car has no bindings to have.
void loadChannelStore() {
    auto* store = g_key_file_new();

    GError* error = NULL;
    if (!g_key_file_load_from_file(store, appConfig.configFilePath.c_str(), G_KEY_FILE_NONE, &error)) {
        // Unreachable in practice - loadConfig() has already read this file and exits if it cannot
        // - so this is the "somebody deleted it between the two reads" case, not a normal path.
        g_message("OneWire: could not re-read '%s' for bindings (%s), no channel is bound",
            appConfig.configFilePath.c_str(), error->message);
        g_clear_error(&error);
        g_key_file_free(store);
        return;
    }

    for (auto i = 0; i < TEMP_CHANNEL_COUNT; i++) {
        auto name = getChannelName(i);
        auto romIdKey = std::format(CONFIG_KEY_TEMP_ROM_ID_FORMAT, i);
        auto* romId = g_key_file_get_string(store, CONFIG_GROUP_THERMAL, romIdKey.c_str(), NULL);
        if (romId == NULL) { continue; }

        if (strlen(g_strstrip(romId)) == 0) { g_free(romId); continue; }

        // The family filter applies to the CONFIG too, not only to the bus. A `00-*` phantom that
        // somehow reached the file would otherwise be honoured forever, which is the one failure
        // the filter exists to prevent, arriving by the back door. The file is hand-editable now,
        // so this guard also catches a typed ROM ID rather than only a machine-written one.
        if (!g_str_has_prefix(romId, ONE_WIRE_DS18B20_PREFIX) || strlen(romId) != ONE_WIRE_ROM_ID_LENGTH) {
            g_warning("OneWire: '%s' is not a DS18B20 ROM ID ('%s'), ignoring it",
                romIdKey.c_str(), romId);
            writeEventRecord("error", std::format(
                "config holds non-DS18B20 ROM ID '{}' on {}, ignored", romId, name));
            g_free(romId);
            continue;
        }

        // One probe cannot serve two channels, and a config that says otherwise would produce two
        // channels tracking each other perfectly - a mislabelling that looks like agreement.
        gboolean isDuplicate = FALSE;
        for (auto j = 0; j < i; j++) {
            if (appData.thermal.channels[j].isBound && appData.thermal.channels[j].romId == romId) {
                isDuplicate = TRUE;
            }
        }
        if (isDuplicate) {
            g_warning("OneWire: ROM ID '%s' is bound to more than one channel, ignoring %s",
                romId, name.c_str());
            writeEventRecord("error", std::format(
                "config binds ROM ID '{}' to more than one channel, {} ignored", romId, name));
            g_free(romId);
            continue;
        }

        auto& channel = appData.thermal.channels[i];
        channel.romId = romId;
        channel.isBound = TRUE;
        auto boundKey = std::format(CONFIG_KEY_TEMP_BOUND_TAI_US_FORMAT, i);
        channel.boundTaiUs = g_key_file_get_uint64(store, CONFIG_GROUP_THERMAL, boundKey.c_str(), NULL);
        g_free(romId);
    }

    g_key_file_free(store);
}

// Written through a temporary and renamed, with the containing directory fsync'd. The bindings are
// the one piece of state whose loss costs a trip to the car, and the power cut they have to
// survive is the fuse being pulled, which arrives without warning during exactly this write.
gboolean writeFileAtomically(const std::string& path, const std::string& content) {
    auto tempPath = std::format("{}.tmp", path);
    auto fd = open(tempPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        g_critical("OneWire: could not open '%s': %s", tempPath.c_str(), strerror(errno));
        return FALSE;
    }

    gboolean isWritten = TRUE;
    auto remaining = content.size();
    auto* cursor = content.data();
    while (remaining > 0) {
        auto written = write(fd, cursor, remaining);
        if (written < 0) {
            if (errno == EINTR) { continue; }
            g_critical("OneWire: could not write '%s': %s", tempPath.c_str(), strerror(errno));
            isWritten = FALSE;
            break;
        }
        remaining -= written;
        cursor += written;
    }

    if (isWritten && fdatasync(fd) != 0) {
        g_critical("OneWire: could not fdatasync '%s': %s", tempPath.c_str(), strerror(errno));
        isWritten = FALSE;
    }
    close(fd);

    if (!isWritten) { unlink(tempPath.c_str()); return FALSE; }

    if (rename(tempPath.c_str(), path.c_str()) != 0) {
        g_critical("OneWire: could not rename '%s' into place: %s", tempPath.c_str(), strerror(errno));
        unlink(tempPath.c_str());
        return FALSE;
    }

    // The rename itself is metadata on the containing directory, so without this the file can be
    // durable and still not be reachable after a cut.
    auto parentDir = std::filesystem::path(path).parent_path().string();
    auto dirFd = open(parentDir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirFd < 0) {
        g_warning("OneWire: could not open '%s' to fsync it: %s", parentDir.c_str(), strerror(errno));
        return TRUE;
    }
    if (fsync(dirFd) != 0) {
        g_warning("OneWire: could not fsync '%s': %s", parentDir.c_str(), strerror(errno));
    }
    close(dirFd);

    return TRUE;
}

// The key an ini line assigns to, or "" for a blank line, a comment or a section header.
std::string getIniLineKey(const std::string& line) {
    auto trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';' || trimmed[0] == '[') { return ""; }

    auto equals = trimmed.find('=');
    if (equals == std::string::npos) { return ""; }

    return trim(trimmed.substr(0, equals));
}

// KnurLogger.ini is hand-maintained and its comment block is the most useful documentation in this
// repository, so --enroll rewrites it LINE BY LINE rather than through g_key_file_to_data(), which
// re-encodes comments and destroys every non-ASCII character in them - measured 2026-09-10, the
// em-dashes in this file's header came back as '?'. Every line the binding keys do not name is
// copied through byte for byte, so a hand-written comment survives however it is spelled.
gboolean saveChannelStore() {
    auto original = readWholeFile(appConfig.configFilePath);
    if (!original.has_value()) {
        g_critical("OneWire: could not read '%s' to update the bindings: %s",
            appConfig.configFilePath.c_str(), strerror(errno));
        return FALSE;
    }

    std::vector<std::string> lines;
    std::string current;
    for (auto character : *original) {
        if (character == '\n') { lines.push_back(current); current.clear(); continue; }
        current += character;
    }
    auto endsWithNewline = current.empty() && !original->empty();
    if (!current.empty()) { lines.push_back(current); }

    auto sectionHeader = std::format("[{}]", CONFIG_GROUP_THERMAL);
    size_t sectionStart = lines.size();
    for (size_t i = 0; i < lines.size(); i++) {
        if (trim(lines[i]) == sectionHeader) { sectionStart = i; break; }
    }
    if (sectionStart == lines.size()) {
        // Not a recoverable condition: loadConfig() requires four offsets in this section, so a
        // file without it could not have started the logger.
        g_critical("OneWire: '%s' has no %s section to write the bindings into",
            appConfig.configFilePath.c_str(), sectionHeader.c_str());
        return FALSE;
    }

    size_t sectionEnd = lines.size();
    for (size_t i = sectionStart + 1; i < lines.size(); i++) {
        auto trimmed = trim(lines[i]);
        if (!trimmed.empty() && trimmed[0] == '[') { sectionEnd = i; break; }
    }

    // An unbound channel's keys are written empty rather than deleted, so that all four slots are
    // visible in the file whether or not anything is enrolled, and so that --reset leaves evidence
    // of itself rather than a section that looks like it was never written.
    std::vector<std::pair<std::string, std::string>> wanted;
    for (auto i = 0; i < TEMP_CHANNEL_COUNT; i++) {
        auto& channel = appData.thermal.channels[i];
        wanted.emplace_back(std::format(CONFIG_KEY_TEMP_ROM_ID_FORMAT, i),
            channel.isBound ? channel.romId : "");
        wanted.emplace_back(std::format(CONFIG_KEY_TEMP_BOUND_TAI_US_FORMAT, i),
            channel.isBound ? std::format("{}", channel.boundTaiUs) : "");
        // A hand-created binding carries no bind time, and stamping the epoch on it would read as
        // a broken clock in the one field whose job is provenance.
        wanted.emplace_back(std::format(CONFIG_KEY_TEMP_BOUND_ISO_FORMAT, i),
            !channel.isBound ? ""
                : channel.boundTaiUs == 0 ? "unknown" : getIsoTimestamp(channel.boundTaiUs));
    }

    std::vector<gboolean> isPlaced(wanted.size(), FALSE);
    for (auto i = sectionStart + 1; i < sectionEnd; i++) {
        auto key = getIniLineKey(lines[i]);
        if (key.empty()) { continue; }

        for (size_t w = 0; w < wanted.size(); w++) {
            if (key != wanted[w].first) { continue; }
            lines[i] = std::format("{}={}", wanted[w].first, wanted[w].second);
            isPlaced[w] = TRUE;
            break;
        }
    }

    // Appended after the section's last assignment rather than at its very end, so that a blank
    // line or a trailing comment separating this section from the next stays where it was put.
    auto insertAt = sectionStart + 1;
    for (auto i = sectionStart + 1; i < sectionEnd; i++) {
        if (!getIniLineKey(lines[i]).empty()) { insertAt = i + 1; }
    }

    std::vector<std::string> additions;
    for (size_t w = 0; w < wanted.size(); w++) {
        if (isPlaced[w]) { continue; }
        additions.push_back(std::format("{}={}", wanted[w].first, wanted[w].second));
    }
    lines.insert(lines.begin() + insertAt, additions.begin(), additions.end());

    std::string rewritten;
    for (size_t i = 0; i < lines.size(); i++) {
        rewritten += lines[i];
        if (i + 1 < lines.size() || endsWithNewline) { rewritten += '\n'; }
    }

    return writeFileAtomically(appConfig.configFilePath, rewritten);
}

std::string getBindingsJson() {
    std::string bindings;
    for (auto i = 0; i < TEMP_CHANNEL_COUNT; i++) {
        auto& channel = appData.thermal.channels[i];
        bindings += std::format(
            "{}{{\"ch\":{},\"name\":\"{}\",\"bound\":{},\"romId\":{},\"boundTaiUs\":{},"
            "\"boundIso\":{},\"offsetC\":{:.4f}}}",
            i == 0 ? "" : ",", i, getChannelName(i), channel.isBound ? "true" : "false",
            channel.isBound ? std::format("\"{}\"", channel.romId) : "null",
            channel.boundTaiUs,
            channel.isBound
                ? std::format("\"{}\"", channel.boundTaiUs == 0
                    ? "unknown" : getIsoTimestamp(channel.boundTaiUs))
                : "null",
            appConfig.tempOffsetsC[i]);
    }
    return bindings;
}

// --reset discards the whole store and enrolls from temp0 again. The discarded bindings are
// written into the session file FIRST: they are otherwise unrecoverable, and a wrong reset is a
// trip back to the car.
void discardChannelStore() {
    auto discarded = getBindingsJson();

    for (auto i = 0; i < TEMP_CHANNEL_COUNT; i++) {
        appData.thermal.channels[i] = {};
        appData.thermal.channels[i].centiC = TEMP_CENTI_C_INVALID;
    }

    writeSessionRecord("enrollment", std::format(
        "\"action\":\"reset\",\"discarded\":[{}]", discarded));

    if (!saveChannelStore()) {
        logError("OneWire: could not write the emptied binding store, exiting...");
        return;
    }

    g_message("OneWire: binding store discarded on --reset, enrolling from temp0");
}

gint findFreeChannel() {
    for (auto i = 0; i < TEMP_CHANNEL_COUNT; i++) {
        if (!appData.thermal.channels[i].isBound) { return i; }
    }
    return -1;
}

gint findChannelByRomId(const std::string& romId) {
    for (auto i = 0; i < TEMP_CHANNEL_COUNT; i++) {
        auto& channel = appData.thermal.channels[i];
        if (channel.isBound && channel.romId == romId) { return i; }
    }
    return -1;
}

gboolean wasUnknownRomIdReported(const std::string& romId) {
    auto& reported = appData.thermal.reportedUnknownRomIds;
    if (std::find(reported.begin(), reported.end(), romId) != reported.end()) { return TRUE; }
    reported.push_back(romId);
    return FALSE;
}

void bindChannel(gint channelIndex, const std::string& romId, const ProbeReading& reading) {
    auto& channel = appData.thermal.channels[channelIndex];
    channel.romId = romId;
    channel.isBound = TRUE;
    channel.boundTaiUs = getCurrentTimeUs();

    auto name = getChannelName(channelIndex);
    auto readingText = reading.isValid
        ? std::format("{:.2f} C", reading.centiC / 100.0)
        : std::format("INVALID ({})", reading.invalidReason == NULL ? "unknown" : reading.invalidReason);

    // Reported loudly and with the reading taken at the moment of binding, because rule 5 is what
    // makes the binding checkable against the lead being plugged in - and because at the car the
    // only other feedback channel is a phone watching 0x602.
    g_message("OneWire: BOUND %s <- %s, reading %s", name.c_str(), romId.c_str(), readingText.c_str());

    writeSessionRecord("enrollment", std::format(
        "\"action\":\"bound\",\"ch\":{},\"name\":\"{}\",\"romId\":\"{}\",\"boundTaiUs\":{},"
        "\"readingValid\":{},\"readingCentiC\":{},\"scratchpadHex\":\"{}\"",
        channelIndex, name, romId, channel.boundTaiUs,
        reading.isValid ? "true" : "false", (int)reading.centiC, escapeJson(reading.scratchpadHex)));

    if (!saveChannelStore()) {
        // A binding that is not durable is worse than no binding, because the next run reads the
        // store and not this process's memory. Fail loudly rather than carry on enrolling into a
        // store nobody can save.
        logError("OneWire: could not persist the binding for %s, exiting...", name.c_str());
    }
}

void runEnrollmentStep(const std::vector<std::string>& unboundRomIds,
    const std::vector<ProbeReading>& readings, const std::vector<std::string>& scannedRomIds) {
    if (unboundRomIds.empty()) {
        appData.thermal.lastAmbiguousRomIds.clear();
        return;
    }

    // Rule 3. sysfs order is not arrival order, so with two unbound probes on the bus there is no
    // fact available that says which one was plugged in first. Refusing is the only correct move:
    // a guess here produces two mislabelled channels and no evidence that it happened.
    if (unboundRomIds.size() > 1) {
        auto idList = getRomIdListJson(unboundRomIds);
        if (appData.thermal.lastAmbiguousRomIds == idList) { return; }
        appData.thermal.lastAmbiguousRomIds = idList;

        g_warning("OneWire: %zu unbound probes on the bus at once, refusing to guess arrival order"
            " - plug them in ONE at a time", unboundRomIds.size());
        writeSessionRecord("enrollment", std::format(
            "\"action\":\"refusedAmbiguous\",\"unboundRomIds\":[{}],\"note\":\"{}\"",
            idList, "sysfs order is not arrival order; enrollment binds one probe per step"));
        return;
    }

    appData.thermal.lastAmbiguousRomIds.clear();

    const auto& romId = unboundRomIds.front();
    auto channelIndex = findFreeChannel();

    // Enrollment deliberately keeps running with every channel bound, so that the in-situ
    // identification check - warm one probe by hand, watch which channel moves - has a live logger
    // and a live BLE link to watch. A fifth probe is reported once and never bound.
    if (channelIndex < 0) {
        if (wasUnknownRomIdReported(romId)) { return; }

        g_warning("OneWire: all four channels are bound, not binding '%s'."
            " Use --enroll --reset to start over.", romId.c_str());
        writeSessionRecord("enrollment", std::format(
            "\"action\":\"refusedStoreFull\",\"romId\":\"{}\",\"note\":\"{}\"",
            romId, "all four channels bound; --enroll --reset discards the store and starts over"));
        return;
    }

    auto position = std::find(scannedRomIds.begin(), scannedRomIds.end(), romId);
    bindChannel(channelIndex, romId, readings[position - scannedRomIds.begin()]);
}

void reportUnknownRomIds(const std::vector<std::string>& unboundRomIds) {
    for (const auto& romId : unboundRomIds) {
        if (wasUnknownRomIdReported(romId)) { continue; }

        // Rule 2. Outside enrollment this is invalid data and an event, never a new channel: a
        // probe that dropped out and came back, or a replacement fitted mid-session, must not
        // silently take over a slot.
        g_warning("OneWire: unknown probe '%s' on the bus, not bound and not logged as a channel."
            " Re-run with --enroll to bind it.", romId.c_str());
        writeSessionRecord("thermalUnknownProbe", std::format(
            "\"romId\":\"{}\",\"note\":\"{}\"",
            romId, "binding happens only under --enroll; this reading belongs to no channel"));
    }
}

// The hand-entered offset for this channel SLOT, from KnurLogger.ini, applied here and ONLY here —
// on the way to RaceChrono (owner decision, 2026-09-10). Two properties keep it recoverable, and
// both matter because the offset is a typo away from wrong:
//   1. The session record carries the raw reading and the corrected one side by side, so a bad
//      offset costs a reprocess rather than the session.
//   2. The result is clamped clear of INT16_MIN. A corrected reading that landed exactly on the
//      invalid sentinel would be indistinguishable from "no reading at all".
gint16 getCorrectedCentiC(gint channelIndex, gint16 rawCentiC) {
    auto offsetC = appConfig.tempOffsetsC[channelIndex];
    if (offsetC == 0.0) { return rawCentiC; }

    auto corrected = (glong)rawCentiC + (glong)std::llround(offsetC * 100.0);
    return (gint16)std::clamp<glong>(corrected, TEMP_CENTI_C_INVALID + 1, INT16_MAX);
}

void publishTempPacket() {
    guint8 data[CAN_DATA_SIZE];

    for (auto i = 0; i < TEMP_CHANNEL_COUNT; i++) {
        auto& channel = appData.thermal.channels[i];
        // Unbound, absent and unreadable all send the same sentinel, and that is correct: none of
        // them is a temperature. The session record is where the three are told apart.
        auto centiC = channel.isValid
            ? getCorrectedCentiC(i, channel.centiC) : (gint16)TEMP_CENTI_C_INVALID;
        data[i * 2] = (guint8)((centiC >> 8) & 0xFF);
        data[i * 2 + 1] = (guint8)(centiC & 0xFF);
    }

    updateBlePacket(&appData.bluetooth.temp, data);
}

// Layout transcribed from the ESP32 rig's 0x603 so the owner's existing RaceChrono channel
// definitions carry over unchanged. Every field big-endian; only the packet ID is little-endian.
void publishThermalStatusPacket(guint8 validMask) {
    auto readErrors = (guint16)std::min<guint32>(appData.thermal.readErrors, G_MAXUINT16);
    auto cycles = (guint16)(appData.thermal.sampleCycles & 0xFFFF);
    auto conversionMs = (guint16)std::min<guint32>(appData.thermal.lastConversionMs, G_MAXUINT16);

    guint8 data[CAN_DATA_SIZE] = {
        (guint8)std::min<guint32>(appData.thermal.enumeratedCount, 0xFF),
        validMask,
        (guint8)((readErrors >> 8) & 0xFF),
        (guint8)(readErrors & 0xFF),
        (guint8)((cycles >> 8) & 0xFF),
        (guint8)(cycles & 0xFF),
        (guint8)((conversionMs >> 8) & 0xFF),
        (guint8)(conversionMs & 0xFF),
    };

    updateBlePacket(&appData.bluetooth.thermalStatus, data);
}

void sampleProbes() {
    static guint64 nextSampleBootUs = 0;

    auto cycleStartUs = getBootTimeUs();
    if (cycleStartUs < nextSampleBootUs) { return; }
    nextSampleBootUs = cycleStartUs + (guint64)appConfig.tempIntervalMs * 1000;

    auto scan = scanOneWireBus();
    auto bulkRead = triggerBulkConversion();

    std::vector<ProbeReading> readings;
    std::vector<std::string> unboundRomIds;
    gboolean isAbandoned = FALSE;

    for (const auto& romId : scan.romIds) {
        if (appData.shutdownRequested) { isAbandoned = TRUE; }

        // The record is still emitted for an abandoned cycle, with the flag, rather than dropped.
        // Shutdown arrives at the end of the day (a service stop before the fuse is pulled; the feed
        // is constant 12 V) and the last cycle before it is the one worth having.
        if (isAbandoned) {
            ProbeReading skipped = {};
            skipped.centiC = TEMP_CENTI_C_INVALID;
            skipped.invalidReason = "cycleAbandoned";
            readings.push_back(skipped);
        }
        else {
            readings.push_back(readProbe(romId));
        }

        if (findChannelByRomId(romId) < 0) { unboundRomIds.push_back(romId); }
    }

    appData.thermal.enumeratedCount = (guint32)scan.romIds.size();
    appData.thermal.nonProbeEntries = scan.nonProbeEntries;
    reportProbeCapabilities(scan);

    if (appData.mode == ModeEnroll) { runEnrollmentStep(unboundRomIds, readings, scan.romIds); }
    else { reportUnknownRomIds(unboundRomIds); }

    guint8 validMask = 0;
    guint validChannelCount = 0;
    guint32 slowestReadMs = 0;
    std::string channelFields;

    for (auto i = 0; i < TEMP_CHANNEL_COUNT; i++) {
        auto& channel = appData.thermal.channels[i];

        // Iterating CHANNELS rather than present devices is what makes a bound-but-absent channel
        // land in the record as present-but-invalid instead of vanishing from it. A vanished
        // channel is indistinguishable from a logger that was not running.
        ProbeReading reading = {};
        reading.centiC = TEMP_CENTI_C_INVALID;
        reading.invalidReason = channel.isBound ? "absent" : "unbound";

        if (channel.isBound) {
            auto position = std::find(scan.romIds.begin(), scan.romIds.end(), channel.romId);
            if (position != scan.romIds.end()) { reading = readings[position - scan.romIds.begin()]; }
        }

        channel.isPresent = reading.isPresent;
        channel.isValid = reading.isValid;
        channel.centiC = reading.centiC;
        channel.sampleBootUs = cycleStartUs;
        slowestReadMs = std::max(slowestReadMs, reading.readMs);

        // Only a probe that answered and read badly is a read error. A bound channel whose probe
        // is absent is a different fault - a dropped lead, not a marginal bus - and inflating the
        // error counter with it would hide the bus-quality signal 0x603 byte 2 exists to carry.
        // `isReadError` rather than `isPresent && !isValid`, because a probe pulled off the bus
        // stays present in sysfs for ~100 s while answering with nothing, and that is a dropped
        // lead by any other name. See readProbe().
        if (channel.isBound && reading.isReadError) {
            channel.readErrors++;
            appData.thermal.readErrors++;
        }
        if (channel.isBound && reading.isPresent && !reading.isValid && !reading.isReadError) {
            channel.notAnswering++;
        }

        if (reading.isValid) { validMask |= (guint8)(1u << i); validChannelCount++; }

        channelFields += std::format(
            "{}{{\"ch\":{},\"name\":\"{}\",\"bound\":{},\"romId\":{},\"present\":{},\"valid\":{},"
            "\"milliC\":{},\"centiC\":{},\"sentCentiC\":{},\"crcOk\":{},\"reason\":{},"
            "\"readMs\":{},\"readErrors\":{},\"notAnswering\":{},\"offsetC\":{:.4f},"
            "\"scratchpadHex\":{},\"unparsed\":{}}}",
            i == 0 ? "" : ",", i, getChannelName(i),
            channel.isBound ? "true" : "false",
            channel.isBound ? std::format("\"{}\"", channel.romId) : "null",
            reading.isPresent ? "true" : "false",
            reading.isValid ? "true" : "false",
            reading.isPresent ? std::format("{}", reading.milliC) : "null",
            // `centiC` is the raw reading; `sentCentiC` is what went to RaceChrono, offset
            // included. Both are recorded because the offset is hand-entered: without the raw
            // value a mistyped offset would be unrecoverable, and without the sent value the SD
            // record and the phone could disagree with no way to tell which was which.
            (int)reading.centiC,
            (int)(reading.isValid
                ? getCorrectedCentiC(i, reading.centiC) : (gint16)TEMP_CENTI_C_INVALID),
            reading.isPresent ? (reading.crcOk ? "true" : "false") : "null",
            reading.invalidReason == NULL ? "null" : std::format("\"{}\"", reading.invalidReason),
            reading.readMs, channel.readErrors, channel.notAnswering, appConfig.tempOffsetsC[i],
            reading.scratchpadHex.empty()
                ? "null" : std::format("\"{}\"", escapeJson(reading.scratchpadHex)),
            reading.unparsedContent.empty()
                ? "null" : std::format("\"{}\"", escapeJson(reading.unparsedContent)));
    }

    appData.thermal.sampleCycles++;
    // What the cycle ACTUALLY paid for its temperatures. Reporting the bulk wait whenever the
    // write was accepted put a 0 on 0x603 bytes 6-7 while every probe was still converting for
    // ~800 ms - a field whose whole job is to show conversion cost, reading zero during the most
    // expensive part of the cycle. Only a conversion the kernel confirmed may be reported as one.
    appData.thermal.lastConversionMs = bulkRead.didConvert ? bulkRead.waitMs : slowestReadMs;
    appData.thermal.lastCycleMs = (guint32)((getBootTimeUs() - cycleStartUs) / 1000);

    writeSessionRecord("temp", std::format(
        "\"cycle\":{},\"busPresent\":{},\"enumerated\":{},\"nonProbeEntries\":{},"
        "\"validMask\":{},\"validChannels\":{},\"conversionMs\":{},\"cycleMs\":{},"
        "\"bulkConversion\":{},\"bulkAvailable\":{},\"bulkState\":{},"
        "\"abandoned\":{},\"unboundRomIds\":[{}],\"channels\":[{}]",
        appData.thermal.sampleCycles,
        // An EMPTY devices directory is the real failure signal, because a working bus always
        // registers its master. Zero probes with the master present is a perfectly good result.
        scan.isBusPresent ? "true" : "false",
        appData.thermal.enumeratedCount, scan.nonProbeEntries,
        validMask, validChannelCount, appData.thermal.lastConversionMs, appData.thermal.lastCycleMs,
        bulkRead.didConvert ? "true" : "false",
        bulkRead.isAvailable ? "true" : "false",
        bulkRead.state.empty() ? "null" : std::format("\"{}\"", escapeJson(bulkRead.state)),
        isAbandoned ? "true" : "false",
        getRomIdListJson(unboundRomIds), channelFields));

    publishTempPacket();
    publishThermalStatusPacket(validMask);
}

void writeThermalBaseline() {
    auto scan = scanOneWireBus();
    appData.thermal.isBulkReadAvailable = std::filesystem::exists(getBulkReadPath());

    // Commissioning item 2: the channel mapping in force for this session, per probe ROM ID, is
    // logged at boot. It is a per-session RECORD and never a channel name.
    writeSessionRecord("thermalBaseline", std::format(
        "\"mode\":\"{}\",\"configPath\":\"{}\",\"busPresent\":{},\"enumerated\":{},"
        "\"nonProbeEntries\":{},\"scannedRomIds\":[{}],\"bulkReadAvailable\":{},"
        "\"tempIntervalMs\":{},\"offsetsAppliedToBle\":true,"
        "\"bindings\":[{}],\"note\":\"{}\"",
        appData.mode == ModeEnroll ? "enroll" : "log",
        escapeJson(appConfig.configFilePath),
        scan.isBusPresent ? "true" : "false",
        (guint)scan.romIds.size(), scan.nonProbeEntries, getRomIdListJson(scan.romIds),
        appData.thermal.isBulkReadAvailable ? "true" : "false",
        appConfig.tempIntervalMs,
        getBindingsJson(),
        "channel names are positional; channel -> role is this record, not a name"));

    if (!scan.isBusPresent) {
        g_warning("OneWire: no '%s' under %s - the 1-Wire master is not registered, which is the"
            " real failure signal. Zero probes with the master present is fine.",
            ONE_WIRE_MASTER_NAME, ONE_WIRE_DEVICES_DIR);
        writeEventRecord("error", std::format(
            "1-Wire master '{}' is not registered under {}", ONE_WIRE_MASTER_NAME, ONE_WIRE_DEVICES_DIR));
    }

    if (scan.nonProbeEntries > 0) {
        g_warning("OneWire: %u non-DS18B20 entries on the bus, ignored by the family filter",
            scan.nonProbeEntries);
        writeEventRecord("warning", std::format(
            "{} non-DS18B20 entries under {}, ignored by the 28-* family filter",
            scan.nonProbeEntries, ONE_WIRE_DEVICES_DIR));
    }
}

gpointer oneWireProbesLoop(gpointer _) {
    g_message("OneWire: starting in %s mode", appData.mode == ModeEnroll ? "enroll" : "log");

    for (auto i = 0; i < TEMP_CHANNEL_COUNT; i++) {
        appData.thermal.channels[i].centiC = TEMP_CENTI_C_INVALID;
    }

    loadChannelStore();
    if (appData.mode == ModeEnroll && appData.resetRequested) { discardChannelStore(); }

    writeThermalBaseline();

    for (auto i = 0; i < TEMP_CHANNEL_COUNT; i++) {
        auto& channel = appData.thermal.channels[i];
        g_message("OneWire: %s -> %s, offset %+.2f C applied to RaceChrono",
            getChannelName(i).c_str(),
            channel.isBound ? channel.romId.c_str() : "unbound", appConfig.tempOffsetsC[i]);

        // Printed and recorded rather than refused. A relative offset between two DS18B20s is a
        // fraction of a kelvin, so anything this large is a decimal-point slip in a hand-edited
        // file - but the owner is the authority on that field, and silently ignoring a typed value
        // would be worse than honouring a visible one.
        if (std::fabs(appConfig.tempOffsetsC[i]) <= TEMP_OFFSET_IMPLAUSIBLE_C) { continue; }

        g_warning("OneWire: %s offset is %+.2f C, far larger than any DS18B20 relative offset"
            " - check KnurLogger.ini for a decimal-point slip. Applying it anyway.",
            getChannelName(i).c_str(), appConfig.tempOffsetsC[i]);
        writeEventRecord("warning", std::format(
            "{}OffsetC is {:+.2f}, implausibly large for a relative probe offset; applied anyway",
            getChannelName(i), appConfig.tempOffsetsC[i]));
    }

    if (appData.mode == ModeEnroll) {
        auto rescanSeconds = readWholeFile(std::format("{}/{}/w1_master_timeout",
            ONE_WIRE_DEVICES_DIR, ONE_WIRE_MASTER_NAME));

        g_message("OneWire: plug the probes in ONE AT A TIME, lowest channel first."
            " The bus rescans every %s s, so allow at least that long for each one to appear."
            " Enrollment keeps running after the fourth bind, so warming one probe by hand and"
            " watching one channel move confirms the map; stop it with SIGTERM.",
            rescanSeconds.has_value() ? trim(*rescanSeconds).c_str() : "w1_master_timeout");
    }

    appData.thermal.isRunning = true;

    while (!appData.shutdownRequested) {
        sampleProbes();
        g_usleep(ONE_WIRE_IDLE_SLEEP_US);
    }

    appData.thermal.isRunning = false;
    g_message("OneWire: shutting down, %u cycles, %u read errors",
        appData.thermal.sampleCycles, appData.thermal.readErrors);

    // Last act, after every closing record: the session writer keeps draining until this hits zero.
    appData.producersRunning--;

    return NULL;
}
