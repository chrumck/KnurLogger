#pragma once

#include "blePackets.cxx"
#include "i2cBus.cxx"

// The five SDP810 differential-pressure sensors, one per PCA9548A channel, on the same main I2C
// bus as the BME280. It owns packets 0x605, 0x606 and 0x607.
//
// Four properties of this hardware shape the code and none of them is obvious:
//
//   1. ALL FIVE ANSWER AT 0x25 AND CANNOT BE STRAPPED APART. The mux is mandatory, one sensor per
//      channel, and every sample costs a channel select first.
//   2. ADDRESSING AN UNPOPULATED OR FAULTY CHANNEL HANGS THE WHOLE MAIN BUS - mux and BME280
//      included - and only a ~RESET pulse on GPIO17 recovers it. Both bus lines read idle-high and
//      i2cdetect still lists every device while it happens, so nothing cheap detects it. This is
//      why the worker iterates a CONFIGURED LIST and why selectMuxChannel refuses anything outside
//      it: a sweep has to be impossible to write by accident.
//   3. 0x3615 IS NAK'D WHEN THE PART IS ALREADY IN CONTINUOUS MODE, and a mux channel change does
//      not end that mode. It is issued once per sensor and re-issued only after a failure, because
//      a harness that re-armed every cycle had 145 of its 150 start-continuous commands refused
//      and looked like a dying bus.
//   4. THE SCALE FACTOR IS PER SENSOR AND ARRIVES IN EVERY FRAME. The +-125 Pa part returns 240
//      where the others return 60, and it was found on mux channel 2 rather than the specified
//      channel 4 only because it was asked what it was. Never hard-code 60.
//
// The retry for the idle-bus refusal lives in i2cBus.cxx and is deliberately not duplicated here.

#define PRESSURE_IDLE_SLEEP_US 5000
// The BME280 samples as soon as its worker starts, so the wait normally ends within tens of
// milliseconds; the bound stops a BME280 that is not answering from holding up raw logging.
#define PRESSURE_ABSOLUTE_START_WAIT_MS 2000

// --- Transport ---------------------------------------------------------------------------------

// The PCA9548A has one register and no register number: a single byte written is the channel
// bitmask, a single byte read is that mask back.
gboolean writeMuxControl(gint fd, guint8 control) {
    struct i2c_msg messages[] = {
        { .addr = (guint16)appConfig.muxAddress, .flags = 0, .len = 1, .buf = &control },
    };

    return transferI2c(fd, (guint8)appConfig.muxAddress, messages, getLength(messages));
}

gboolean readMuxControl(gint fd, guint8* out) {
    struct i2c_msg messages[] = {
        { .addr = (guint16)appConfig.muxAddress, .flags = I2C_M_RD, .len = 1, .buf = out },
    };

    return transferI2c(fd, (guint8)appConfig.muxAddress, messages, getLength(messages));
}

// Deselects every channel, so the main bus is never left with a downstream segment bridged onto it
// while nothing is reading it. Called at the end of every cycle and on shutdown.
gboolean deselectMux(gint fd) {
    auto isDeselected = writeMuxControl(fd, MUX_CHANNEL_NONE);
    appData.pressure.muxControl = isDeselected ? MUX_CHANNEL_NONE : -1;
    return isDeselected;
}

guint8 getMuxControlByte() {
    gint control = appData.pressure.muxControl;
    return control < 0 ? (guint8)PRESSURE_MUX_CONTROL_UNKNOWN : (guint8)control;
}

// The refusal below is the code half of the rule that no channel outside the populated list is
// ever addressed, and it is the whole reason the config key is a list of populated channels
// rather than a count of them.
//
// The read-back is not belt-and-braces either. A write that APPEARS to succeed onto a faulty
// segment is precisely the failure that hangs the bus, and reading the control register back is
// where it is caught - the mux answers from the main side, so it still replies while the segment
// it just connected is dragging the downstream lines together.
gboolean selectMuxChannel(gint fd, gint channel) {
    if (channel < 0 || channel > MUX_MAX_CHANNEL
        || !appConfig.pressureChannelsEnabled[channel]) {
        if (!appData.pressure.isChannelRefusalReported) {
            appData.pressure.isChannelRefusalReported = TRUE;
            g_warning("Pressure: refusing to select mux channel %d - it is not in"
                " pressureChannelsEnabled. Addressing an unpopulated channel hangs the whole main"
                " bus and only a ~RESET pulse on GPIO17 recovers it.", channel);
            writeEventRecord("error", std::format(
                "refused to select mux channel {}: not in {}",
                channel, CONFIG_KEY_PRESSURE_CHANNELS_ENABLED));
        }
        return FALSE;
    }

    appData.pressure.muxControl = (gint)(1u << channel);

    if (!writeMuxControl(fd, (guint8)(1u << channel))) { return FALSE; }

    guint8 readBack = 0;
    if (!readMuxControl(fd, &readBack)) { return FALSE; }

    return readBack == (guint8)(1u << channel);
}

