#pragma once

#include "helpers.cxx"

#define getConfigString(_target, _group, _key)                                                     \
    appConfig._target = g_key_file_get_string(config, _group, _key, &error);                       \
    if (error != NULL) {                                                                           \
        logErrorAndKill("Error getting config: '%s', error: %s, exiting...", _key, error->message); \
    }

#define getConfigInteger(_target, _group, _key, _min, _max)                                        \
    appConfig._target = g_key_file_get_integer(config, _group, _key, &error);                      \
    if (error != NULL) {                                                                           \
        logErrorAndKill("Error getting config: '%s', error: %s, exiting...", _key, error->message); \
    }                                                                                              \
    if (appConfig._target < _min || appConfig._target > _max) {                                     \
        logErrorAndKill("Value out of range for config: %s: '%d', expected %d..%d, exiting...",     \
            _key, appConfig._target, _min, _max);                                                   \
    }

void loadConfig()
{
    GKeyFile* config = g_key_file_new();

    // Beside the binary, resolved from /proc/self/exe rather than from the working directory, so
    // the systemd unit and a hand-run from any cwd read the same file.
    auto execPath = std::filesystem::canonical("/proc/self/exe");
    auto configPath = std::format("{}/{}", execPath.parent_path().string(), CONFIG_FILE_NAME);

    GError* error = NULL;
    if (!g_key_file_load_from_file(config, configPath.c_str(), G_KEY_FILE_NONE, &error)) {
        logErrorAndKill("Error loading config file from path: '%s', error: %s, exiting...",
            configPath.c_str(), error->message);
    }

    getConfigString(filesDirPath, CONFIG_GROUP_SYSTEM, CONFIG_KEY_FILES_DIR);
    if (strlen(g_strstrip(appConfig.filesDirPath)) == 0) {
        logErrorAndKill("Invalid config: '%s' must not be empty, exiting...", CONFIG_KEY_FILES_DIR);
    }

    std::string filesDir = appConfig.filesDirPath;
    while (filesDir.size() > 1 && filesDir.back() == '/') { filesDir.pop_back(); }
    appConfig.sessionsDirPath = std::format("{}/sessions", filesDir);
    // The binding store lives in the data directory, not beside the binary: it is field state that
    // must outlive a rebuild, and build/ is where a rebuild lands.
    appConfig.channelStorePath = std::format("{}/{}", filesDir, CHANNEL_STORE_FILE_NAME);

    // The plan fixes the cadence at ~1 s and gives the reasoning: flushing per sample at 10 Hz
    // shortens the loss window at the price of write amplification without changing the failure
    // mode. The bounds here allow experiment, not a different design.
    getConfigInteger(fsyncIntervalMs, CONFIG_GROUP_SYSTEM, CONFIG_KEY_FSYNC_INTERVAL_MS, 100, 10000);
    getConfigInteger(supplyIntervalMs, CONFIG_GROUP_SYSTEM, CONFIG_KEY_SUPPLY_INTERVAL_MS, 200, 60000);

    // Thermal item 2: start around 1 Hz. The DS18B20 needs up to 750 ms at 12 bits, so anything
    // much faster leaves no idle bus time.
    getConfigInteger(tempIntervalMs, CONFIG_GROUP_THERMAL, CONFIG_KEY_TEMP_INTERVAL_MS, 500, 60000);

    // "Before anything warms up" is not observable from a single instant, so the session-start
    // common-temperature sample is a window and settledness is judged from each probe's drift rate
    // across it. The threshold is a measurement decision the plan does not yet give a number for;
    // this default is deliberately conservative and the raw drift rates are recorded regardless,
    // so a later owner figure can be applied in post-processing without re-running anything.
    getConfigInteger(settlingWindowSeconds, CONFIG_GROUP_THERMAL, CONFIG_KEY_SETTLING_WINDOW_SECONDS, 10, 900);
    getConfigInteger(settlingMaxDriftMilliKPerMin, CONFIG_GROUP_THERMAL,
        CONFIG_KEY_SETTLING_MAX_DRIFT_MK_PER_MIN, 1, 60000);

    getConfigString(bleDeviceName, CONFIG_GROUP_BLUETOOTH, CONFIG_KEY_BLE_DEVICE_NAME);
    if (strlen(g_strstrip(appConfig.bleDeviceName)) == 0) {
        logErrorAndKill("Invalid config: '%s' must not be empty, exiting...", CONFIG_KEY_BLE_DEVICE_NAME);
    }

    // Bus 1 is the perfboard. /dev/i2c-20 and /dev/i2c-21 also exist and are the VC4 display DDC
    // buses, nothing to do with this board.
    getConfigInteger(i2cBus, CONFIG_GROUP_SENSORS, CONFIG_KEY_I2C_BUS, 0, 20);
    // 0x77 as built, not the 0x76 the build sheet originally specified: the breakout's own SDO
    // pull-up wins and the owner amended the document rather than the board.
    getConfigInteger(bme280Address, CONFIG_GROUP_SENSORS, CONFIG_KEY_BME280_ADDRESS, 0x08, 0x77);
    getConfigInteger(muxAddress, CONFIG_GROUP_SENSORS, CONFIG_KEY_MUX_ADDRESS, 0x08, 0x77);

    appConfig.verboseMode = g_key_file_get_boolean(config, CONFIG_GROUP_DEBUG, CONFIG_KEY_VERBOSE_MODE, &error);
    if (error != NULL) {
        logErrorAndKill("Error getting config: '%s', error: %s, exiting...",
            CONFIG_KEY_VERBOSE_MODE, error->message);
    }

    g_key_file_free(config);
}
