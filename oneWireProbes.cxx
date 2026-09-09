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
//   4. The store carries provenance: the ROM ID, the channel and the bind timestamp. It no longer
//      carries a calibration offset — those moved to KnurLogger.ini, one per channel slot (owner
//      decision, 2026-09-10). Two offset fields, one of them dead, would be worse than one.
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
#define ONE_WIRE_BULK_POLL_US 20000
#define ONE_WIRE_BULK_MAX_WAIT_US 1500000

#define CHANNEL_STORE_GROUP_META "store"
#define CHANNEL_STORE_VERSION 1
#define CHANNEL_STORE_KEY_ROM_ID "romId"
#define CHANNEL_STORE_KEY_BOUND_TAI_US "boundTaiUs"
#define CHANNEL_STORE_KEY_BOUND_ISO "boundIso"

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

gboolean writeSysfsValue(const std::string& path, const gchar* value) {
    auto fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) { return FALSE; }

    auto length = (ssize_t)strlen(value);
    auto written = write(fd, value, length);
    close(fd);

    return written == length;
}

// Returns the milliseconds spent waiting, or nothing if the bulk path was unavailable or refused.
// A failure here is not an error: the per-probe reads that follow are correct either way, they
// just each pay for their own conversion. That is exactly why this optimisation is safe to ship
// on a box where it cannot be exercised - its worst case is the behaviour without it.
std::optional<guint32> triggerBulkConversion() {
    auto path = getBulkReadPath();
    if (!std::filesystem::exists(path)) { return std::nullopt; }
    if (!writeSysfsValue(path, "trigger")) {
        if (appConfig.verboseMode) { g_warning("OneWire: bulk read trigger refused"); }
        return std::nullopt;
    }

    auto startUs = getBootTimeUs();
    while (getBootTimeUs() - startUs < ONE_WIRE_BULK_MAX_WAIT_US) {
        if (appData.shutdownRequested) { break; }

        auto state = readWholeFile(path);
        // "-1" means a conversion is still running on at least one probe. Anything else means the
        // results are ready to be read.
        if (!state.has_value() || trim(*state) != "-1") { break; }

        g_usleep(ONE_WIRE_BULK_POLL_US);
    }

    return (guint32)((getBootTimeUs() - startUs) / 1000);
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

    if (!content.has_value()) { reading.invalidReason = "readFailed"; return reading; }

    reading.isPresent = TRUE;

    auto crcPos = content->find("crc=");
    auto valuePos = content->find("t=");
    if (crcPos == std::string::npos || valuePos == std::string::npos) {
        reading.invalidReason = "unparseable";
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
    catch (const std::exception&) { reading.invalidReason = "unparseable"; return reading; }

    if (!reading.crcOk) { reading.invalidReason = "crc"; return reading; }

    auto centiC = (reading.milliC >= 0 ? reading.milliC + 5 : reading.milliC - 5) / 10;

    // 85.00 C is the DS18B20's power-on scratchpad default: the probe answered but never
    // converted, which points at power or a marginal pull-up rather than at a hot probe. Flagged
    // with its own reason rather than lumped in with a CRC failure, because the two send whoever
    // reads the record to different parts of the board.
    if (centiC == TEMP_POWER_ON_DEFAULT_CENTI_C) {
        reading.invalidReason = "powerOnDefault";
        return reading;
    }

    if (centiC < TEMP_VALID_MIN_CENTI_C || centiC > TEMP_VALID_MAX_CENTI_C) {
        reading.invalidReason = "outOfRange";
        return reading;
    }

    reading.centiC = (gint16)centiC;
    reading.isValid = TRUE;
    reading.invalidReason = NULL;

    return reading;
}

void loadChannelStore() {
    auto* store = g_key_file_new();

    GError* error = NULL;
    if (!g_key_file_load_from_file(store, appConfig.channelStorePath.c_str(), G_KEY_FILE_NONE, &error)) {
        g_message("OneWire: no binding store at '%s' (%s), no channel is bound",
            appConfig.channelStorePath.c_str(), error->message);
        g_clear_error(&error);
        g_key_file_free(store);
        return;
    }

    for (auto i = 0; i < TEMP_CHANNEL_COUNT; i++) {
        auto group = getChannelName(i);
        auto* romId = g_key_file_get_string(store, group.c_str(), CHANNEL_STORE_KEY_ROM_ID, NULL);
        if (romId == NULL) { continue; }

        // The family filter applies to the STORE too, not only to the bus. A `00-*` phantom that
        // somehow reached the file would otherwise be honoured forever, which is the one failure
        // the filter exists to prevent, arriving by the back door.
        if (!g_str_has_prefix(romId, ONE_WIRE_DS18B20_PREFIX) || strlen(romId) != ONE_WIRE_ROM_ID_LENGTH) {
            g_warning("OneWire: store has a non-DS18B20 ROM ID '%s' on %s, ignoring it",
                romId, group.c_str());
            writeEventRecord("error", std::format(
                "binding store holds non-DS18B20 ROM ID '{}' on {}, ignored", romId, group));
            g_free(romId);
            continue;
        }

        // One probe cannot serve two channels, and a store that says otherwise would produce two
        // channels tracking each other perfectly - a mislabelling that looks like agreement.
        gboolean isDuplicate = FALSE;
        for (auto j = 0; j < i; j++) {
            if (appData.thermal.channels[j].isBound && appData.thermal.channels[j].romId == romId) {
                isDuplicate = TRUE;
            }
        }
        if (isDuplicate) {
            g_warning("OneWire: store binds ROM ID '%s' to more than one channel, ignoring %s",
                romId, group.c_str());
            writeEventRecord("error", std::format(
                "binding store binds ROM ID '{}' to more than one channel, {} ignored", romId, group));
            g_free(romId);
            continue;
        }

        auto& channel = appData.thermal.channels[i];
        channel.romId = romId;
        channel.isBound = TRUE;
        channel.boundTaiUs = g_key_file_get_uint64(store, group.c_str(), CHANNEL_STORE_KEY_BOUND_TAI_US, NULL);
        g_free(romId);
    }

    g_key_file_free(store);
}

// Written through a temporary and renamed, with the containing directory fsync'd. The store is the
// one piece of state whose loss costs a trip to the car, and the power cut it has to survive is
// ignition-off, which arrives without warning during exactly the write this function performs.
gboolean saveChannelStore() {
    auto* store = g_key_file_new();

    g_key_file_set_integer(store, CHANNEL_STORE_GROUP_META, "version", CHANNEL_STORE_VERSION);
    g_key_file_set_string(store, CHANNEL_STORE_GROUP_META, "writtenIso",
        getIsoTimestamp(getCurrentTimeUs()).c_str());
    g_key_file_set_comment(store, CHANNEL_STORE_GROUP_META, NULL,
        " KnurLogger DS18B20 channel bindings. Written by --enroll; read by every run.\n"
        " This file is machine-written and holds bindings only. CALIBRATION OFFSETS ARE NOT HERE -\n"
        " they are 'temp0OffsetC'..'temp3OffsetC' in KnurLogger.ini, one per channel slot.\n"
        " A logging run never rewrites this file; only --enroll does.\n"
        " Channel names are positional and carry no meaning; channel -> role is a per-session\n"
        " record in the session file, never a name.", NULL);

    for (auto i = 0; i < TEMP_CHANNEL_COUNT; i++) {
        auto& channel = appData.thermal.channels[i];
        if (!channel.isBound) { continue; }

        auto group = getChannelName(i);
        g_key_file_set_string(store, group.c_str(), CHANNEL_STORE_KEY_ROM_ID, channel.romId.c_str());
        g_key_file_set_uint64(store, group.c_str(), CHANNEL_STORE_KEY_BOUND_TAI_US, channel.boundTaiUs);
        // A hand-created binding carries no bind time, and stamping the epoch on it would read
        // as a broken clock in the one field whose job is provenance.
        g_key_file_set_string(store, group.c_str(), CHANNEL_STORE_KEY_BOUND_ISO,
            channel.boundTaiUs == 0 ? "unknown" : getIsoTimestamp(channel.boundTaiUs).c_str());
    }

    gsize length = 0;
    auto* data = g_key_file_to_data(store, &length, NULL);
    g_key_file_free(store);

    if (data == NULL) {
        g_critical("OneWire: could not serialise the binding store");
        return FALSE;
    }

    auto tempPath = std::format("{}.tmp", appConfig.channelStorePath);
    auto fd = open(tempPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        g_critical("OneWire: could not open '%s': %s", tempPath.c_str(), strerror(errno));
        g_free(data);
        return FALSE;
    }

    gboolean isWritten = TRUE;
    auto remaining = (size_t)length;
    auto* cursor = data;
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
    g_free(data);

    if (!isWritten) { unlink(tempPath.c_str()); return FALSE; }

    if (rename(tempPath.c_str(), appConfig.channelStorePath.c_str()) != 0) {
        g_critical("OneWire: could not rename '%s' into place: %s", tempPath.c_str(), strerror(errno));
        unlink(tempPath.c_str());
        return FALSE;
    }

    // The rename itself is metadata on the containing directory, so without this the store can be
    // durable and still not be reachable after a cut.
    auto storeDir = std::filesystem::path(appConfig.channelStorePath).parent_path().string();
    auto dirFd = open(storeDir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirFd < 0) {
        g_warning("OneWire: could not open '%s' to fsync it: %s", storeDir.c_str(), strerror(errno));
        return TRUE;
    }
    if (fsync(dirFd) != 0) {
        g_warning("OneWire: could not fsync '%s': %s", storeDir.c_str(), strerror(errno));
    }
    close(dirFd);

    return TRUE;
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
    auto bulkWaitMs = triggerBulkConversion();

    std::vector<ProbeReading> readings;
    std::vector<std::string> unboundRomIds;
    gboolean isAbandoned = FALSE;

    for (const auto& romId : scan.romIds) {
        if (appData.shutdownRequested) { isAbandoned = TRUE; }

        // The record is still emitted for an abandoned cycle, with the flag, rather than dropped.
        // Shutdown arrives at ignition-off and the last cycle before it is the one worth having.
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
        if (channel.isBound && reading.isPresent && !reading.isValid) {
            channel.readErrors++;
            appData.thermal.readErrors++;
        }

        if (reading.isValid) { validMask |= (guint8)(1u << i); validChannelCount++; }

        channelFields += std::format(
            "{}{{\"ch\":{},\"name\":\"{}\",\"bound\":{},\"romId\":{},\"present\":{},\"valid\":{},"
            "\"milliC\":{},\"centiC\":{},\"sentCentiC\":{},\"crcOk\":{},\"reason\":{},"
            "\"readMs\":{},\"readErrors\":{},\"offsetC\":{:.4f},\"scratchpadHex\":{}}}",
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
            reading.readMs, channel.readErrors, appConfig.tempOffsetsC[i],
            reading.scratchpadHex.empty()
                ? "null" : std::format("\"{}\"", escapeJson(reading.scratchpadHex)));
    }

    appData.thermal.sampleCycles++;
    appData.thermal.lastConversionMs = bulkWaitMs.has_value() ? *bulkWaitMs : slowestReadMs;
    appData.thermal.lastCycleMs = (guint32)((getBootTimeUs() - cycleStartUs) / 1000);

    writeSessionRecord("temp", std::format(
        "\"cycle\":{},\"busPresent\":{},\"enumerated\":{},\"nonProbeEntries\":{},"
        "\"validMask\":{},\"validChannels\":{},\"conversionMs\":{},\"cycleMs\":{},"
        "\"bulkConversion\":{},\"abandoned\":{},\"unboundRomIds\":[{}],\"channels\":[{}]",
        appData.thermal.sampleCycles,
        // An EMPTY devices directory is the real failure signal, because a working bus always
        // registers its master. Zero probes with the master present is a perfectly good result.
        scan.isBusPresent ? "true" : "false",
        appData.thermal.enumeratedCount, scan.nonProbeEntries,
        validMask, validChannelCount, appData.thermal.lastConversionMs, appData.thermal.lastCycleMs,
        bulkWaitMs.has_value() ? "true" : "false",
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
        "\"mode\":\"{}\",\"storePath\":\"{}\",\"busPresent\":{},\"enumerated\":{},"
        "\"nonProbeEntries\":{},\"scannedRomIds\":[{}],\"bulkReadAvailable\":{},"
        "\"tempIntervalMs\":{},\"offsetsAppliedToBle\":true,"
        "\"bindings\":[{}],\"note\":\"{}\"",
        appData.mode == ModeEnroll ? "enroll" : "log",
        escapeJson(appConfig.channelStorePath),
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