// CRC-8, polynomial 0x31, initial value 0xFF, computed over each 2-byte word of a frame
// independently. Transcribed from the Sensirion SDP8xx datasheet.
guint8 sdpCrc8(const guint8* data, guint length) {
    guint8 crc = SDP810_CRC_INIT;

    for (guint i = 0; i < length; i++) {
        crc ^= data[i];
        for (auto bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (guint8)((crc << 1) ^ SDP810_CRC_POLYNOMIAL) : (guint8)(crc << 1);
        }
    }

    return crc;
}

// Straight through transferI2c, which already carries the ten-attempt retry. NO SECOND RETRY LAYER:
// a chain of chains turns a 5 ms recovery into a 50 ms one and hides the counters that are the only
// evidence of which boot mode the bus is in.
gboolean sendSdpCommand(gint fd, guint16 command) {
    guint8 payload[] = { (guint8)(command >> 8), (guint8)(command & 0xFF) };

    struct i2c_msg messages[] = {
        { .addr = SDP810_ADDRESS, .flags = 0, .len = getLength(payload), .buf = payload },
    };

    return transferI2c(fd, SDP810_ADDRESS, messages, getLength(messages));
}

gboolean readSdpFrame(gint fd, guint8* out, guint length) {
    struct i2c_msg messages[] = {
        { .addr = SDP810_ADDRESS, .flags = I2C_M_RD, .len = (guint16)length, .buf = out },
    };

    return transferI2c(fd, SDP810_ADDRESS, messages, getLength(messages));
}

// The part's own measurement range, by product number. A reading outside it is invalid data rather
// than a clipped value. An unrecognised product gets the wider bound: the logger warns about the
// product number at boot and then keeps the channel, because refusing every sample from an
// unexpected-but-working sensor would cost more than it saves.
gdouble getSdpRangePa(guint32 productNumber) {
    return productNumber == SDP810_PRODUCT_125PA ? SDP810_RANGE_125PA_PA : SDP810_RANGE_500PA_PA;
}

// The 9-byte measurement frame: differential pressure, sensor temperature and the scale factor,
// each a big-endian int16 followed by its own CRC. `rangePa` is passed in rather than looked up
// because the range is a property of the PART and this function only ever sees the frame.
void parseSdpMeasurement(const guint8* frame, gdouble rangePa, SdpReading* out) {
    out->isPresent = TRUE;

    // Every word is checked, not just the first. A CRC failure is INVALID DATA - never a
    // carried-forward value - and it is counted apart from a transport error because the two send
    // a reader to different parts of the box.
    out->crcOk = sdpCrc8(&frame[0], 2) == frame[2]
        && sdpCrc8(&frame[3], 2) == frame[5]
        && sdpCrc8(&frame[6], 2) == frame[8];

    if (!out->crcOk) {
        out->invalidReason = "crc";
        return;
    }

    out->rawDifferential = (gint16)((frame[0] << 8) | frame[1]);
    out->rawTemperature = (gint16)((frame[3] << 8) | frame[4]);
    out->scaleFactor = (guint16)((frame[6] << 8) | frame[7]);

    // A zero scale factor is a frame that passed its CRC and still means nothing, which is the one
    // case the CRC cannot catch. Dividing by it would produce an infinity that formats as a JSON
    // token no parser accepts.
    if (out->scaleFactor == 0) {
        out->invalidReason = "scaleFactorZero";
        return;
    }

    out->pressurePa = (gdouble)out->rawDifferential / (gdouble)out->scaleFactor;
    out->temperatureCentiC = (gint16)std::lround(
        (gdouble)out->rawTemperature * 100.0 / SDP810_TEMPERATURE_DIVISOR);

    if (std::fabs(out->pressurePa) > rangePa) {
        out->invalidReason = "outOfRange";
        return;
    }

    out->isValid = TRUE;
    out->invalidReason = NULL;
}

// --- The correction ----------------------------------------------------------------------------

#define CUBIC_METRES_PER_MILLILITRE 1e-6

// A port with length 0 is open to the bay, so it has no filter, wand or connector either.
gdouble getLinePathResistance(gint slot) {
    gdouble resistance = 0.0;

    auto lengthsM = { appConfig.pressureLineLengthHighM[slot], appConfig.pressureLineLengthLowM[slot] };
    for (auto lengthM : lengthsM) {
        if (lengthM <= 0.0) { continue; }
        resistance += appConfig.lineFixedResistance + appConfig.tubingResistancePerMetre * lengthM;
    }

    return resistance;
}

