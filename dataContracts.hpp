#pragma once

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <algorithm>
#include <chrono>
#include <cmath>
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
#include "characteristic.h"
#include "device.h"
#include "logger.h"

namespace chr = std::chrono;

#define APP_VERSION "0.1.0"

#define CONFIG_FILE_NAME "KnurLogger.ini"
#define SESSION_FILE_EXTENSION ".ndjson"

#define CONFIG_GROUP_SYSTEM "system"
#define CONFIG_KEY_FILES_DIR "filesDir"
#define CONFIG_KEY_FSYNC_INTERVAL_MS "fsyncIntervalMs"
#define CONFIG_KEY_SUPPLY_INTERVAL_MS "supplyIntervalMs"

#define CONFIG_GROUP_THERMAL "thermal"
#define CONFIG_KEY_TEMP_INTERVAL_MS "tempIntervalMs"
// One per channel: temp0OffsetC .. temp3OffsetC. Keyed to the SLOT, not to the probe's ROM ID
// (owner decision, 2026-09-10) — see the note on TEMP_OFFSET_IMPLAUSIBLE_C.
#define CONFIG_KEY_TEMP_OFFSET_C_FORMAT "temp{}OffsetC"
// Machine-written by --enroll, hand-readable afterwards. `BoundIso` is provenance for a human and
// is never read back; `BoundTaiUs` is what the logger reads.
#define CONFIG_KEY_TEMP_ROM_ID_FORMAT "temp{}RomId"
#define CONFIG_KEY_TEMP_BOUND_TAI_US_FORMAT "temp{}BoundTaiUs"
#define CONFIG_KEY_TEMP_BOUND_ISO_FORMAT "temp{}BoundIso"

#define CONFIG_GROUP_BLUETOOTH "bluetooth"
#define CONFIG_KEY_BLE_DEVICE_NAME "bleDeviceName"

#define CONFIG_GROUP_SENSORS "sensors"
#define CONFIG_KEY_I2C_BUS "i2cBus"
#define CONFIG_KEY_BME280_ADDRESS "bme280Address"
#define CONFIG_KEY_BME280_INTERVAL_MS "bme280IntervalMs"
#define CONFIG_KEY_MUX_ADDRESS "muxAddress"

#define CONFIG_GROUP_PRESSURE "pressure"
#define CONFIG_KEY_PRESSURE_INTERVAL_MS "pressureIntervalMs"
// A LIST of populated mux channels, never a count. A count would say "channels 0..n-1", which is
// a claim about which channels are safe to address - and addressing an unpopulated one hangs the
// whole main bus (see the mux prohibition below). The list is what the sampling loop iterates, so
// a channel that is not in it can never be selected by accident.
#define CONFIG_KEY_PRESSURE_CHANNELS_ENABLED "pressureChannelsEnabled"

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
// owner already built against it carry over to this logger unchanged. 0x600 and 0x601 are the
// exception: on the rig they carried synthetic ramp/triangle test frames, and the owner released
// them for real use (2026-09-10) since that rig is spent. Anyone still holding the rig's 0x600
// channel definitions must re-enter them - the bytes mean something else here.
#define PACKET_ID_ENCLOSURE 0x600
#define PACKET_ID_ENCLOSURE_STATUS 0x601
#define PACKET_ID_TEMP 0x602
#define PACKET_ID_THERMAL_STATUS 0x603
#define PACKET_ID_SUPPLY 0x604
// The three pressure frames, contiguous with the block above (owner decision, 2026-09-19, recorded
// in ../ndLouvers/CFD-Learning-Plan.md Step 0b commissioning item 4a - "commissioning" matters,
// since that plan also has a thermal item 4a and an open item 4a). Checked clear against the
// committed RaceChrono/vehicleProfile.json: across both boxes the only IDs claimed are 0x78,
// 0x202, 0x420, 0x4FA, 0x600-0x604 and 0x7F0.
#define PACKET_ID_PRESSURE_A 0x605
#define PACKET_ID_PRESSURE_B 0x606
#define PACKET_ID_PRESSURE_STATUS 0x607

