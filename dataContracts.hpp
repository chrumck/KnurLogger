#pragma once

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <format>

#include <glib.h>
#include <glib-unix.h>
#include <gio/gio.h>

#include "adapter.h"
#include "advertisement.h"
#include "application.h"
#include "device.h"
#include "logger.h"

namespace chr = std::chrono;

#define APP_VERSION "0.1.0"

#define CONFIG_FILE_NAME "KnurLogger.ini"
#define CHANNEL_STORE_FILE_NAME "channels.ini"
#define SESSION_FILE_EXTENSION ".ndjson"

#define CONFIG_GROUP_SYSTEM "system"
#define CONFIG_KEY_FILES_DIR "filesDir"
#define CONFIG_KEY_FSYNC_INTERVAL_MS "fsyncIntervalMs"
#define CONFIG_KEY_SUPPLY_INTERVAL_MS "supplyIntervalMs"

#define CONFIG_GROUP_THERMAL "thermal"
#define CONFIG_KEY_TEMP_INTERVAL_MS "tempIntervalMs"
#define CONFIG_KEY_SETTLING_WINDOW_SECONDS "settlingWindowSeconds"
#define CONFIG_KEY_SETTLING_MAX_DRIFT_MK_PER_MIN "settlingMaxDriftMilliKPerMin"

#define CONFIG_GROUP_BLUETOOTH "bluetooth"
#define CONFIG_KEY_BLE_DEVICE_NAME "bleDeviceName"

#define CONFIG_GROUP_SENSORS "sensors"
#define CONFIG_KEY_I2C_BUS "i2cBus"
#define CONFIG_KEY_BME280_ADDRESS "bme280Address"
#define CONFIG_KEY_MUX_ADDRESS "muxAddress"

#define CONFIG_GROUP_DEBUG "debug"
#define CONFIG_KEY_VERBOSE_MODE "verboseMode"

// The RaceChrono DIY CAN-Bus device protocol, as implemented by KnurDash and by the ESP32 rig in
// ../ndLouvers/step0b-rig/racechrono_ble_test/. Both are bench-proven against the phone, so these
// are transcribed rather than chosen.
#define BLE_SERVICE_ID "00001ff8-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_ID_MAIN "00000001-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_ID_FILTER "00000002-0000-1000-8000-00805f9b34fb"

#define RACECHRONO_DENY_ALL 0x00
#define RACECHRONO_ALLOW_ALL 0x01
#define RACECHRONO_ALLOW_SINGLE 0x02

#define BLE_NOTIFY_INTERVAL_MIN_MS 5
#define BLE_NOTIFY_INTERVAL_MAX_MS 5000
#define BLE_SHUTDOWN_POLL_MS 500

#define CAN_FRAME_ID_LENGTH 4
#define CAN_DATA_SIZE 8

// Packet IDs are kept identical to the ESP32 rig's so that the RaceChrono channel definitions the
// owner already built against it carry over to this logger unchanged.
#define PACKET_ID_TEMP 0x602
#define PACKET_ID_THERMAL_STATUS 0x603
#define PACKET_ID_SUPPLY 0x604

// Signed 0.01 C/LSB, and INT16_MIN for a channel with no trustworthy reading. Deliberately absurd
// after the divide (-327.68 C) rather than plausible: commissioning item 4 forbids presenting a
// carried-forward value as new, and RaceChrono holds the last value it received indefinitely.
#define TEMP_CENTI_C_INVALID INT16_MIN
#define TEMP_VALID_MIN_CENTI_C -5500
#define TEMP_VALID_MAX_CENTI_C 12500

// The DS18B20's power-on scratchpad default. The probe answered but never converted, which points
// at power or a marginal pull-up rather than at a hot probe, so it is flagged and not trusted.
#define TEMP_POWER_ON_DEFAULT_CENTI_C 8500

#define ONE_WIRE_DEVICES_DIR "/sys/bus/w1/devices"
#define ONE_WIRE_MASTER_NAME "w1_bus_master1"
// A DS18B20 is family 28. The bare bus invented churning `00-*` phantoms while the sensor zone was
// unbuilt and GPIO4 floated; they stopped once R11 terminated the line, but nothing may rely on
// that — matching the family code is the only thing that separates a probe from noise.
#define ONE_WIRE_DS18B20_PREFIX "28-"
#define ONE_WIRE_ROM_ID_LENGTH 15