// DIVIDE by the density factor, never multiply: the SDP810 reads high in denser air, so
// multiplying applies the error twice.
//
// The pressure across the ports fixes the flow through the sensor's own bypass resistance,
// a * Q^n with Q in mL/s, and the lines lose that flow times their resistance before the sensor.
gboolean correctPressure(gint slot, gdouble readingPa, gdouble absolutePa, gdouble* outPa) {
    if (readingPa == 0.0) {
        *outPa = 0.0;
        return TRUE;
    }

    auto density = absolutePa / SDP810_CALIBRATION_ABSOLUTE_PA;
    auto span = readingPa >= 0.0
        ? appConfig.pressureSpanPositive[slot] : appConfig.pressureSpanNegative[slot];
    auto portPa = std::fabs(readingPa) / (density * (1.0 + span));

    auto flowMl = std::pow(
        portPa / (appConfig.pressureBypassCoefficient[slot] * CUBIC_METRES_PER_MILLILITRE),
        1.0 / (appConfig.pressureBypassExponent[slot] + 1.0));
    auto correctedPa = portPa + flowMl * CUBIC_METRES_PER_MILLILITRE * getLinePathResistance(slot);

    if (!std::isfinite(correctedPa)) { return FALSE; }

    *outPa = std::copysign(correctedPa, readingPa);
    return TRUE;
}

void reportAbsolutePressureChange(const gchar* refusal, gdouble absolutePa, guint64 ageMs) {
    if (refusal != NULL && !appData.pressure.isAbsolutePressureRefusalReported) {
        appData.pressure.isAbsolutePressureRefusalReported = TRUE;
        auto detail = g_str_equal(refusal, "absolutePressureMissing")
            ? std::string("no valid BME280 pressure yet")
            : std::format("last valid BME280 pressure {:.2f} Pa is {} ms old", absolutePa, ageMs);
        g_warning("Pressure: %s (%s); every enabled channel is sent invalid until it returns",
            refusal, detail.c_str());
        writeEventRecord("warning", std::format(
            "{}: {}; every enabled pressure channel sent invalid until it returns", refusal, detail));
        return;
    }

    if (refusal == NULL && appData.pressure.isAbsolutePressureRefusalReported) {
        appData.pressure.isAbsolutePressureRefusalReported = FALSE;
        g_message("Pressure: absolute pressure back at %.2f Pa; channels corrected again", absolutePa);
        writeEventRecord("info", std::format(
            "absolute pressure back at {:.2f} Pa, {} ms old; pressure channels corrected again",
            absolutePa, ageMs));
    }
}

void applyPressureCorrection(gint slot, const gchar* absoluteRefusal, gdouble absolutePa,
    SdpReading* reading) {
    reading->pressureDeciPa = PRESSURE_DECI_PA_INVALID;

    if (absoluteRefusal != NULL) {
        reading->invalidReason = absoluteRefusal;
        return;
    }

    // A value past the wire's int16 would wrap into a plausible pressure of the other sign.
    if (!correctPressure(slot, reading->pressurePa, absolutePa, &reading->correctedPa)
        || std::fabs(reading->correctedPa) * PRESSURE_DECI_PA_PER_PA > INT16_MAX) {
        reading->invalidReason = "correctedOutOfRange";
        return;
    }

    reading->isCorrected = TRUE;
    reading->pressureDeciPa = (gint16)std::lround(reading->correctedPa * PRESSURE_DECI_PA_PER_PA);
}

// --- Identity, and the baseline record ---------------------------------------------------------

void closePressureBus() {
    if (appData.pressure.fd < 0) { return; }
    close(appData.pressure.fd);
    appData.pressure.fd = -1;
}

// Identity is 0x367C then 0xE102 then an 18-byte read: six words, each with its own CRC. The first
// two words are the product number, the last four the 64-bit serial.
gboolean readSdpIdentity(gint fd, guint32* outProduct, guint64* outSerial) {
    if (!sendSdpCommand(fd, SDP810_CMD_READ_PRODUCT_ID_1)) { return FALSE; }
    if (!sendSdpCommand(fd, SDP810_CMD_READ_PRODUCT_ID_2)) { return FALSE; }

    guint8 frame[SDP810_IDENTITY_LENGTH];
    if (!readSdpFrame(fd, frame, getLength(frame))) { return FALSE; }

    guint16 words[6];
    for (auto i = 0; i < 6; i++) {
        const guint8* word = &frame[i * 3];
        if (sdpCrc8(word, 2) != word[2]) { return FALSE; }
        words[i] = (guint16)((word[0] << 8) | word[1]);
    }

    *outProduct = ((guint32)words[0] << 16) | words[1];
    *outSerial = ((guint64)words[2] << 48) | ((guint64)words[3] << 32)
        | ((guint64)words[4] << 16) | words[5];

    return TRUE;
}