// Signed 0.01 C/LSB, and INT16_MIN for a channel with no trustworthy reading. Deliberately absurd
// after the divide (-327.68 C) rather than plausible: commissioning item 4 forbids presenting a
// carried-forward value as new, and RaceChrono holds the last value it received indefinitely.
#define TEMP_CENTI_C_INVALID INT16_MIN
#define TEMP_VALID_MIN_CENTI_C -5500
#define TEMP_VALID_MAX_CENTI_C 12500

// Calibration offsets are hand-entered in KnurLogger.ini, one per CHANNEL SLOT, and applied to the
// value sent to RaceChrono (owner decision, 2026-09-10). Slot-keyed rather than ROM-ID-keyed is a
// deliberate simplification with one consequence worth stating: an offset is a property of a
// particular DS18B20, so **re-enrolling the probes in a different order, or swapping a probe,
// leaves the offsets pointing at the wrong parts and they must be re-checked.** In exchange the
// calibration lives in the git-tracked config beside every other setting, rather than in the
// data directory where a wipe would take it.
//
// A relative offset between two DS18B20s is a fraction of a kelvin, so anything past this is a
// decimal-point slip. Warned about and applied anyway; the hard bound below only rejects nonsense,
// because a logger that refuses to start in the car over a typo loses the whole session.
#define TEMP_OFFSET_IMPLAUSIBLE_C 5.0
#define TEMP_OFFSET_MAX_C 50.0

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

// --- BME280, on the main I2C bus behind no mux -------------------------------------------------
//
// Registers and the compensation algorithm are transcribed from the Bosch BME280 datasheet
// (rev 1.6), not derived. The three quantities have three different consumers and only one of
// them is a measurement channel, so they are named for their roles rather than for the part.

#define BME280_CHIP_ID 0x60
#define BME280_REG_CALIBRATION_1 0x88
#define BME280_REG_CALIBRATION_1_LENGTH 26
#define BME280_REG_ID 0xD0
#define BME280_REG_RESET 0xE0
#define BME280_REG_CALIBRATION_2 0xE1
#define BME280_REG_CALIBRATION_2_LENGTH 7
#define BME280_REG_CTRL_HUM 0xF2
#define BME280_REG_STATUS 0xF3
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG 0xF5
#define BME280_REG_DATA 0xF7
#define BME280_REG_DATA_LENGTH 8

#define BME280_RESET_WORD 0xB6
#define BME280_STATUS_MEASURING 0x08
// Set while the part is copying its calibration out of NVM after a reset. Reading the
// coefficients through it yields a half-copied block, which is the failure that produces
// plausible-looking wrong numbers rather than an obvious one.
#define BME280_STATUS_IM_UPDATE 0x01
#define BME280_RESET_SETTLE_US 5000

// Oversampling x1 on all three quantities, IIR filter off, and forced mode: one conversion per
// sample, the part asleep in between. That is the datasheet's lowest-self-heating setting and the
// choice is a MEASUREMENT one rather than a power one - this part is the cavity thermometer
// (plan item 1c), so any heat it makes is an error in the quantity it exists to report. Higher
// oversampling would buy pressure noise this channel has no use for: it is a density term and is
// disqualified as a reference, so 3.3 Pa RMS against ~101 kPa is already far past sufficient.
#define BME280_OVERSAMPLING_X1 0x01
#define BME280_MODE_FORCED 0x01
#define BME280_CTRL_HUM_VALUE BME280_OVERSAMPLING_X1
#define BME280_CTRL_MEAS_VALUE     ((BME280_OVERSAMPLING_X1 << 5) | (BME280_OVERSAMPLING_X1 << 2) | BME280_MODE_FORCED)
#define BME280_CONFIG_VALUE 0x00

