#pragma once

#include "blePackets.cxx"
#include "i2cBus.cxx"

// The BME280 on the main I2C bus at 0x77, behind no mux. It owns packet 0x600 and packet 0x601.
//
// It is the only sensor on this board that is fitted, answering and independent of the five
// undelivered SDP810s, and it is here for a commissioning reason rather than a feature one. Its
// three quantities have three different consumers and it is worth being explicit about all three,
// because two of them are easy to point at the wrong thing:
//
//   1. TEMPERATURE IS THE CAVITY THERMOMETER (plan item 1c), and Pi SoC temperature is a
//      cross-check against it rather than the primary proxy. Item 1d wants cavity temperature
//      recorded across a full session before the installed pre/post envelope is trusted, which a
//      thermals drive now answers for free.
//      **It is NOT the inlet density term.** That is T_ambient's DS18B20 - a probe in the air
//      the car drives through, not one sealed in a box bolted behind a wheel.
//   2. PRESSURE IS ENCLOSURE PRESSURE AND NEVER A STATIC REFERENCE. The cavity is
//      aerodynamically live: at Cp -1 the offset is ~464 Pa against 45-90 Pa measurands, five to
//      ten times the signal, and it is speed-correlated so it will not average out of a speed
//      sweep. Tolerable as a density term, disqualifying as a reference. The field names carry
//      the role so the value cannot be picked up as a zero.
//   3. HUMIDITY IS A SEAL AND DESICCANT DIAGNOSTIC for the condensation risks in plan items 1a
//      and 1d. It is discarded by the dry-air approximation and has no measurement consumer.
//
// Everything below the transport is transcribed from the Bosch BME280 datasheet (rev 1.6) rather
// than derived: the register map, the fixed-point compensation, and the constraint that ctrl_hum
// takes effect only when ctrl_meas is written afterwards.
//
// Three rules follow from commissioning item 4, and they are why this is longer than a driver:
// a quantity the part could not measure is reported as invalid rather than compensated; a
// temperature the part reports as skipped invalidates pressure and humidity too, because t_fine
// feeds both; and no value is ever carried forward from a cycle that failed.

#define BME280_IDLE_SLEEP_US 50000

gint16 readInt16Le(const guint8* buffer) { return (gint16)(buffer[0] | (buffer[1] << 8)); }
guint16 readUint16Le(const guint8* buffer) { return (guint16)(buffer[0] | (buffer[1] << 8)); }

std::string getBme280CalibrationJson(const Bme280Calibration& c) {
    return std::format(
        "{{\"digT1\":{},\"digT2\":{},\"digT3\":{},"
        "\"digP1\":{},\"digP2\":{},\"digP3\":{},\"digP4\":{},\"digP5\":{},"
        "\"digP6\":{},\"digP7\":{},\"digP8\":{},\"digP9\":{},"
        "\"digH1\":{},\"digH2\":{},\"digH3\":{},\"digH4\":{},\"digH5\":{},\"digH6\":{}}}",
        c.digT1, c.digT2, c.digT3,
        c.digP1, c.digP2, c.digP3, c.digP4, c.digP5, c.digP6, c.digP7, c.digP8, c.digP9,
        c.digH1, c.digH2, c.digH3, c.digH4, c.digH5, c.digH6);
}