// A mismatch WARNS and does not refuse. A logger that will not start in the car because one sensor
// reports an unexpected product number loses every other channel with it - the same principle the
// TEMP_OFFSET_MAX_C comment states for a mistyped calibration offset.
void reportIdentityMismatch(const PressureChannel& channel) {
    if (channel.productNumber != SDP810_PRODUCT_500PA
        && channel.productNumber != SDP810_PRODUCT_125PA) {
        g_warning("Pressure: P%d reports product 0x%08X, which is neither the +-500 Pa 0x%08X nor"
            " the +-125 Pa 0x%08X - logging it anyway",
            channel.muxChannel, channel.productNumber, SDP810_PRODUCT_500PA, SDP810_PRODUCT_125PA);
        writeEventRecord("warning", std::format(
            "P{} product number 0x{:08X} is not a known SDP810; channel kept",
            channel.muxChannel, channel.productNumber));
        return;
    }

    auto expected = channel.productNumber == SDP810_PRODUCT_125PA
        ? SDP810_SCALE_125PA : SDP810_SCALE_500PA;
    if (channel.expectedScale == expected) { return; }

    // The returned factor still wins - this only says the part is not the one its product number
    // implies, which is worth a record because it is the check that caught the +-125 Pa sitting on
    // a channel every document said was a +-500 Pa.
    g_warning("Pressure: P%d returns %u counts/Pa but product 0x%08X specifies %d - using the"
        " returned factor", channel.muxChannel, channel.expectedScale, channel.productNumber, expected);
    writeEventRecord("warning", std::format(
        "P{} scale factor {} disagrees with product 0x{:08X} (expected {}); returned factor used",
        channel.muxChannel, channel.expectedScale, channel.productNumber, expected));
}

// Reads identity from every ENABLED channel. Stop-continuous first and its failure tolerated: the
// part may legitimately be idle already, and a NAK there says nothing.
gboolean initialisePressureSensors() {
    if (appData.pressure.fd < 0) { appData.pressure.fd = openI2cBus(appConfig.i2cBus); }
    if (appData.pressure.fd < 0) { return FALSE; }

    auto fd = appData.pressure.fd;
    gboolean isAnyPresent = FALSE;

    for (auto i = 0; i < PRESSURE_CHANNEL_COUNT; i++) {
        auto& channel = appData.pressure.channels[i];
        if (!channel.isEnabled) { continue; }

        channel.isContinuousStarted = FALSE;

        if (!selectMuxChannel(fd, channel.muxChannel)) {
            channel.isPresent = FALSE;
            g_warning("Pressure: could not select mux channel %d", channel.muxChannel);
            continue;
        }

        sendSdpCommand(fd, SDP810_CMD_STOP_CONTINUOUS);

        guint32 productNumber = 0;
        guint64 serial = 0;
        if (!readSdpIdentity(fd, &productNumber, &serial)) {
            channel.isPresent = FALSE;
            g_warning("Pressure: no SDP810 identity from mux channel %d", channel.muxChannel);
            continue;
        }

        channel.isPresent = TRUE;
        channel.productNumber = productNumber;
        channel.serial = serial;
        // Provisional: overwritten by the first measurement frame, which is the authority. It is
        // recorded here only so the baseline has something to compare against.
        channel.expectedScale = productNumber == SDP810_PRODUCT_125PA
            ? SDP810_SCALE_125PA : SDP810_SCALE_500PA;
        isAnyPresent = TRUE;

        g_message("Pressure: P%d product 0x%08X serial 0x%016llX",
            channel.muxChannel, productNumber, (unsigned long long)serial);
    }

    deselectMux(fd);

    return isAnyPresent;
}

std::string getPressureChannelsJson() {
    std::string result;

    for (auto i = 0; i < PRESSURE_CHANNEL_COUNT; i++) {
        const auto& channel = appData.pressure.channels[i];
        result += std::format(
            // `expectedScaleFactor` rather than `scaleFactor`, and the distinction is the whole
            // point of this record: at boot the part has only been asked what it IS, not what it
            // reads, so this is the factor its product number implies. The factor it actually
            // returns is in every sample record, it is what the pascals are computed from, and it
            // wins. A provenance record that quietly reported an expectation as a measurement
            // would be worse than one that reported nothing.
            "{}{{\"ch\":\"P{}\",\"muxChannel\":{},\"enabled\":{},\"present\":{},"
            "\"productNumber\":{},\"serial\":{},\"expectedScaleFactor\":{},\"rangePa\":{:.1f}}}",
            i == 0 ? "" : ",", i, channel.muxChannel,
            channel.isEnabled ? "true" : "false",
            channel.isPresent ? "true" : "false",
            channel.isPresent ? std::format("\"0x{:08X}\"", channel.productNumber) : "null",
            channel.isPresent ? std::format("\"0x{:016X}\"", channel.serial) : "null",
            channel.isPresent ? std::format("{}", channel.expectedScale) : "null",
            channel.isPresent ? getSdpRangePa(channel.productNumber) : 0.0);
    }

    return result;
}