#define TEMP_CHANNEL_COUNT 4
#define PRESSURE_CHANNEL_COUNT 6

#define HWMON_DIR "/sys/class/hwmon"
#define HWMON_SUPPLY_NAME "rpi_volt"
#define HWMON_SUPPLY_ALARM_FILE "in0_lcrit_alarm"

// vcgencmd get_throttled: bits 0-3 are live, bits 16-19 latch "has occurred since boot". The
// latching is what makes a 1 Hz sampler unable to miss a transient.
#define THROTTLED_BIT_UNDERVOLTAGE 0x1
#define THROTTLED_BIT_ARM_CAPPED 0x2
#define THROTTLED_BIT_THROTTLED 0x4
#define THROTTLED_BIT_SOFT_TEMP_LIMIT 0x8
#define THROTTLED_STICKY_SHIFT 16
#define THROTTLED_LIVE_MASK 0xF

typedef enum {
    ModeLog = 0,
    ModeEnroll = 1,
} RunMode;

typedef struct {
    gchar* filesDirPath;
    std::string sessionsDirPath;
    std::string channelStorePath;

    gint fsyncIntervalMs;
    gint supplyIntervalMs;

    gint tempIntervalMs;
    gint settlingWindowSeconds;
    gint settlingMaxDriftMilliKPerMin;

    gchar* bleDeviceName;

    gint i2cBus;
    gint bme280Address;
    gint muxAddress;

    gboolean verboseMode;
} AppConfig;

typedef struct {
    gint fd;
    GAsyncQueue* queue;
    std::string filePath;

    std::atomic<guint64> recordsWritten;
    std::atomic<guint64> recordsDropped;
    std::atomic<guint64> lastFsyncBootUs;
    std::atomic<bool> isRunning;
} SessionWriter;

typedef struct {
    std::string romId;
    gboolean isBound;

    gint16 centiC;
    gboolean isValid;
    guint64 sampleBootUs;

    guint32 readErrors;
    // Keyed to the ROM ID rather than to the slot, and present from the store's first version
    // because thermal item 1's calibration needs somewhere to put offsets and a later format
    // change is a drift liability. Recorded only — never applied to a logged reading.
    gdouble offsetC;
    guint64 boundTaiUs;
} TempChannel;

typedef struct {
    TempChannel channels[TEMP_CHANNEL_COUNT];
    std::mutex lock;

    guint32 sampleCycles;
    guint32 readErrors;
    guint32 lastConversionMs;

    std::atomic<bool> isRunning;
    std::atomic<bool> settlingSampleDone;
} ThermalData;

typedef struct {
    std::string hwmonAlarmPath;

    guint32 throttledLive;
    guint32 throttledSticky;
    // get_throttled's sticky bits latch since BOOT, not since session start, so a second session
    // on the same boot inherits the first one's bits. This baseline is what lets a reader tell an
    // inherited bit from one this session earned.
    guint32 throttledStickyAtStart;
    guint64 firstTransitionBootUs[4];

    std::atomic<bool> isRunning;
} SupplyData;

typedef struct {
    guint32 packetId;
    guint8 data[CAN_DATA_SIZE];
    guint64 updatedBootUs;
    gboolean wasSent;
    GMutex lock;
    guint notifySourceId;
} BlePacket;

typedef struct {
    GMainLoop* mainLoop;
    GDBusConnection* dbusConn;
    Adapter* adapter;
    Application* app;
    Advertisement* adv;

    BlePacket temp;
    BlePacket thermalStatus;
    BlePacket supply;

    std::atomic<bool> isNotifying;
    std::atomic<bool> isConnected;
    std::atomic<guint64> notifiesSent;
    std::atomic<bool> isRunning;
} BluetoothData;

typedef struct {
    RunMode mode;
    gboolean resetRequested;

    std::atomic<bool> shutdownRequested;

    guint64 bootTimeUs;
    guint64 sessionStartBootUs;

    // Incremented by main BEFORE a record-producing worker is spawned and decremented by that
    // worker as its last act, so the session writer can outlive every producer deterministically.
    // Counting rather than polling per-worker isRunning flags avoids the startup race where the
    // writer sees a not-yet-started worker as already finished.
    std::atomic<int> producersRunning;

    SessionWriter session;
    ThermalData thermal;
    SupplyData supply;
    BluetoothData bluetooth;
} AppData;