// The coefficients live in two non-contiguous blocks, and dig_H4/dig_H5 straddle one byte between
// them as two 12-bit signed values sharing 0xE5's nibbles. Getting that split wrong produces
// humidity that is wrong by a plausible amount, which is why it is spelled out rather than packed
// into a loop.
gboolean readBme280Calibration(Bme280Calibration* out) {
    guint8 block1[BME280_REG_CALIBRATION_1_LENGTH];
    guint8 block2[BME280_REG_CALIBRATION_2_LENGTH];

    if (!readI2cRegisters(appData.bme280.fd, (guint8)appConfig.bme280Address,
        BME280_REG_CALIBRATION_1, block1, getLength(block1))) {
        return FALSE;
    }
    if (!readI2cRegisters(appData.bme280.fd, (guint8)appConfig.bme280Address,
        BME280_REG_CALIBRATION_2, block2, getLength(block2))) {
        return FALSE;
    }

    out->digT1 = readUint16Le(&block1[0]);
    out->digT2 = readInt16Le(&block1[2]);
    out->digT3 = readInt16Le(&block1[4]);
    out->digP1 = readUint16Le(&block1[6]);
    out->digP2 = readInt16Le(&block1[8]);
    out->digP3 = readInt16Le(&block1[10]);
    out->digP4 = readInt16Le(&block1[12]);
    out->digP5 = readInt16Le(&block1[14]);
    out->digP6 = readInt16Le(&block1[16]);
    out->digP7 = readInt16Le(&block1[18]);
    out->digP8 = readInt16Le(&block1[20]);
    out->digP9 = readInt16Le(&block1[22]);
    out->digH1 = block1[25];

    out->digH2 = readInt16Le(&block2[0]);
    out->digH3 = block2[2];
    out->digH4 = (gint16)((gint16)((gint8)block2[3]) * 16 | (block2[4] & 0x0F));
    out->digH5 = (gint16)((gint16)((gint8)block2[5]) * 16 | (block2[4] >> 4));
    out->digH6 = (gint8)block2[6];

    // dig_T1 and dig_P1 are unsigned and specified non-zero on every part, so a block of all
    // zeroes or all ones is a transfer that returned nothing rather than a calibration. Without
    // this check a dead bus yields temperatures that are merely wrong instead of absent.
    if (out->digT1 == 0 || out->digP1 == 0
        || (out->digT1 == 0xFFFF && out->digP1 == 0xFFFF)) {
        g_warning("BME280: calibration block reads as %s - treating the part as uncalibrated",
            out->digT1 == 0 ? "all zeroes" : "all ones");
        return FALSE;
    }

    return TRUE;
}

// Returns centi-C and yields t_fine, which the pressure and humidity compensation both need. The
// coupling is the reason a bad temperature reading invalidates all three quantities.
gint32 compensateTemperature(const Bme280Calibration& c, gint32 rawTemperature, gint32* outTFine) {
    gint32 var1 = ((((rawTemperature >> 3) - ((gint32)c.digT1 << 1))) * ((gint32)c.digT2)) >> 11;
    gint32 var2 = (((((rawTemperature >> 4) - ((gint32)c.digT1))
        * ((rawTemperature >> 4) - ((gint32)c.digT1))) >> 12) * ((gint32)c.digT3)) >> 14;

    *outTFine = var1 + var2;
    return (*outTFine * 5 + 128) >> 8;
}

// Returns pressure in Q24.8 pascals, or 0 if the calibration would divide by zero.
guint32 compensatePressure(const Bme280Calibration& c, gint32 rawPressure, gint32 tFine) {
    gint64 var1 = ((gint64)tFine) - 128000;
    gint64 var2 = var1 * var1 * (gint64)c.digP6;
    var2 = var2 + ((var1 * (gint64)c.digP5) << 17);
    var2 = var2 + (((gint64)c.digP4) << 35);
    var1 = ((var1 * var1 * (gint64)c.digP3) >> 8) + ((var1 * (gint64)c.digP2) << 12);
    var1 = (((((gint64)1) << 47) + var1)) * ((gint64)c.digP1) >> 33;

    if (var1 == 0) { return 0; }

    gint64 pressure = 1048576 - rawPressure;
    pressure = (((pressure << 31) - var2) * 3125) / var1;
    var1 = (((gint64)c.digP9) * (pressure >> 13) * (pressure >> 13)) >> 25;
    var2 = (((gint64)c.digP8) * pressure) >> 19;
    pressure = ((pressure + var1 + var2) >> 8) + (((gint64)c.digP7) << 4);

    return (guint32)pressure;
}

// Returns relative humidity in Q22.10 percent, clamped by the datasheet's own algorithm to 0-100.
guint32 compensateHumidity(const Bme280Calibration& c, gint32 rawHumidity, gint32 tFine) {
    gint32 value = tFine - ((gint32)76800);

    value = (((((rawHumidity << 14) - (((gint32)c.digH4) << 20) - (((gint32)c.digH5) * value))
        + ((gint32)16384)) >> 15) * (((((((value * ((gint32)c.digH6)) >> 10)
        * (((value * ((gint32)c.digH3)) >> 11) + ((gint32)32768))) >> 10)
        + ((gint32)2097152)) * ((gint32)c.digH2) + 8192) >> 14));
    value = value - ((((( value >> 15) * (value >> 15)) >> 7) * ((gint32)c.digH1)) >> 4);
    value = value < 0 ? 0 : value;
    value = value > 419430400 ? 419430400 : value;

    return (guint32)(value >> 12);
}

