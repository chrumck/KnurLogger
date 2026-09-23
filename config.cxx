#pragma once

#include "helpers.cxx"

#define getConfigString(_target, _group, _key)                                                     \
    appConfig._target = g_key_file_get_string(config, _group, _key, &error);                       \
    if (error != NULL) {                                                                           \
        logErrorAndKill("Error getting config: '%s', error: %s, exiting...", _key, error->message); \
    }

#define getConfigDouble(_target, _group, _key, _min, _max)                                             appConfig._target = g_key_file_get_double(config, _group, _key, &error);                            if (error != NULL) {                                                                                    logErrorAndKill("Error getting config: '%s', error: %s, exiting...", _key, error->message);      }                                                                                                   if (appConfig._target < _min || appConfig._target > _max) {                                              logErrorAndKill("Value out of range for config: %s: '%f', expected %f..%f, exiting...",                   _key, appConfig._target, (gdouble)_min, (gdouble)_max);                                      }

#define getConfigInteger(_target, _group, _key, _min, _max)                                        \
    appConfig._target = g_key_file_get_integer(config, _group, _key, &error);                      \
    if (error != NULL) {                                                                           \
        logErrorAndKill("Error getting config: '%s', error: %s, exiting...", _key, error->message); \
    }                                                                                              \
    if (appConfig._target < _min || appConfig._target > _max) {                                     \
        logErrorAndKill("Value out of range for config: %s: '%d', expected %d..%d, exiting...",     \
            _key, appConfig._target, _min, _max);                                                   \
    }

// A LIST of populated mux channels, never a count. A count says "channels 0..n-1", which is a claim
// about which channels are SAFE, and addressing an unpopulated or faulty one hangs the entire main
// bus - mux and BME280 with it - recoverable only by a ~RESET pulse on GPIO17.
//
// Channel 5 is rejected by name rather than by a range check, because "5 is out of range" would be
// a lie: it is wired, it is a legitimate mux channel, and the only thing missing is its pull-up
// pair. A reader who is told that will fit R13/R14 and change one line; one told it is out of range
// will go looking for a bug.
void loadPressureChannelsEnabled(GKeyFile* config) {
    GError* error = NULL;
    auto* raw = g_key_file_get_string(config, CONFIG_GROUP_PRESSURE,
        CONFIG_KEY_PRESSURE_CHANNELS_ENABLED, &error);
    if (error != NULL) {
        logErrorAndKill("Error getting config: '%s', error: %s, exiting...",
            CONFIG_KEY_PRESSURE_CHANNELS_ENABLED, error->message);
    }

    auto** entries = g_strsplit(g_strstrip(raw), ",", -1);

    for (auto i = 0; entries[i] != NULL; i++) {
        auto* entry = g_strstrip(entries[i]);
        // An empty list is a legitimate state - a box with no sensors fitted - but a stray comma
        // is not, because it is indistinguishable from a channel number someone meant to type.
        if (strlen(entry) == 0) {
            if (i == 0 && entries[1] == NULL) { continue; }
            logErrorAndKill("Invalid config: '%s' has an empty entry at position %d, exiting...",
                CONFIG_KEY_PRESSURE_CHANNELS_ENABLED, i);
        }

        gchar* end = NULL;
        auto channel = (gint)g_ascii_strtoll(entry, &end, 10);
        if (end == entry || *end != '\0') {
            logErrorAndKill("Invalid config: '%s' entry '%s' is not a number, exiting...",
                CONFIG_KEY_PRESSURE_CHANNELS_ENABLED, entry);
        }

        if (channel == MUX_MAX_CHANNEL) {
            logErrorAndKill("Invalid config: '%s' names mux channel %d, whose pull-ups R13/R14 are"
                " footprints only. Addressing an unpopulated channel hangs the whole main I2C bus."
                " Fit them before enabling it,"
                " exiting...", CONFIG_KEY_PRESSURE_CHANNELS_ENABLED, channel);
        }

        if (channel < 0 || channel > MUX_MAX_CHANNEL) {
            logErrorAndKill("Invalid config: '%s' entry '%s' is not a mux channel, expected 0..%d,"
                " exiting...", CONFIG_KEY_PRESSURE_CHANNELS_ENABLED, entry, MUX_MAX_CHANNEL);
        }

        appConfig.pressureChannelsEnabled[channel] = TRUE;
    }

    g_strfreev(entries);
    g_free(raw);
}