// A conversion at x1/x1/x1 takes 9.3 ms worst case. The margin is for a bus retry, not for a
// slower conversion, and a part that has not finished by then is reported as invalid rather than
// waited on indefinitely.
#define BME280_MEASUREMENT_TIMEOUT_US 100000
#define BME280_MEASUREMENT_POLL_US 2000

// The reset value of the raw ADC registers, and what the part returns for a quantity whose
// oversampling is set to `skipped`. It is NOT a reading of zero and must never be compensated:
// t_fine derived from it would silently corrupt pressure and humidity too.
#define BME280_RAW_SKIPPED_20BIT 0x80000
#define BME280_RAW_SKIPPED_16BIT 0x8000

// The part's own operating range, per the datasheet. Anything outside it is a transport fault or
// a corrupt calibration block, not weather.
#define BME280_TEMP_VALID_MIN_CENTI_C -4000
#define BME280_TEMP_VALID_MAX_CENTI_C 8500
#define BME280_PRESSURE_VALID_MIN_PA 30000
#define BME280_PRESSURE_VALID_MAX_PA 110000
#define BME280_HUMIDITY_MAX_CENTI_PCT 10000

// Invalid sentinels for the two unsigned BLE fields. Both are absurd after the divide - 4294967
// kPa and 655.35 %RH - for the same reason the thermal channels send -327.68 C: RaceChrono holds
// the last value it received indefinitely, so an invalid marker has to be unmistakable rather
// than plausible. Temperature reuses TEMP_CENTI_C_INVALID, so one decode rule covers every
// temperature channel this logger publishes.
#define BME280_PRESSURE_PA_INVALID 0xFFFFFFFF
#define BME280_HUMIDITY_CENTI_PCT_INVALID 0xFFFF

// Bits of 0x601 byte 0.
#define BME280_STATUS_BIT_PRESENT 0x01
#define BME280_STATUS_BIT_CALIBRATED 0x02
#define BME280_STATUS_BIT_PRESSURE_VALID 0x04
#define BME280_STATUS_BIT_TEMPERATURE_VALID 0x08
#define BME280_STATUS_BIT_HUMIDITY_VALID 0x10

// --- The five SDP810s, one per PCA9548A channel ------------------------------------------------
//
// Commands, frame layout and the CRC are transcribed from the Sensirion SDP8xx digital datasheet
// and were confirmed against all five real parts on the bench, 2026-09-19
// (../ndLouvers/pressure-testing.md 3.1).
//
// ALL FIVE ANSWER AT THE SAME FIXED ADDRESS AND CANNOT BE STRAPPED APART, which is why the mux is
// mandatory rather than a convenience, and why every sample costs a channel select first.
#define SDP810_ADDRESS 0x25

// Averaged, temperature-compensated differential pressure. ISSUED ONCE PER SENSOR, NEVER PER
// SAMPLE: the part NAKs it when it is already in continuous mode, and selecting a different mux
// channel does NOT take it out of that mode. A bring-up harness that re-armed it every cycle lost
// 145 of 150 transfers and looked exactly like a failing bus.
#define SDP810_CMD_START_CONTINUOUS 0x3615
#define SDP810_CMD_STOP_CONTINUOUS 0x3FF9
#define SDP810_CMD_READ_PRODUCT_ID_1 0x367C
#define SDP810_CMD_READ_PRODUCT_ID_2 0xE102

// Expected values for a WARNING, never substitutes for what the part returns. The scale factor
// arrives in every measurement frame and is retained per sensor; the +-125 Pa turning up on mux
// channel 2 rather than the specified channel 4 was caught ONLY because the part was asked what it
// was instead of being read off the board. Identify an SDP810 by its product number.
#define SDP810_PRODUCT_500PA 0x03020A01
#define SDP810_PRODUCT_125PA 0x03020B01
#define SDP810_SCALE_500PA 60
#define SDP810_SCALE_125PA 240