void closeBme280Bus() {
    if (appData.bme280.fd < 0) { return; }
    close(appData.bme280.fd);
    appData.bme280.fd = -1;
}

// Idempotent, and re-run whenever the part stops answering. That matters on this box for a reason
// that is not obvious: a cranking dip can brown the part out without taking the Pi down with it,
// and a BME280 that has reset comes back in sleep mode with ctrl_hum cleared - which reports
// humidity as `skipped` rather than as an error. Re-initialising on recovery is what stops that
// becoming a silently missing channel for the rest of the day.
gboolean initialiseBme280() {
    if (appData.bme280.fd < 0) { appData.bme280.fd = openI2cBus(appConfig.i2cBus); }
    if (appData.bme280.fd < 0) { return FALSE; }

    auto address = (guint8)appConfig.bme280Address;

    guint8 chipId = 0;
    if (!readI2cRegisters(appData.bme280.fd, address, BME280_REG_ID, &chipId, 1)) {
        appData.bme280.isPresent = FALSE;
        return FALSE;
    }

    appData.bme280.chipId = chipId;

    // 0x60 is the BME280 signature; 0x58 is a BMP280, which has no humidity and different
    // calibration. Answering at the right address is not the same as being the right part, and
    // the whole reason 0x77 rather than 0x76 is the specified address here is that the board did
    // something the build sheet did not expect.
    if (chipId != BME280_CHIP_ID) {
        appData.bme280.isPresent = FALSE;
        if (!appData.bme280.isChipIdMismatchReported) {
            appData.bme280.isChipIdMismatchReported = TRUE;
            g_warning("BME280: chip ID at 0x%02X is 0x%02X, expected 0x%02X (0x58 would be a"
                " BMP280) - not reading it", address, chipId, BME280_CHIP_ID);
            writeEventRecord("error", std::format(
                "chip ID at I2C 0x{:02X} is 0x{:02X}, expected 0x{:02X}",
                address, chipId, BME280_CHIP_ID));
        }
        return FALSE;
    }

    if (!writeI2cRegister(appData.bme280.fd, address, BME280_REG_RESET, BME280_RESET_WORD)) {
        appData.bme280.isPresent = FALSE;
        return FALSE;
    }

    // The part copies its calibration out of NVM after a reset and flags that in status bit 0.
    // Reading the coefficients through it yields a half-copied block.
    for (auto waitedUs = 0; waitedUs < (gint)BME280_MEASUREMENT_TIMEOUT_US;
        waitedUs += BME280_RESET_SETTLE_US) {
        g_usleep(BME280_RESET_SETTLE_US);

        guint8 status = 0;
        if (!readI2cRegisters(appData.bme280.fd, address, BME280_REG_STATUS, &status, 1)) {
            appData.bme280.isPresent = FALSE;
            return FALSE;
        }
        if ((status & BME280_STATUS_IM_UPDATE) == 0) { break; }
    }

    appData.bme280.isCalibrationLoaded = readBme280Calibration(&appData.bme280.calibration);

    if (!writeI2cRegister(appData.bme280.fd, address, BME280_REG_CONFIG, BME280_CONFIG_VALUE)) {
        appData.bme280.isPresent = FALSE;
        return FALSE;
    }

    appData.bme280.isPresent = TRUE;
    appData.bme280.isBusFailureReported = FALSE;
    appData.bme280.isChipIdMismatchReported = FALSE;

    return appData.bme280.isCalibrationLoaded;
}

