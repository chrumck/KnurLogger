#pragma once

#include "blePackets.cxx"

#define SUPPLY_MONITOR_IDLE_SLEEP_US 50000

static const gchar* const throttledBitNames[] = {
    "undervoltage", "armFrequencyCapped", "throttled", "softTemperatureLimit",
};

// The hwmon index is NOT stable across boots, so this resolves the device by reading each hwmon's
// `name` file rather than by hardcoding hwmon1.
std::string findSupplyAlarmPath() {
    auto* dir = opendir(HWMON_DIR);
    if (dir == NULL) {
        g_warning("SupplyMonitor: cannot open %s: %s", HWMON_DIR, strerror(errno));
        return "";
    }

    std::string found;
    while (auto* entry = readdir(dir)) {
        if (strncmp(entry->d_name, "hwmon", 5) != 0) { continue; }

        auto namePath = std::format("{}/{}/name", HWMON_DIR, entry->d_name);
        auto name = readWholeFile(namePath);
        if (!name.has_value() || trim(*name) != HWMON_SUPPLY_NAME) { continue; }

        auto alarmPath = std::format("{}/{}/{}", HWMON_DIR, entry->d_name, HWMON_SUPPLY_ALARM_FILE);
        if (std::filesystem::exists(alarmPath)) { found = alarmPath; }
        break;
    }

    closedir(dir);

    if (found.empty()) {
        g_warning("SupplyMonitor: no '%s' hwmon device with '%s' found",
            HWMON_SUPPLY_NAME, HWMON_SUPPLY_ALARM_FILE);
    }
    else {
        g_message("SupplyMonitor: undervoltage comparator at '%s'", found.c_str());
    }

    return found;
}

std::optional<guint32> readThrottled() {
    static const gchar* const argv[] = { "vcgencmd", "get_throttled", NULL };

    auto output = runCommand(argv);
    if (!output.has_value()) { return std::nullopt; }

    // "throttled=0x0"
    auto separator = output->find('=');
    if (separator == std::string::npos) { return std::nullopt; }

    try { return (guint32)std::stoul(output->substr(separator + 1), nullptr, 0); }
    catch (const std::exception&) { return std::nullopt; }
}

// NOT the supply rail. This is the regulated SoC core voltage, ~0.906 V, and it says nothing about
// the 5 V input. The field name carries `socCore` precisely so it cannot be read as a rail
// measurement — the rail is build sheet §10 step 2's meter at TP2 and nothing here substitutes.
std::optional<gdouble> readSocCoreVolts() {
    static const gchar* const argv[] = { "vcgencmd", "measure_volts", "core", NULL };

    auto output = runCommand(argv);
    if (!output.has_value()) { return std::nullopt; }

    auto separator = output->find('=');
    if (separator == std::string::npos) { return std::nullopt; }

    try { return std::stod(output->substr(separator + 1)); }
    catch (const std::exception&) { return std::nullopt; }
}

std::optional<gdouble> readSocTempC() {
    auto raw = readWholeFile(std::format("{}/hwmon0/temp1_input", HWMON_DIR));
    if (!raw.has_value()) {
        raw = readWholeFile("/sys/class/thermal/thermal_zone0/temp");
        if (!raw.has_value()) { return std::nullopt; }
    }

    try { return std::stod(trim(*raw)) / 1000.0; }
    catch (const std::exception&) { return std::nullopt; }
}

std::optional<gint> readUndervoltageAlarm() {
    if (appData.supply.hwmonAlarmPath.empty()) { return std::nullopt; }

    auto raw = readWholeFile(appData.supply.hwmonAlarmPath);
    if (!raw.has_value()) { return std::nullopt; }

    try { return std::stoi(trim(*raw)); }
    catch (const std::exception&) { return std::nullopt; }
}

// A sticky bit that is set the moment this session starts was earned by an EARLIER session on the
// same boot, because the latch is since-boot. Reporting the first transition against the
// session-start baseline is what separates "it happened during this run" from "it happened before
// it", and the plan's whole argument for sampled telemetry rests on that distinction.
void recordStickyTransitions(guint32 sticky) {
    auto newlySet = sticky & ~appData.supply.throttledSticky;
    if (newlySet == 0) { return; }

    auto nowBootUs = getBootTimeUs();

    for (guint bit = 0; bit < getLength(throttledBitNames); bit++) {
        if ((newlySet & (1u << bit)) == 0) { continue; }

        appData.supply.firstTransitionBootUs[bit] = nowBootUs;

        auto isInherited = (appData.supply.throttledStickyAtStart & (1u << bit)) != 0;
        writeSessionRecord("supplyTransition", std::format(
            "\"bit\":\"{}\",\"inheritedFromEarlierSession\":{},\"stickyWord\":{}",
            throttledBitNames[bit], isInherited ? "true" : "false", sticky));

        g_warning("SupplyMonitor: %s latched (sticky word 0x%X)", throttledBitNames[bit], sticky);
    }
}