// A reading outside the part's own range is invalid data rather than a clipped value
// (commissioning item 4). Ranges are the datasheet's, selected by the product number read back at
// boot; a part whose product number is unrecognised gets the wider bound, since refusing every
// sample from an unexpected-but-working sensor would be worse than logging it with a warning.
#define SDP810_RANGE_500PA_PA 500.0
#define SDP810_RANGE_125PA_PA 125.0

#define SDP810_MEASUREMENT_LENGTH 9
#define SDP810_IDENTITY_LENGTH 18
#define SDP810_CRC_POLYNOMIAL 0x31
#define SDP810_CRC_INIT 0xFF
#define SDP810_TEMPERATURE_DIVISOR 200

// The datasheet's delay before the first measurement is available after 0x3615. 20 ms is ample and
// is what bring-up used; it is paid once per sensor at start, never in the sampling loop.
#define SDP810_START_SETTLE_US 20000

// Signed 0.1 Pa/LSB - decipascals - on all six channels, with INT16_MIN for a channel with no
// trustworthy reading (owner decision, 2026-09-19; ../ndLouvers/CFD-Learning-Plan.md Step 0b item
// 4a). 0.01 Pa/LSB overflows at 327 Pa and so cannot carry a +-500 Pa channel, which would mean a
// SECOND decode rule on a hand-typed channel list this code cannot check - the fault class that
// hid bytesToUint on Temperature Front 2 for days. The resolution given up on the +-125 Pa part is
// affordable precisely because the session file carries raw counts and the returned scale factor
// at full resolution, so every past session can be reprocessed if this is ever revisited.
//
// -327.68 Pa is deliberately absurd rather than plausible, and the argument is STRONGER here than
// for temperature: zero is a completely believable differential pressure, so a channel that has
// never been read must not look like one reading nothing.
#define PRESSURE_DECI_PA_INVALID INT16_MIN
#define PRESSURE_DECI_PA_PER_PA 10.0

// Writing 0x00 to the PCA9548A's control register connects no channel at all. The bus is left this
// way at the end of every cycle and on shutdown, so no downstream segment is ever bridged onto the
// main bus while nothing is reading it.
#define MUX_CHANNEL_NONE 0x00
#define MUX_MAX_CHANNEL 5

// NEVER ADDRESS A CHANNEL THAT IS NOT IN THE CONFIGURED ENABLED LIST. An unpopulated or faulty
// downstream channel hangs the ENTIRE main bus - mux and BME280 with it - and only a ~RESET pulse
// on GPIO17 recovers it. Both bus lines read idle-high and i2cdetect still lists every device
// while it is happening, so every cheap check says the bus is fine. CLAUDE.md has the diagnosis
// and the two occurrences; Hardware/logger-perfboard-wiring.md 5 has which channels are populated.
// Today channel 5 is the unpopulated one: its pull-ups are footprints only.
//
// This is why the config key is a LIST and why selectMuxChannel refuses anything outside it. A
// sweep must be impossible to write by accident.

#define I2C_BUS_PATH_FORMAT "/dev/i2c-{}"