// One forced-mode conversion: configure, trigger, wait for the part to go back to sleep, burst
// read all eight data registers. The burst is one transfer on purpose - reading the registers
// individually can straddle the part's shadow-register update and mix bytes from two conversions.
Bme280Reading readBme280() {
    Bme280Reading reading = {};
    reading.invalidReason = "notAnswering";

    auto address = (guint8)appConfig.bme280Address;
    auto startUs = getBootTimeUs();

    if (appData.bme280.fd < 0 || !appData.bme280.isPresent) {
        reading.invalidReason = "deviceUnavailable";
        return reading;
    }

    if (!appData.bme280.isCalibrationLoaded) {
        reading.isPresent = TRUE;
        reading.invalidReason = "calibrationUnread";
        return reading;
    }

    // ctrl_hum first, then ctrl_meas: the datasheet is explicit that a change to ctrl_hum takes
    // effect only when ctrl_meas is written afterwards, and writing them the other way round
    // leaves humidity reporting `skipped` while temperature and pressure look perfectly healthy.
    // Both are rewritten every cycle so that a part which reset itself recovers on its own.
    if (!writeI2cRegister(appData.bme280.fd, address, BME280_REG_CTRL_HUM, BME280_CTRL_HUM_VALUE)
        || !writeI2cRegister(appData.bme280.fd, address, BME280_REG_CTRL_MEAS,
            BME280_CTRL_MEAS_VALUE)) {
        reading.isReadError = TRUE;
        reading.readMs = (guint32)((getBootTimeUs() - startUs) / 1000);
        return reading;
    }

    gboolean isMeasurementDone = FALSE;
    for (guint64 waitedUs = 0; waitedUs < BME280_MEASUREMENT_TIMEOUT_US;
        waitedUs += BME280_MEASUREMENT_POLL_US) {
        g_usleep(BME280_MEASUREMENT_POLL_US);

        guint8 status = 0;
        if (!readI2cRegisters(appData.bme280.fd, address, BME280_REG_STATUS, &status, 1)) {
            reading.isReadError = TRUE;
            reading.readMs = (guint32)((getBootTimeUs() - startUs) / 1000);
            return reading;
        }
        if ((status & BME280_STATUS_MEASURING) == 0) { isMeasurementDone = TRUE; break; }
    }

    reading.isPresent = TRUE;

    if (!isMeasurementDone) {
        reading.invalidReason = "measurementTimeout";
        reading.isReadError = TRUE;
        reading.readMs = (guint32)((getBootTimeUs() - startUs) / 1000);
        return reading;
    }

    guint8 data[BME280_REG_DATA_LENGTH];
    if (!readI2cRegisters(appData.bme280.fd, address, BME280_REG_DATA, data, getLength(data))) {
        reading.invalidReason = "notAnswering";
        reading.isReadError = TRUE;
        reading.readMs = (guint32)((getBootTimeUs() - startUs) / 1000);
        return reading;
    }

    reading.readMs = (guint32)((getBootTimeUs() - startUs) / 1000);

    reading.rawPressure = (gint32)(((guint32)data[0] << 12) | ((guint32)data[1] << 4) | (data[2] >> 4));
    reading.rawTemperature = (gint32)(((guint32)data[3] << 12) | ((guint32)data[4] << 4) | (data[5] >> 4));
    reading.rawHumidity = (gint32)(((guint32)data[6] << 8) | data[7]);

    // The reset value, and what the part returns for a quantity it did not measure. Compensating
    // it produces about -145 C and a pressure to match, which is a plausible-looking number for a
    // reading that does not exist - so temperature is checked first and its failure takes the
    // other two with it, t_fine feeding both.
    if (reading.rawTemperature == BME280_RAW_SKIPPED_20BIT) {
        reading.invalidReason = "measurementSkipped";
        return reading;
    }

    auto temperatureCentiC = compensateTemperature(
        appData.bme280.calibration, reading.rawTemperature, &reading.tFine);

    if (temperatureCentiC < BME280_TEMP_VALID_MIN_CENTI_C
        || temperatureCentiC > BME280_TEMP_VALID_MAX_CENTI_C) {
        reading.invalidReason = "temperatureOutOfRange";
        return reading;
    }

    reading.temperatureCentiC = (gint16)temperatureCentiC;
    reading.isTemperatureValid = TRUE;

    if (reading.rawPressure != BME280_RAW_SKIPPED_20BIT) {
        auto pressureQ248 = compensatePressure(
            appData.bme280.calibration, reading.rawPressure, reading.tFine);
        reading.pressurePa = pressureQ248 / 256.0;

        reading.isPressureValid = pressureQ248 != 0
            && reading.pressurePa >= BME280_PRESSURE_VALID_MIN_PA
            && reading.pressurePa <= BME280_PRESSURE_VALID_MAX_PA;
    }

    if (reading.rawHumidity != BME280_RAW_SKIPPED_16BIT) {
        auto humidityQ2210 = compensateHumidity(
            appData.bme280.calibration, reading.rawHumidity, reading.tFine);
        auto centiPct = (humidityQ2210 * 100 + 512) / 1024;

        reading.humidityCentiPct = (guint16)std::min<guint32>(centiPct, BME280_HUMIDITY_MAX_CENTI_PCT);
        reading.isHumidityValid = centiPct <= BME280_HUMIDITY_MAX_CENTI_PCT;
    }

    // Only a quantity the part refused or a value outside its own operating range gets here with
    // a reason; a fully good cycle carries none.
    if (!reading.isPressureValid || !reading.isHumidityValid) {
        reading.invalidReason = !reading.isPressureValid ? "pressureOutOfRange" : "humidityOutOfRange";
    }
    else { reading.invalidReason = NULL; }

    return reading;
}

