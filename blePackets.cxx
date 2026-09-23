#pragma once

#include "sessionWriter.cxx"

// The RaceChrono DIY CAN-Bus device protocol's packet primitives, kept separate from the BLE
// worker so that the worker which OWNS a reading is the one that packs it. In a single translation
// unit that also settles the include order: this comes before every producer, the BLE worker comes
// after them.
//
// Two properties of the protocol are easy to get backwards, and both are transcribed from two
// bench-proven implementations rather than designed here — KnurDash's bluetoothWorker.c and the
// ESP32 test rig:
//
//   1. The packet ID is the ONE little-endian field. Every payload field is big-endian.
//   2. RaceChrono, not the logger, chooses the notify rate — it writes a filter command asking for
//      all frames or one frame at an interval in milliseconds. So "10 Hz" is a request the phone
//      makes, and the logger's job is to have fresh data ready when it fires.

void updateBlePacket(BlePacket* packet, const guint8* data) {
    g_mutex_lock(&packet->lock);
    memcpy(packet->data, data, CAN_DATA_SIZE);
    packet->updatedBootUs = getBootTimeUs();
    // Cleared so the notify timer knows there is something new to send. A packet whose data has not
    // changed is deliberately NOT re-sent: the logger never re-transmits a stale value as though it
    // were new, on the BLE path any more than in the session file.
    packet->wasSent = FALSE;
    g_mutex_unlock(&packet->lock);
}

GByteArray* buildPacketBytes(BlePacket* packet) {
    auto* bytes = g_byte_array_sized_new(CAN_FRAME_ID_LENGTH + CAN_DATA_SIZE);

    g_mutex_lock(&packet->lock);

    guint8 packetId[CAN_FRAME_ID_LENGTH] = {
        (guint8)(packet->packetId & 0xFF),
        (guint8)((packet->packetId >> 8) & 0xFF),
        (guint8)((packet->packetId >> 16) & 0xFF),
        (guint8)((packet->packetId >> 24) & 0xFF),
    };

    g_byte_array_append(bytes, packetId, CAN_FRAME_ID_LENGTH);
    g_byte_array_append(bytes, packet->data, CAN_DATA_SIZE);
    packet->wasSent = TRUE;

    g_mutex_unlock(&packet->lock);

    return bytes;
}

BlePacket* getAllPackets(guint index) {
    switch (index) {
    case 0: return &appData.bluetooth.enclosure;
    case 1: return &appData.bluetooth.enclosureStatus;
    case 2: return &appData.bluetooth.temp;
    case 3: return &appData.bluetooth.thermalStatus;
    case 4: return &appData.bluetooth.supply;
    case 5: return &appData.bluetooth.pressureA;
    case 6: return &appData.bluetooth.pressureB;
    case 7: return &appData.bluetooth.pressureStatus;
    default: return NULL;
    }
}

#define forEachBlePacket(_packet) \
    for (guint _i = 0; BlePacket* _packet = getAllPackets(_i); _i++)