// THE FIRST TRANSFER AFTER AN IDLE BUS ALWAYS FAILS ON THIS BOARD, AND A RETRY ALWAYS FIXES IT.
// Measured 2026-09-10 against the BME280 at 0x77: with an idle gap of 10 ms or more the first
// I2C_RDWR is refused every single time, and a second attempt 500 us later succeeded 60 times out
// of 60 across gaps of 50, 200 and 1000 ms. Back to back at 2 ms the first attempt mostly works.
// So this is a property of the bus rather than error recovery, and the retry below is what makes
// a 1 Hz sampler work at all - at 1 Hz EVERY cycle starts with an idle bus.
//
// It is retried rather than worked around, and COUNTED rather than swallowed: the expected
// pattern is about one recovered transfer per sample cycle, so a count far above that is a bus
// that has genuinely degraded, and the counter is the only thing that would show it.
//
// TEN ATTEMPTS RATHER THAN FOUR, AND THAT WAS MEASURED, NOT GUESSED. A retry chain long enough
// for the isolated chip-ID read is not long enough for a whole sample cycle: a cycle is eight
// transfers, and at four attempts 2 of 64 cycles still lost every channel (0.4 % of transfers
// exhausted). At ten, 64 of 64 and then 300 of 300 over a five-minute run came back clean with
// nothing exhausted at all. The attempts cost
// nothing when they are not needed - measured cycle read time was 15 ms either way - because the
// delay is only paid on a failure.
#define I2C_TRANSFER_MAX_ATTEMPTS 10
#define I2C_TRANSFER_RETRY_US 500

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
    // Beside the binary, resolved from /proc/self/exe. It carries the bindings as well as the
    // offsets, so --enroll writes back into it.
    std::string configFilePath;

    gint fsyncIntervalMs;
    gint supplyIntervalMs;

    gint tempIntervalMs;
    // Indexed by channel, so tempOffsetsC[1] belongs to temp1 whichever probe is bound there.
    gdouble tempOffsetsC[TEMP_CHANNEL_COUNT];

    gchar* bleDeviceName;

    gint i2cBus;
    gint bme280Address;
    gint bme280IntervalMs;
    gint muxAddress;

    gint pressureIntervalMs;
    // Indexed by mux channel, TRUE only for a channel the config named. Held as flags rather than
    // as the parsed list because every consumer asks "may I select channel n" rather than "what is
    // the n-th enabled channel", and an index-into-a-list is one off-by-one away from selecting a
    // channel nobody enabled.
    gboolean pressureChannelsEnabled[PRESSURE_CHANNEL_COUNT];

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

    // Bound but absent is a THIRD state, distinct from unbound and from reading badly, and it has
    // to survive into the record: a channel whose ROM ID stops appearing on the bus is logged as
    // present-but-invalid, never omitted. Omitting it would make a mid-session dropout
    // indistinguishable from the logger not having run.
    gboolean isPresent;

    gint16 centiC;
    gboolean isValid;
    guint64 sampleBootUs;

    guint32 readErrors;
    // Counted apart from readErrors on purpose: a probe pulled off the bus keeps its sysfs entry
    // for up to ~100 s and reads back with nothing in it, and folding that into the bus-quality
    // counter would bury the signal 0x603 byte 2 exists to carry. See readProbe().
    guint32 notAnswering;
    guint64 boundTaiUs;
} TempChannel;

// One probe read, as parsed out of the w1_therm `w1_slave` attribute. Transient rather than state,
// but it lands verbatim in the session record, so it is a data contract.
typedef struct {
    gboolean isPresent;
    gboolean crcOk;
    gboolean isValid;
    gint16 centiC;
    gint32 milliC;
    // The nine scratchpad bytes as the kernel printed them, kept because commissioning item 4 asks
    // for the raw reading beside the corrected one and this is the rawest form available.
    std::string scratchpadHex;
    // Whatever the kernel returned when it could not be parsed, truncated. Without it a marginal
    // bus and a lead pulled out of its socket leave identical evidence.
    std::string unparsedContent;
    const gchar* invalidReason;
    // Set only by the failures that say something about the BUS. A dropped lead is not one of
    // them, whether it shows up as a missing sysfs entry or as an entry that answers with nothing.
    gboolean isReadError;
    guint32 readMs;
} ProbeReading;

// No lock: every field here is written and read by the oneWireProbes worker alone. The only
// cross-thread boundary the thermal data crosses is the BLE packet, which carries its own mutex.
// An unused mutex parked here would imply this state is shared, and the next reader would trust it.
typedef struct {
    TempChannel channels[TEMP_CHANNEL_COUNT];

    guint32 sampleCycles;
    guint32 readErrors;
    guint32 lastConversionMs;
    guint32 lastCycleMs;
    guint32 enumeratedCount;
    // Entries under the devices directory that were neither the bus master nor a `28-*` probe.
    // This is the family filter's own output: a non-zero count is the churning-phantom trap
    // happening again, and it is the only way anyone will see that the filter did something.
    guint32 nonProbeEntries;
    gboolean isBulkReadAvailable;
    // Rate limiter: the trigger is refused on every cycle once it is refused at all, and 863 lines
    // an hour of the same warning is not evidence, it is noise.
    gboolean isBulkTriggerRefusalReported;
    // Separate from the refusal: the write can be ACCEPTED and still convert nothing, which is a
    // different fault with a different fix and needs its own one-shot report.
    gboolean isBulkNoOpReported;
    gboolean isProbeInfoReported;

    // Rate limiters, so a permanent condition produces one record rather than one per second.
    std::vector<std::string> reportedUnknownRomIds;
    std::string lastAmbiguousRomIds;

    std::atomic<bool> isRunning;
} ThermalData;