// Written into the baseline so a recording states its own correction and can be recomputed from
// its raw readings without the .ini it was made with.
std::string getPressureCorrectionJson() {
    std::string channels;

    for (auto i = 0; i < PRESSURE_CHANNEL_COUNT; i++) {
        if (!appConfig.pressureChannelsEnabled[i]) { continue; }

        channels += std::format(
            "{}{{\"ch\":\"P{}\",\"spanPositive\":{},\"spanNegative\":{},\"lineLengthHighM\":{},"
            "\"lineLengthLowM\":{},\"bypassCoefficient\":{},\"bypassExponent\":{},"
            "\"linePathResistance\":{}}}",
            channels.empty() ? "" : ",", i,
            appConfig.pressureSpanPositive[i], appConfig.pressureSpanNegative[i],
            appConfig.pressureLineLengthHighM[i], appConfig.pressureLineLengthLowM[i],
            appConfig.pressureBypassCoefficient[i], appConfig.pressureBypassExponent[i],
            getLinePathResistance(i));
    }

    return std::format(
        "{{\"formula\":\"divide\",\"calibrationAbsolutePa\":{},\"absoluteHoldMs\":{},"
        "\"tubingResistancePerMetre\":{},\"lineFixedResistance\":{},\"channels\":[{}]}}",
        SDP810_CALIBRATION_ABSOLUTE_PA, PRESSURE_ABSOLUTE_HOLD_MS,
        appConfig.tubingResistancePerMetre, appConfig.lineFixedResistance, channels);
}

// Product, revision, serial and CRC are read at boot and logged with the channel mapping. THIS
// RECORD IS THE PER-SESSION CHANNEL -> PART PROVENANCE, and it is the only place a later analysis
// can learn which physical sensor produced which channel.
void writePressureBaseline() {
    writeSessionRecord("pressureBaseline", std::format(
        "\"i2cBus\":{},\"devicePath\":\"{}\",\"muxAddress\":{},\"sensorAddress\":{},"
        "\"intervalMs\":{},\"enabledCount\":{},\"channels\":[{}],"
        "\"sampling\":\"{}\",\"wireScaling\":\"{}\",\"correction\":{}",
        appConfig.i2cBus,
        escapeJson(std::format(I2C_BUS_PATH_FORMAT, appConfig.i2cBus)),
        appConfig.muxAddress, SDP810_ADDRESS,
        appConfig.pressureIntervalMs, appData.pressure.enabledCount,
        getPressureChannelsJson(),
        "continuous differential pressure, temperature compensated, averaged (0x3615), started"
            " once per sensor; each cycle is a mux select plus a 9-byte read",
        "the corrected pressure at 0.1 Pa/LSB signed on the BLE path, INT16_MIN for no"
            " trustworthy or no correctable reading; the phone divides by 10000, so a RaceChrono"
            " pressure column is in kPa and its invalid marker is -3.2768; raw counts, the returned"
            " scale factor and the raw pressure are in every sample record here at full resolution"
            " and are unaffected by the correction and the BLE scaling",
        getPressureCorrectionJson()));
}

// --- The sampling loop -------------------------------------------------------------------------

// 0x605 and 0x606 bytes 0-3: the six channels as signed decipascals. Every payload field is
// big-endian; only the packet ID is little-endian.
void publishPressurePackets() {
    gint16 deciPa[PRESSURE_CHANNEL_COUNT];
    for (auto i = 0; i < PRESSURE_CHANNEL_COUNT; i++) {
        const auto& reading = appData.pressure.channels[i].lastReading;
        deciPa[i] = reading.isValid ? reading.pressureDeciPa : (gint16)PRESSURE_DECI_PA_INVALID;
    }

    guint8 dataA[CAN_DATA_SIZE];
    for (auto i = 0; i < 4; i++) {
        dataA[i * 2] = (guint8)((deciPa[i] >> 8) & 0xFF);
        dataA[i * 2 + 1] = (guint8)(deciPa[i] & 0xFF);
    }
    updateBlePacket(&appData.bluetooth.pressureA, dataA);

    // Free-running, and it advances whatever the sensors report. That matters more here than on
    // 0x601 or 0x603: five steady zeroes is a PLAUSIBLE reading at rest, so without this counter a
    // dead worker and a rig sitting in still air look identical on the phone.
    auto cycles = (guint16)(appData.pressure.sampleCycles & 0xFFFF);
    auto cycleMs = (guint16)std::min<guint32>(appData.pressure.lastCycleMs, G_MAXUINT16);

    guint8 dataB[CAN_DATA_SIZE] = {
        (guint8)((deciPa[4] >> 8) & 0xFF),
        (guint8)(deciPa[4] & 0xFF),
        (guint8)((deciPa[5] >> 8) & 0xFF),
        (guint8)(deciPa[5] & 0xFF),
        (guint8)((cycles >> 8) & 0xFF),
        (guint8)(cycles & 0xFF),
        (guint8)((cycleMs >> 8) & 0xFF),
        (guint8)(cycleMs & 0xFF),
    };
    updateBlePacket(&appData.bluetooth.pressureB, dataB);
}

