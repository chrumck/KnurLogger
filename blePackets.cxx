#pragma once

#include "sessionWriter.cxx"

// The RaceChrono DIY CAN-Bus device protocol's packet primitives, kept separate from the BLE
// worker so that the worker which OWNS a reading is the one that packs it. In a single translation
// unit that also settles the include order: this comes before every producer, the BLE worker comes
// after them.
//
// Two properties of the protocol are easy to get backwards, and both are transcribed from two
// bench-proven implementations rather than designed here — KnurDash's bluetoothWorker.c and the
// ESP32 rig in ../ndLouvers/step0b-rig/racechrono_ble_test/:
//
//   1. The packet ID is the ONE little-endian field. Every payload field is big-endian.
//   2. RaceChrono, not the logger, chooses the notify rate — it writes a filter command asking for
//      all frames or one frame at an interval in milliseconds. So "10 Hz" is a request the phone
//      makes, and the logger's job is to have fresh data ready when it fires.

void updateBlePacket(BlePacket* packet, const guint8* data) {
    g_mutex_lock(&packet->lock);
    memcpy(packet->data, data, CAN_DATA_SIZE);
    packet->updatedBootUs = getBootTimeUs();
    // Cleared so the notify timer knows there is something new to send. A packet whose data has
    // not changed is deliberately NOT re-sent: the logger never re-transmits a stale value as
    // though it were new, which is commissioning item 4's rule applied to the BLE path.
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
    case 0: return &appData.bluetooth.temp;
    case 1: return &appData.bluetooth.thermalStatus;
    case 2: return &appData.bluetooth.supply;
    default: return NULL;
    }
}

#define forEachBlePacket(_packet) \
    for (guint _i = 0; BlePacket* _packet = getAllPackets(_i); _i++)


void initialiseBlePackets() {
    appData.bluetooth.temp.packetId = PACKET_ID_TEMP;
    appData.bluetooth.thermalStatus.packetId = PACKET_ID_THERMAL_STATUS;
    appData.bluetooth.supply.packetId = PACKET_ID_SUPPLY;

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
}