// 0x600. Field roles rather than part names, because the pressure field is the one that must not
// be mistaken for a static reference. Every field big-endian; only the packet ID is little-endian.
void publishEnclosurePacket(const Bme280Reading& reading) {
    auto pressurePa = reading.isPressureValid
        ? (guint32)std::llround(reading.pressurePa) : (guint32)BME280_PRESSURE_PA_INVALID;
    auto temperatureCentiC = reading.isTemperatureValid
        ? reading.temperatureCentiC : (gint16)TEMP_CENTI_C_INVALID;
    auto humidityCentiPct = reading.isHumidityValid
        ? reading.humidityCentiPct : (guint16)BME280_HUMIDITY_CENTI_PCT_INVALID;

    guint8 data[CAN_DATA_SIZE] = {
        (guint8)((pressurePa >> 24) & 0xFF),
        (guint8)((pressurePa >> 16) & 0xFF),
        (guint8)((pressurePa >> 8) & 0xFF),
        (guint8)(pressurePa & 0xFF),
        (guint8)((temperatureCentiC >> 8) & 0xFF),
        (guint8)(temperatureCentiC & 0xFF),
        (guint8)((humidityCentiPct >> 8) & 0xFF),
        (guint8)(humidityCentiPct & 0xFF),
    };

    updateBlePacket(&appData.bluetooth.enclosure, data);
}

// 0x601, and it is 0x603's argument applied to this part: three plausible numbers on 0x600 say
// nothing about whether they are being READ. That matters more here than for the thermal
// channels, because a sealed cavity's temperature and pressure legitimately sit still for
// minutes, so a frozen 0x600 is not by itself evidence of anything. Byte 4-5 advances every cycle
// regardless of what the part reports, which is what separates a dead worker from a still cavity.
void publishEnclosureStatusPacket(const Bme280Reading& reading) {
    guint8 statusByte = 0;
    if (appData.bme280.isPresent) { statusByte |= BME280_STATUS_BIT_PRESENT; }
    if (appData.bme280.isCalibrationLoaded) { statusByte |= BME280_STATUS_BIT_CALIBRATED; }
    if (reading.isPressureValid) { statusByte |= BME280_STATUS_BIT_PRESSURE_VALID; }
    if (reading.isTemperatureValid) { statusByte |= BME280_STATUS_BIT_TEMPERATURE_VALID; }
    if (reading.isHumidityValid) { statusByte |= BME280_STATUS_BIT_HUMIDITY_VALID; }

    auto readErrors = (guint16)std::min<guint32>(appData.bme280.readErrors, G_MAXUINT16);
    auto cycles = (guint16)(appData.bme280.sampleCycles & 0xFFFF);
    auto readMs = (guint16)std::min<guint32>(appData.bme280.lastReadMs, G_MAXUINT16);

    guint8 data[CAN_DATA_SIZE] = {
        statusByte,
        appData.bme280.chipId,
        (guint8)((readErrors >> 8) & 0xFF),
        (guint8)(readErrors & 0xFF),
        (guint8)((cycles >> 8) & 0xFF),
        (guint8)(cycles & 0xFF),
        (guint8)((readMs >> 8) & 0xFF),
        (guint8)(readMs & 0xFF),
    };

    updateBlePacket(&appData.bluetooth.enclosureStatus, data);
}