// 0x607. Read errors and CRC failures are two counters on purpose: one says the bus did not
// complete a transfer, the other says a frame arrived and could not be trusted, and they send a
// reader to different parts of the box. Collapsing them is what left a probe sitting at 85.000 C
// misclassified as a bus fault for two track days on the thermal side.
void publishPressureStatusPacket(guint8 enabledMask, guint8 sentMask, gint16 sensorTemperatureC) {
    auto readErrors = (guint16)std::min<guint32>(appData.pressure.readErrors, G_MAXUINT16);
    auto crcFailures = (guint16)std::min<guint32>(appData.pressure.crcFailures, G_MAXUINT16);

    // A valid reading is clamped short of INT8_MIN so it can never be mistaken for the marker.
    auto temperatureByte = sensorTemperatureC == PRESSURE_SENSOR_TEMP_INVALID_C
        ? (guint8)(gint8)PRESSURE_SENSOR_TEMP_INVALID_C
        : (guint8)(gint8)std::clamp<gint16>(sensorTemperatureC, INT8_MIN + 1, INT8_MAX);

    guint8 data[CAN_DATA_SIZE] = {
        enabledMask,
        sentMask,
        (guint8)((readErrors >> 8) & 0xFF),
        (guint8)(readErrors & 0xFF),
        (guint8)((crcFailures >> 8) & 0xFF),
        (guint8)(crcFailures & 0xFF),
        temperatureByte,
        getMuxControlByte(),
    };

    updateBlePacket(&appData.bluetooth.pressureStatus, data);
}

// Called by the BLE worker before it sends 0x607. A cycle stuck in a transfer publishes nothing, and
// the notify timer never re-sends an unchanged packet, so this is the only way the phone can learn
// that the worker is stuck and on which channel. The byte returns to 0 with the cycle's own packet.
void reportPressureStall() {
    guint64 cycleStartUs = appData.pressure.cycleStartBootUs;
    if (cycleStartUs == 0) { return; }

    auto nowUs = getBootTimeUs();
    if (nowUs < cycleStartUs + (guint64)PRESSURE_STALL_REPORT_MS * 1000) { return; }

    auto* packet = &appData.bluetooth.pressureStatus;
    auto control = getMuxControlByte();

    g_mutex_lock(&packet->lock);
    if (packet->data[7] != control) {
        packet->data[7] = control;
        packet->updatedBootUs = nowUs;
        packet->wasSent = FALSE;
    }
    g_mutex_unlock(&packet->lock);
}

// One channel: select, re-arm if needed, read, parse. Every failure path leaves a reading that is
// explicitly invalid with a reason; none of them leaves the previous cycle's value in place.
SdpReading readPressureChannel(gint fd, PressureChannel* channel) {
    SdpReading reading = {};
    reading.pressureDeciPa = PRESSURE_DECI_PA_INVALID;
    reading.invalidReason = "notPresent";

    auto startUs = getBootTimeUs();

    if (!selectMuxChannel(fd, channel->muxChannel)) {
        reading.invalidReason = "muxSelectFailed";
        reading.isReadError = TRUE;
        reading.readMs = (guint32)((getBootTimeUs() - startUs) / 1000);
        return reading;
    }

    // Re-armed only when this worker believes the sensor is NOT continuous - after a failed read,
    // or at start. A part that browned out has forgotten the mode; one that did not answers the
    // command with a NAK, which is harmless BECAUSE it is expected. Re-arming unconditionally is
    // the 145-of-150 failure from bring-up.
    if (!channel->isContinuousStarted) {
        sendSdpCommand(fd, SDP810_CMD_START_CONTINUOUS);
        g_usleep(SDP810_START_SETTLE_US);
    }

    guint8 frame[SDP810_MEASUREMENT_LENGTH];
    if (!readSdpFrame(fd, frame, getLength(frame))) {
        reading.invalidReason = channel->isContinuousStarted ? "readFailed" : "notStarted";
        reading.isReadError = TRUE;
        reading.readMs = (guint32)((getBootTimeUs() - startUs) / 1000);
        return reading;
    }

    parseSdpMeasurement(frame, getSdpRangePa(channel->productNumber), &reading);
    reading.readMs = (guint32)((getBootTimeUs() - startUs) / 1000);

    // The read worked, so the part is in continuous mode whether or not this worker put it there.
    channel->isContinuousStarted = TRUE;

    return reading;
}