// Every field big-endian, matching the ESP32 rig's 0x604 sibling layout conventions. Named fields
// are documented in the README so the RaceChrono channel definitions can be written by hand.
void publishSupplyPacket() {
    auto socMillivolts = (guint16)0;
    auto socCentiC = (gint16)0;

    auto volts = readSocCoreVolts();
    if (volts.has_value()) { socMillivolts = (guint16)(*volts * 1000.0); }

    auto tempC = readSocTempC();
    if (tempC.has_value()) { socCentiC = (gint16)(*tempC * 100.0); }

    auto alarm = readUndervoltageAlarm();

    guint8 data[CAN_DATA_SIZE] = {
        (guint8)(appData.supply.throttledLive & 0xFF),
        (guint8)(appData.supply.throttledSticky & 0xFF),
        // 0xFF rather than 0 when the comparator could not be read, so "no reading" is not
        // presented as "no alarm".
        (guint8)(alarm.has_value() ? *alarm : 0xFF),
        (guint8)((socMillivolts >> 8) & 0xFF),
        (guint8)(socMillivolts & 0xFF),
        (guint8)((socCentiC >> 8) & 0xFF),
        (guint8)(socCentiC & 0xFF),
        (guint8)(appData.session.recordsDropped > 0 ? 1 : 0),
    };

    updateBlePacket(&appData.bluetooth.supply, data);
}

void sampleSupply() {
    static guint64 nextSampleBootUs = 0;

    auto nowBootUs = getBootTimeUs();
    if (nowBootUs < nextSampleBootUs) { return; }
    nextSampleBootUs = nowBootUs + (guint64)appConfig.supplyIntervalMs * 1000;

    auto throttled = readThrottled();
    auto socCoreVolts = readSocCoreVolts();
    auto socTempC = readSocTempC();
    auto alarm = readUndervoltageAlarm();

    std::string throttledFields = "\"throttledRead\":false";
    if (throttled.has_value()) {
        auto live = *throttled & THROTTLED_LIVE_MASK;
        auto sticky = (*throttled >> THROTTLED_STICKY_SHIFT) & THROTTLED_LIVE_MASK;

        recordStickyTransitions(sticky);
        appData.supply.throttledLive = live;
        appData.supply.throttledSticky = sticky;

        throttledFields = std::format(
            "\"throttledRead\":true,\"throttledWord\":{},\"live\":{},\"sticky\":{},"
            "\"stickyAtSessionStart\":{},"
            "\"liveUndervoltage\":{},\"stickyUndervoltage\":{}",
            *throttled, live, sticky, appData.supply.throttledStickyAtStart,
            (live & THROTTLED_BIT_UNDERVOLTAGE) ? "true" : "false",
            (sticky & THROTTLED_BIT_UNDERVOLTAGE) ? "true" : "false");
    }

    writeSessionRecord("supply", std::format(
        "{},\"lcritAlarm\":{},\"socCoreVolts\":{},\"socTempC\":{}",
        throttledFields,
        // Live only, no sticky history — that asymmetry with get_throttled is why both are logged.
        alarm.has_value() ? std::format("{}", *alarm) : "null",
        socCoreVolts.has_value() ? std::format("{:.4f}", *socCoreVolts) : "null",
        socTempC.has_value() ? std::format("{:.1f}", *socTempC) : "null"));

    publishSupplyPacket();
}

gpointer supplyMonitorLoop(gpointer _) {
    g_message("SupplyMonitor: starting");

    appData.supply.hwmonAlarmPath = findSupplyAlarmPath();

    auto atStart = readThrottled();
    if (atStart.has_value()) {
        appData.supply.throttledStickyAtStart = (*atStart >> THROTTLED_STICKY_SHIFT) & THROTTLED_LIVE_MASK;
        appData.supply.throttledSticky = appData.supply.throttledStickyAtStart;
        appData.supply.throttledLive = *atStart & THROTTLED_LIVE_MASK;
    }
    else {
        g_warning("SupplyMonitor: vcgencmd get_throttled unavailable at start");
    }

    writeSessionRecord("supplyBaseline", std::format(
        "\"throttledRead\":{},\"stickyAtSessionStart\":{},\"hwmonAlarmPath\":\"{}\","
        "\"note\":\"{}\"",
        atStart.has_value() ? "true" : "false",
        appData.supply.throttledStickyAtStart,
        escapeJson(appData.supply.hwmonAlarmPath),
        "sticky bits latch since BOOT; any bit set here was earned before this session started"));

    appData.supply.isRunning = true;

    while (!appData.shutdownRequested) {
        sampleSupply();
        g_usleep(SUPPLY_MONITOR_IDLE_SLEEP_US);
    }

    appData.supply.isRunning = false;
    g_message("SupplyMonitor: shutting down");

    // Last act, and it must come after any closing record this worker writes: the session writer
    // keeps draining until this hits zero.
    appData.producersRunning--;

    return NULL;
}