// The BME280's factory calibration, read once from the part's NVM. It is logged verbatim at boot
// because commissioning item 2 asks for identity and calibration data: without these coefficients
// the raw ADC counts mean nothing, so they are as much a part of a reading's provenance as the
// chip ID, and a corrupted block is the failure that produces plausible-looking wrong numbers.
typedef struct {
    guint16 digT1;
    gint16 digT2, digT3;
    guint16 digP1;
    gint16 digP2, digP3, digP4, digP5, digP6, digP7, digP8, digP9;
    guint8 digH1;
    gint16 digH2;
    guint8 digH3;
    gint16 digH4, digH5;
    gint8 digH6;
} Bme280Calibration;

// One conversion, as read back and compensated. Transient rather than state, but it lands
// verbatim in the session record, so it is a data contract.
typedef struct {
    gboolean isPresent;
    // Three separate flags because the three quantities fail together only sometimes. A raw
    // temperature the part reports as skipped invalidates all three, since t_fine feeds the
    // pressure and humidity compensation; a pressure outside the part's range invalidates only
    // pressure. Carrying one forward on the strength of another would be exactly what
    // commissioning item 4 forbids.
    gboolean isPressureValid;
    gboolean isTemperatureValid;
    gboolean isHumidityValid;

    gint32 rawPressure;
    gint32 rawTemperature;
    gint32 rawHumidity;
    // The datasheet's shared intermediate: temperature-derived, and the reason a bad temperature
    // reading poisons the other two. Recorded so a wrong pressure can be traced to its cause.
    gint32 tFine;

    gdouble pressurePa;
    gint16 temperatureCentiC;
    guint16 humidityCentiPct;

    const gchar* invalidReason;
    // Set only by failures that say something about the BUS or the part, so that a quantity
    // rejected for being out of range is not confused with a transfer that did not complete.
    gboolean isReadError;
    guint32 readMs;
} Bme280Reading;

// No lock, for the same reason ThermalData has none: every field is written and read by the
// bme280Sensor worker alone, and the only cross-thread boundary the data crosses is the BLE
// packet, which carries its own mutex.
// Transport-level, and deliberately not part of any one device's state: the mux and the five
// SDP810s will share this bus and this counter, and a bus that has degraded degrades for all of
// them at once.
typedef struct {
    std::atomic<guint64> firstAttemptFailures;
    std::atomic<guint64> recoveredTransfers;
    std::atomic<guint64> exhaustedTransfers;
} I2cBusData;

typedef struct {
    gint fd;
    gboolean isPresent;
    gboolean isCalibrationLoaded;
    guint8 chipId;
    Bme280Calibration calibration;

    guint32 sampleCycles;
    guint32 readErrors;
    guint32 lastReadMs;

    Bme280Reading lastReading;

    // Rate limiters, so a permanent condition produces one record rather than one per second.
    // The 1-Wire worker learned this the expensive way: 863 identical warnings in 14 minutes at
    // the car buried the two messages the run existed to produce.
    gboolean isBusFailureReported;
    gboolean isChipIdMismatchReported;

    std::atomic<bool> isRunning;
} Bme280Data;