void sampleBme280() {
    static guint64 nextSampleBootUs = 0;

    auto nowBootUs = getBootTimeUs();
    if (nowBootUs < nextSampleBootUs) { return; }
    nextSampleBootUs = nowBootUs + (guint64)appConfig.bme280IntervalMs * 1000;

    // Cheap enough to attempt every cycle, and the alternative is worse: a part that browned out
    // during cranking would otherwise stay dark for the rest of a twelve-hour day.
    if (!appData.bme280.isPresent || !appData.bme280.isCalibrationLoaded) {
        if (!initialiseBme280() && !appData.bme280.isBusFailureReported) {
            appData.bme280.isBusFailureReported = TRUE;
            g_warning("BME280: not answering at I2C 0x%02X on bus %d; retrying every cycle,"
                " reporting invalid until it does", appConfig.bme280Address, appConfig.i2cBus);
            writeEventRecord("error", std::format(
                "BME280 not answering at I2C 0x{:02X} on bus {}",
                appConfig.bme280Address, appConfig.i2cBus));
        }
    }

    auto reading = readBme280();

    appData.bme280.sampleCycles++;
    appData.bme280.lastReading = reading;
    appData.bme280.lastReadMs = reading.readMs;
    if (reading.isReadError) { appData.bme280.readErrors++; }

    // A read that fails after the part had been answering means it has gone away or reset. Drop
    // the bus so the next cycle re-opens and re-initialises it rather than reading a part whose
    // configuration is no longer what this worker thinks it is.
    if (reading.isReadError) {
        appData.bme280.isPresent = FALSE;
        appData.bme280.isCalibrationLoaded = FALSE;
        closeBme280Bus();
    }

    writeSessionRecord("enclosure", std::format(
        "\"cycle\":{},\"present\":{},\"chipId\":{},\"calibrated\":{},"
        "\"enclosurePressurePa\":{},\"cavityTemperatureC\":{},\"enclosureHumidityPct\":{},"
        "\"pressureValid\":{},\"temperatureValid\":{},\"humidityValid\":{},"
        "\"rawPressure\":{},\"rawTemperature\":{},\"rawHumidity\":{},\"tFine\":{},"
        "\"reason\":{},\"readMs\":{},\"readErrors\":{},"
        "\"i2cFirstAttemptFailures\":{},\"i2cRecovered\":{},\"i2cExhausted\":{}",
        appData.bme280.sampleCycles,
        reading.isPresent ? "true" : "false",
        appData.bme280.chipId,
        appData.bme280.isCalibrationLoaded ? "true" : "false",
        // Null rather than a sentinel in the local record: the SD file is read by a human and by
        // a script, both of which handle a missing value better than they handle a magic one.
        // The BLE path has no such option, which is why 0x600 carries sentinels instead.
        reading.isPressureValid ? std::format("{:.2f}", reading.pressurePa) : "null",
        reading.isTemperatureValid ? std::format("{:.2f}", reading.temperatureCentiC / 100.0) : "null",
        reading.isHumidityValid ? std::format("{:.2f}", reading.humidityCentiPct / 100.0) : "null",
        reading.isPressureValid ? "true" : "false",
        reading.isTemperatureValid ? "true" : "false",
        reading.isHumidityValid ? "true" : "false",
        // The raw ADC counts, kept beside the compensated values because commissioning item 4
        // asks for both: a calibration coefficient read wrong once at boot corrupts every
        // compensated value for the session, and the counts are the only way to reprocess it.
        reading.isPresent ? std::format("{}", reading.rawPressure) : "null",
        reading.isPresent ? std::format("{}", reading.rawTemperature) : "null",
        reading.isPresent ? std::format("{}", reading.rawHumidity) : "null",
        reading.isTemperatureValid ? std::format("{}", reading.tFine) : "null",
        reading.invalidReason == NULL ? "null" : std::format("\"{}\"", reading.invalidReason),
        reading.readMs, appData.bme280.readErrors,
        // Roughly one of each per cycle is the MEASURED normal for this board - a 1 Hz sampler
        // always finds the bus idle, and the first transfer after idle is always refused. What
        // matters is the ratio: recovered climbing far above one per cycle, or exhausted moving
        // off zero, is a bus that has degraded rather than one behaving as characterised.
        (guint64)appData.i2c.firstAttemptFailures,
        (guint64)appData.i2c.recoveredTransfers,
        (guint64)appData.i2c.exhaustedTransfers));

    publishEnclosurePacket(reading);
    publishEnclosureStatusPacket(reading);
}