void initialiseBlePackets() {
    appData.bluetooth.enclosure.packetId = PACKET_ID_ENCLOSURE;
    appData.bluetooth.enclosure.name = "enclosure";
    appData.bluetooth.enclosureStatus.packetId = PACKET_ID_ENCLOSURE_STATUS;
    appData.bluetooth.enclosureStatus.name = "enclosureStatus";
    appData.bluetooth.temp.packetId = PACKET_ID_TEMP;
    appData.bluetooth.temp.name = "temp";
    appData.bluetooth.thermalStatus.packetId = PACKET_ID_THERMAL_STATUS;
    appData.bluetooth.thermalStatus.name = "thermalStatus";
    appData.bluetooth.supply.packetId = PACKET_ID_SUPPLY;
    appData.bluetooth.supply.name = "supply";
    appData.bluetooth.pressureA.packetId = PACKET_ID_PRESSURE_A;
    appData.bluetooth.pressureA.name = "pressureA";
    appData.bluetooth.pressureB.packetId = PACKET_ID_PRESSURE_B;
    appData.bluetooth.pressureB.name = "pressureB";
    appData.bluetooth.pressureStatus.packetId = PACKET_ID_PRESSURE_STATUS;
    appData.bluetooth.pressureStatus.name = "pressureStatus";

    forEachBlePacket(packet) {
        g_mutex_init(&packet->lock);
        packet->wasSent = TRUE;
    }

    // Published before any probe is enrolled, so all four temperature channels read the invalid
    // sentinel rather than zero. -327.68 C in RaceChrono is unmistakable, and it is the correct
    // answer for a channel with no probe bound: zero would be a plausible temperature.
    guint8 tempData[CAN_DATA_SIZE];
    for (auto i = 0; i < TEMP_CHANNEL_COUNT; i++) {
        tempData[i * 2] = (guint8)((TEMP_CENTI_C_INVALID >> 8) & 0xFF);
        tempData[i * 2 + 1] = (guint8)(TEMP_CENTI_C_INVALID & 0xFF);
    }
    updateBlePacket(&appData.bluetooth.temp, tempData);

    // The same argument for the enclosure channels: a phone that connects before the BME280 has
    // been read once must see the invalid sentinels rather than three zeroes, which would decode
    // as 0 Pa, 0 C and 0 %RH - all three plausible enough to be believed for a moment.
    guint8 enclosureData[CAN_DATA_SIZE] = {
        0xFF, 0xFF, 0xFF, 0xFF,
        (guint8)((TEMP_CENTI_C_INVALID >> 8) & 0xFF), (guint8)(TEMP_CENTI_C_INVALID & 0xFF),
        0xFF, 0xFF,
    };
    updateBlePacket(&appData.bluetooth.enclosure, enclosureData);

    // THE SAME ARGUMENT AGAIN, AND IT IS STRONGER FOR PRESSURE THAN FOR ANYTHING ELSE HERE. Zero
    // is a completely plausible differential pressure - it is what a healthy rig reads at rest -
    // so a phone that connects before the first cycle would otherwise see six believable readings
    // of nothing. All six channels go out as INT16_MIN, i.e. -3276.8 Pa, which is 6.5x the widest
    // part's full range and therefore impossible.
    guint8 pressureData[CAN_DATA_SIZE];
    for (auto i = 0; i < 4; i++) {
        pressureData[i * 2] = (guint8)((PRESSURE_DECI_PA_INVALID >> 8) & 0xFF);
        pressureData[i * 2 + 1] = (guint8)(PRESSURE_DECI_PA_INVALID & 0xFF);
    }
    updateBlePacket(&appData.bluetooth.pressureA, pressureData);

    // P4 and P5 as sentinels; the cycle counter and cycle time start at zero, which is honest -
    // no cycle has run. A counter that never leaves zero is exactly what it is there to show.
    guint8 pressureBData[CAN_DATA_SIZE] = {
        (guint8)((PRESSURE_DECI_PA_INVALID >> 8) & 0xFF), (guint8)(PRESSURE_DECI_PA_INVALID & 0xFF),
        (guint8)((PRESSURE_DECI_PA_INVALID >> 8) & 0xFF), (guint8)(PRESSURE_DECI_PA_INVALID & 0xFF),
        0, 0, 0, 0,
    };
    updateBlePacket(&appData.bluetooth.pressureB, pressureBData);

    // Byte 6's temperature marker before the first cycle, for the same reason; the counters and
    // masks start at zero, which is honest.
    guint8 pressureStatusData[CAN_DATA_SIZE] = {
        0, 0, 0, 0, 0, 0, (guint8)(gint8)PRESSURE_SENSOR_TEMP_INVALID_C, MUX_CHANNEL_NONE,
    };
    updateBlePacket(&appData.bluetooth.pressureStatus, pressureStatusData);
}