// One SDP810 measurement frame, as read back and converted. Transient rather than state, but it
// lands verbatim in the session record, so it is a data contract.
typedef struct {
    gboolean isPresent;
    gboolean isValid;

    gint16 rawDifferential;
    // AS RETURNED BY THIS PART IN THIS FRAME, never the nominal 60. The +-125 Pa returns 240, and
    // hard-coding either is how a channel silently reads four times the pressure it should.
    guint16 scaleFactor;
    gint16 rawTemperature;

    // Full resolution, for the session file. pressureDeciPa is the lossy form that goes on the air.
    gdouble pressurePa;
    gint16 pressureDeciPa;
    gint16 temperatureCentiC;

    // All three words of the frame carry their own CRC and all three are checked. A CRC failure is
    // INVALID DATA, never a carried-forward value.
    gboolean crcOk;
    const gchar* invalidReason;
    // Set only by failures that say something about the BUS, so a reading rejected for being out
    // of range is not confused with a transfer that did not complete. 0x607 keeps the two counts
    // apart for the same reason: the thermal side learned the expensive way that one counter
    // conflating "the bus retried" with "a sample was not trusted" hides both.
    gboolean isReadError;
    guint32 readMs;
} SdpReading;

typedef struct {
    gint muxChannel;
    gboolean isEnabled;
    gboolean isPresent;

    // Read once at boot and logged with the baseline: this is the per-session channel -> part
    // provenance commissioning item 2 asks for, and the only place a later analysis can learn
    // which physical sensor produced a channel.
    guint32 productNumber;
    guint64 serial;
    guint16 expectedScale;

    // Tracked per sensor because 0x3615 is NAK'd when the part is already continuous, and a mux
    // channel change does not end that mode. Cleared when a read fails, so a sensor that browned
    // out is re-armed on the next visit and one that did not is left alone.
    gboolean isContinuousStarted;

    guint32 readErrors;
    guint32 crcFailures;
    SdpReading lastReading;
} PressureChannel;

// No lock, for the same reason ThermalData and Bme280Data have none: every field is written and
// read by the pressureSensors worker alone, and the only cross-thread boundary the data crosses is
// the BLE packet, which carries its own mutex.
typedef struct {
    PressureChannel channels[PRESSURE_CHANNEL_COUNT];

    gint fd;
    guint32 sampleCycles;
    guint32 readErrors;
    guint32 crcFailures;
    guint32 lastCycleMs;
    guint enabledCount;
    // Published on 0x607 byte 7 as a cheap liveness tell. Set to MUX_CHANNEL_NONE at the end of
    // every cycle, so a value that sticks at a channel number is a cycle that never finished.
    gint lastSelectedChannel;

    // Rate limiters, so a permanent condition produces one record rather than one per cycle. At
    // 10 Hz that is ten times the noise the 1 Hz workers could make, and the 1-Wire worker already
    // buried two important messages under 863 identical warnings at 1 Hz.
    gboolean isBusFailureReported;
    gboolean isChannelRefusalReported;

    std::atomic<bool> isRunning;
} PressureData;

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
    // For logs and records only. A notify count per packet is what separates "the logger is not
    // sending" from "the logger is sending and the phone is not showing it", and a bare packet ID
    // in a log line is one more thing to decode under time pressure at the car.
    const gchar* name;
    guint8 data[CAN_DATA_SIZE];
    guint64 updatedBootUs;
    gboolean wasSent;
    GMutex lock;
    guint notifySourceId;
    std::atomic<guint32> notifiesSent;
} BlePacket;

typedef struct {
    GMainLoop* mainLoop;
    GDBusConnection* dbusConn;
    Adapter* adapter;
    Application* app;
    Advertisement* adv;

    BlePacket enclosure;
    BlePacket enclosureStatus;
    BlePacket temp;
    BlePacket thermalStatus;
    BlePacket supply;
    BlePacket pressureA;
    BlePacket pressureB;
    BlePacket pressureStatus;

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
    I2cBusData i2c;
    Bme280Data bme280;
    PressureData pressure;
    SupplyData supply;
    BluetoothData bluetooth;
} AppData;