void samplePressure() {
    static guint64 nextSampleBootUs = 0;

    auto nowBootUs = getBootTimeUs();
    if (nowBootUs < nextSampleBootUs) { return; }
    nextSampleBootUs = nowBootUs + (guint64)appConfig.pressureIntervalMs * 1000;
    // Set before the re-initialisation below, whose selects can stall just as a sample's can.
    appData.pressure.cycleStartBootUs = nowBootUs;

    // Cheap enough to attempt when nothing is answering, and the alternative is worse: a bus that
    // was reset, or a sensor that browned out during cranking, would otherwise stay dark for the
    // rest of a twelve-hour day.
    if (appData.pressure.fd < 0) {
        if (!initialisePressureSensors() && !appData.pressure.isBusFailureReported) {
            appData.pressure.isBusFailureReported = TRUE;
            g_warning("Pressure: no SDP810 answering on bus %d; retrying, reporting invalid until"
                " one does", appConfig.i2cBus);
            writeEventRecord("error", std::format(
                "no SDP810 answering behind the mux at 0x{:02X} on bus {}",
                appConfig.muxAddress, appConfig.i2cBus));
        }
    }

    auto cycleStartUs = getBootTimeUs();
    auto fd = appData.pressure.fd;

    gdouble absolutePa = 0.0;
    guint64 absoluteAgeMs = 0;
    auto isAbsoluteHeld = getHeldAbsolutePressure(&absolutePa, &absoluteAgeMs);
    const gchar* absoluteRefusal = !isAbsoluteHeld ? "absolutePressureMissing"
        : absoluteAgeMs > PRESSURE_ABSOLUTE_HOLD_MS ? "absolutePressureStale" : NULL;
    reportAbsolutePressureChange(absoluteRefusal, absolutePa, absoluteAgeMs);

    guint8 enabledMask = 0;
    guint8 validMask = 0;
    // 0x607's mask: a channel whose raw reading is valid but could not be corrected sends
    // INT16_MIN, so the phone must not be told it is valid.
    guint8 sentMask = 0;
    guint validChannelCount = 0;
    // 0x607 byte 6 describes ONE part, the lowest enabled channel's, so that a reading on the phone
    // always means the same sensor; when that part has no valid reading it says so.
    gint16 sensorTemperatureC = PRESSURE_SENSOR_TEMP_INVALID_C;
    gint lowestEnabledChannel = -1;
    std::string channelFields;

    for (auto i = 0; i < PRESSURE_CHANNEL_COUNT; i++) {
        auto& channel = appData.pressure.channels[i];

        // Iterating CHANNELS rather than answering sensors is what makes a disabled or absent
        // channel land in the record as explicitly invalid instead of vanishing from it. A
        // vanished channel is indistinguishable from a logger that was not running.
        SdpReading reading = {};
        reading.pressureDeciPa = PRESSURE_DECI_PA_INVALID;
        reading.invalidReason = "disabled";

        if (channel.isEnabled) {
            enabledMask |= (guint8)(1u << i);
            if (lowestEnabledChannel < 0) { lowestEnabledChannel = i; }
            if (fd < 0) { reading.invalidReason = "busUnavailable"; }
            else { reading = readPressureChannel(fd, &channel); }
        }

        if (reading.isReadError) {
            channel.readErrors++;
            appData.pressure.readErrors++;
            // The part may have reset, and a part that reset is no longer in continuous mode. This
            // is what makes the next cycle re-arm it - and what stops every OTHER cycle re-arming.
            channel.isContinuousStarted = FALSE;
            channel.isPresent = FALSE;
        }
        if (channel.isEnabled && reading.isPresent && !reading.crcOk) {
            channel.crcFailures++;
            appData.pressure.crcFailures++;
        }
        if (reading.isPresent) { channel.isPresent = TRUE; }

        // The returned factor is the authority and is retained per sensor, so a channel whose part
        // was swapped is picked up from the data rather than from the config.
        if (reading.isValid) {
            channel.expectedScale = reading.scaleFactor;
            validMask |= (guint8)(1u << i);
            validChannelCount++;

            if (i == lowestEnabledChannel) {
                sensorTemperatureC = (gint16)std::lround(reading.temperatureCentiC / 100.0);
            }

            applyPressureCorrection(i, absoluteRefusal, absolutePa, &reading);
            if (reading.isCorrected) { sentMask |= (guint8)(1u << i); }
        }

        channel.lastReading = reading;

        channelFields += std::format(
            "{}{{\"ch\":\"P{}\",\"muxChannel\":{},\"enabled\":{},\"present\":{},\"valid\":{},"
            "\"rawDifferential\":{},\"scaleFactor\":{},\"pressurePa\":{},\"correctedPa\":{},"
            "\"sentDeciPa\":{},"
            "\"rawTemperature\":{},\"sensorTemperatureC\":{},\"crcOk\":{},\"reason\":{},"
            "\"readMs\":{},\"readErrors\":{},\"crcFailures\":{}}}",
            i == 0 ? "" : ",", i, channel.muxChannel,
            channel.isEnabled ? "true" : "false",
            reading.isPresent ? "true" : "false",
            reading.isValid ? "true" : "false",
            // The raw counts and the scale factor beside the computed pascals, because that pair
            // is what makes a reprocess possible if the 0.1 Pa/LSB wire scaling is ever revisited.
            reading.isValid ? std::format("{}", (int)reading.rawDifferential) : "null",
            reading.isValid ? std::format("{}", reading.scaleFactor) : "null",
            reading.isValid ? std::format("{:.4f}", reading.pressurePa) : "null",
            reading.isCorrected ? std::format("{:.4f}", reading.correctedPa) : "null",
            (int)reading.pressureDeciPa,
            reading.isValid ? std::format("{}", (int)reading.rawTemperature) : "null",
            reading.isValid ? std::format("{:.2f}", reading.temperatureCentiC / 100.0) : "null",
            reading.isPresent ? (reading.crcOk ? "true" : "false") : "null",
            reading.invalidReason == NULL ? "null" : std::format("\"{}\"", reading.invalidReason),
            reading.readMs, channel.readErrors, channel.crcFailures);
    }

    if (fd >= 0) { deselectMux(fd); }
    else { appData.pressure.muxControl = -1; }

    appData.pressure.cycleStartBootUs = 0;
    appData.pressure.sampleCycles++;
    appData.pressure.lastCycleMs = (guint32)((getBootTimeUs() - cycleStartUs) / 1000);

    writeSessionRecord("pressure", std::format(
        "\"cycle\":{},\"enabledMask\":{},\"validMask\":{},\"sentMask\":{},\"validChannels\":{},"
        "\"absolutePressurePa\":{},\"absolutePressureAgeMs\":{},\"cycleMs\":{},"
        "\"muxControl\":{},\"readErrors\":{},\"crcFailures\":{},"
        "\"i2cFirstAttemptFailures\":{},\"i2cRecovered\":{},\"i2cExhausted\":{},\"channels\":[{}]",
        appData.pressure.sampleCycles, enabledMask, validMask, sentMask, validChannelCount,
        // The held value this cycle read, also when it was too old to use: its age says why.
        isAbsoluteHeld ? std::format("{:.2f}", absolutePa) : "null",
        isAbsoluteHeld ? std::format("{}", absoluteAgeMs) : "null",
        appData.pressure.lastCycleMs, (gint)appData.pressure.muxControl,
        appData.pressure.readErrors, appData.pressure.crcFailures,
        // Logged with every pressure session because the first-transfer refusal is bimodal per
        // boot: these three
        // are what say which mode THIS boot was in, and a clean run is evidence about the boot
        // rather than about the board.
        (guint64)appData.i2c.firstAttemptFailures,
        (guint64)appData.i2c.recoveredTransfers,
        (guint64)appData.i2c.exhaustedTransfers,
        channelFields));

    publishPressurePackets();
    publishPressureStatusPacket(enabledMask, sentMask, sensorTemperatureC);
}