// Commissioning item 2 asks for identity and calibration data at boot. For an SDP810 that is
// product, revision and serial with a CRC; the BME280 has no serial and no CRC, so its identity
// is the chip ID and its calibration IS the coefficient block - which is the part of the record
// that actually matters, because every compensated value in the session is a function of it.
void writeBme280Baseline() {
    writeSessionRecord("enclosureBaseline", std::format(
        "\"i2cBus\":{},\"devicePath\":\"{}\",\"address\":{},\"present\":{},"
        "\"chipId\":{},\"chipIdExpected\":{},\"calibrationRead\":{},\"calibration\":{},"
        "\"intervalMs\":{},\"sampling\":\"{}\",\"i2cRetryNote\":\"{}\","
        "\"pressureRole\":\"{}\","
        "\"temperatureRole\":\"{}\",\"humidityRole\":\"{}\"",
        appConfig.i2cBus,
        escapeJson(std::format(I2C_BUS_PATH_FORMAT, appConfig.i2cBus)),
        appConfig.bme280Address,
        appData.bme280.isPresent ? "true" : "false",
        appData.bme280.chipId, BME280_CHIP_ID,
        appData.bme280.isCalibrationLoaded ? "true" : "false",
        appData.bme280.isCalibrationLoaded
            ? getBme280CalibrationJson(appData.bme280.calibration) : "null",
        appConfig.bme280IntervalMs,
        "forced mode, oversampling x1 on all three quantities, IIR filter off, for lowest self-heating",
        "the first I2C transfer after an idle bus is refused on this board and a retry 500 us"
            " later succeeds (measured 2026-09-10); about one recovered transfer per sample cycle"
            " is normal, and i2cExhausted moving off zero is the signal that matters",
        "ENCLOSURE pressure, never a static reference: the cavity is aerodynamically live and at"
            " Cp -1 the offset is ~464 Pa against 45-90 Pa measurands. Density term only",
        "cavity thermometer (plan item 1c); NOT the inlet density term, which is T_ambient's DS18B20",
        "seal and desiccant diagnostic (plan items 1a and 1d); no measurement consumer"));
}

gpointer bme280SensorLoop(gpointer _) {
    g_message("Bme280: starting");

    appData.bme280.fd = -1;

    if (initialiseBme280()) {
        g_message("Bme280: chip ID 0x%02X at I2C 0x%02X on bus %d, calibration read",
            appData.bme280.chipId, appConfig.bme280Address, appConfig.i2cBus);
    }
    else {
        appData.bme280.isBusFailureReported = TRUE;
        g_warning("Bme280: no usable device at I2C 0x%02X on bus %d at start; the worker keeps"
            " retrying and reports the channels invalid until it answers",
            appConfig.bme280Address, appConfig.i2cBus);
    }

    writeBme280Baseline();

    appData.bme280.isRunning = true;

    while (!appData.shutdownRequested) {
        sampleBme280();
        g_usleep(BME280_IDLE_SLEEP_US);
    }

    appData.bme280.isRunning = false;
    closeBme280Bus();

    g_message("Bme280: shutting down, %u cycles, %u read errors,"
        " I2C first-attempt failures %lu, recovered by retry %lu, exhausted %lu",
        appData.bme280.sampleCycles, appData.bme280.readErrors,
        (unsigned long)appData.i2c.firstAttemptFailures,
        (unsigned long)appData.i2c.recoveredTransfers,
        (unsigned long)appData.i2c.exhaustedTransfers);

    // Last act, after every closing record: the session writer keeps draining until this hits zero.
    appData.producersRunning--;

    return NULL;
}