void loadConfig()
{
    GKeyFile* config = g_key_file_new();

    // Beside the binary, resolved from /proc/self/exe rather than from the working directory, so
    // the systemd unit and a hand-run from any cwd read the same file.
    auto execPath = std::filesystem::canonical("/proc/self/exe");
    appConfig.configFilePath = std::format("{}/{}", execPath.parent_path().string(), CONFIG_FILE_NAME);

    GError* error = NULL;
    if (!g_key_file_load_from_file(config, appConfig.configFilePath.c_str(), G_KEY_FILE_NONE, &error)) {
        logErrorAndKill("Error loading config file from path: '%s', error: %s, exiting...",
            appConfig.configFilePath.c_str(), error->message);
    }

    getConfigString(filesDirPath, CONFIG_GROUP_SYSTEM, CONFIG_KEY_FILES_DIR);
    if (strlen(g_strstrip(appConfig.filesDirPath)) == 0) {
        logErrorAndKill("Invalid config: '%s' must not be empty, exiting...", CONFIG_KEY_FILES_DIR);
    }

    std::string filesDir = appConfig.filesDirPath;
    while (filesDir.size() > 1 && filesDir.back() == '/') { filesDir.pop_back(); }
    appConfig.sessionsDirPath = std::format("{}/sessions", filesDir);

    // The cadence is ~1 s by design: flushing per sample at 10 Hz shortens the loss window at the
    // price of write amplification without changing the failure mode. The bounds here allow
    // experiment, not a different design.
    getConfigInteger(fsyncIntervalMs, CONFIG_GROUP_SYSTEM, CONFIG_KEY_FSYNC_INTERVAL_MS, 100, 10000);
    getConfigInteger(supplyIntervalMs, CONFIG_GROUP_SYSTEM, CONFIG_KEY_SUPPLY_INTERVAL_MS, 200, 60000);

    // Around 1 Hz. The DS18B20 needs up to 750 ms at 12 bits, so anything much faster leaves no
    // idle bus time.
    getConfigInteger(tempIntervalMs, CONFIG_GROUP_THERMAL, CONFIG_KEY_TEMP_INTERVAL_MS, 500, 60000);

    // One calibration offset per channel SLOT, applied to the value sent to RaceChrono. Absent
    // keys are an error rather than a silent zero: an offset that quietly stopped being applied
    // because a key was mistyped would be invisible in the data it corrupts.
    for (auto i = 0; i < TEMP_CHANNEL_COUNT; i++) {
        auto key = std::format(CONFIG_KEY_TEMP_OFFSET_C_FORMAT, i);
        getConfigDouble(tempOffsetsC[i], CONFIG_GROUP_THERMAL, key.c_str(),
            -TEMP_OFFSET_MAX_C, TEMP_OFFSET_MAX_C);
    }

    getConfigString(bleDeviceName, CONFIG_GROUP_BLUETOOTH, CONFIG_KEY_BLE_DEVICE_NAME);
    if (strlen(g_strstrip(appConfig.bleDeviceName)) == 0) {
        logErrorAndKill("Invalid config: '%s' must not be empty, exiting...", CONFIG_KEY_BLE_DEVICE_NAME);
    }

    // Bus 1 is the perfboard. /dev/i2c-20 and /dev/i2c-21 also exist and are the VC4 display DDC
    // buses, nothing to do with this board.
    getConfigInteger(i2cBus, CONFIG_GROUP_SENSORS, CONFIG_KEY_I2C_BUS, 0, 20);
    // 0x77 as built, not the 0x76 originally specified: the breakout's own SDO pull-up wins, and
    // the specification was amended rather than the board.
    getConfigInteger(bme280Address, CONFIG_GROUP_SENSORS, CONFIG_KEY_BME280_ADDRESS, 0x08, 0x77);
    // Cavity conditions move slowly and the part is the cavity thermometer, so the cadence is
    // set by what the record needs rather than by what the sensor can do. 1 Hz matches the other
    // 1 Hz channels; a forced conversion costs ~9 ms, so this is nowhere near a limit.
    getConfigInteger(bme280IntervalMs, CONFIG_GROUP_SENSORS, CONFIG_KEY_BME280_INTERVAL_MS, 200, 60000);
    getConfigInteger(muxAddress, CONFIG_GROUP_SENSORS, CONFIG_KEY_MUX_ADDRESS, 0x08, 0x77);

    // The target is 10 Hz. A five-sensor cycle costs 15.4 ms measured, so the constraint is
    // session-file bytes and the fsync cadence rather than CPU; the bounds allow experiment, not a
    // different design.
    getConfigInteger(pressureIntervalMs, CONFIG_GROUP_PRESSURE, CONFIG_KEY_PRESSURE_INTERVAL_MS,
        50, 60000);
    loadPressureChannelsEnabled(config);

    appConfig.verboseMode = g_key_file_get_boolean(config, CONFIG_GROUP_DEBUG, CONFIG_KEY_VERBOSE_MODE, &error);
    if (error != NULL) {
        logErrorAndKill("Error getting config: '%s', error: %s, exiting...",
            CONFIG_KEY_VERBOSE_MODE, error->message);
    }

    g_key_file_free(config);
}