void waitForAbsolutePressure() {
    auto deadlineUs = getBootTimeUs() + (guint64)PRESSURE_ABSOLUTE_START_WAIT_MS * 1000;
    gdouble absolutePa = 0.0;
    guint64 ageMs = 0;

    while (!appData.shutdownRequested && getBootTimeUs() < deadlineUs
        && !getHeldAbsolutePressure(&absolutePa, &ageMs)) {
        g_usleep(PRESSURE_IDLE_SLEEP_US);
    }
}

void stopPressureSensors() {
    if (appData.pressure.fd < 0) { return; }

    for (auto i = 0; i < PRESSURE_CHANNEL_COUNT; i++) {
        auto& channel = appData.pressure.channels[i];
        if (!channel.isEnabled || !channel.isContinuousStarted) { continue; }

        if (selectMuxChannel(appData.pressure.fd, channel.muxChannel)) {
            sendSdpCommand(appData.pressure.fd, SDP810_CMD_STOP_CONTINUOUS);
        }
        channel.isContinuousStarted = FALSE;
    }

    deselectMux(appData.pressure.fd);
    closePressureBus();
}

gpointer pressureSensorsLoop(gpointer _) {
    g_message("Pressure: starting");

    appData.pressure.fd = -1;
    appData.pressure.muxControl = MUX_CHANNEL_NONE;
    appData.pressure.cycleStartBootUs = 0;

    for (auto i = 0; i < PRESSURE_CHANNEL_COUNT; i++) {
        auto& channel = appData.pressure.channels[i];
        channel.muxChannel = i;
        channel.isEnabled = appConfig.pressureChannelsEnabled[i];
        channel.lastReading.pressureDeciPa = PRESSURE_DECI_PA_INVALID;
        channel.lastReading.invalidReason = channel.isEnabled ? "notPresent" : "disabled";
        if (channel.isEnabled) { appData.pressure.enabledCount++; }
    }

    if (!initialisePressureSensors()) {
        appData.pressure.isBusFailureReported = TRUE;
        g_warning("Pressure: no SDP810 answered at start on bus %d; the worker keeps retrying and"
            " reports every channel invalid until one does", appConfig.i2cBus);
    }

    for (auto i = 0; i < PRESSURE_CHANNEL_COUNT; i++) {
        if (!appData.pressure.channels[i].isEnabled) { continue; }
        if (!appData.pressure.channels[i].isPresent) { continue; }
        reportIdentityMismatch(appData.pressure.channels[i]);
    }

    writePressureBaseline();
    waitForAbsolutePressure();

    appData.pressure.isRunning = true;

    while (!appData.shutdownRequested) {
        samplePressure();
        g_usleep(PRESSURE_IDLE_SLEEP_US);
    }

    appData.pressure.isRunning = false;
    stopPressureSensors();

    g_message("Pressure: shutting down, %u cycles, %u read errors, %u CRC failures",
        appData.pressure.sampleCycles, appData.pressure.readErrors, appData.pressure.crcFailures);

    // Last act, after every closing record: the session writer keeps draining until this hits zero.
    appData.producersRunning--;

    return NULL;
}
