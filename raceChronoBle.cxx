#pragma once

#include "blePackets.cxx"

// The binc plumbing for the RaceChrono DIY CAN-Bus device protocol. blePackets.cxx owns the wire
// format; this file owns the adapter, the advertisement, the GATT application and the notify timers.
//
// KnurDash's bluetoothWorker.c is the reference for the protocol, NOT for these signatures: the
// bluez_inc this repository pins adds `mtu` and `offset` to the characteristic read and write
// callbacks, which KnurDash's older pin does not have. Copying its callback declarations verbatim
// fails to compile, and silently taking the wrong shape would be worse.

void onPoweredStateChanged(Adapter* adapter, gboolean state) {
    g_message("Bluetooth: adapter powered '%s'", state ? "on" : "off");
    writeEventRecord("info", std::format("BLE adapter powered {}", state ? "on" : "off"));

    if (state) { return; }

    // If this loops without ever reaching "powered on", the radio is almost certainly soft-blocked
    // at the rfkill layer: rfkill sits BELOW BlueZ, so no amount of powering on from here can
    // clear it. `sudo rfkill unblock bluetooth` is the fix, and it persists.
    g_message("Bluetooth: powering adapter up");
    binc_adapter_power_on(adapter);
}

void onCentralStateChanged(Adapter* adapter, Device* device) {
    auto* address = binc_device_get_address(device);
    auto state = binc_device_get_connection_state(device);

    g_message("Bluetooth: central %s is %s", address, binc_device_get_connection_state_name(device));

    if (state == BINC_CONNECTED) {
        appData.bluetooth.isConnected = true;
        writeEventRecord("info", std::format("BLE central connected: {}", address));
        binc_adapter_stop_advertising(adapter, appData.bluetooth.adv);
        return;
    }

    if (state == BINC_DISCONNECTED) {
        appData.bluetooth.isConnected = false;
        appData.bluetooth.isNotifying = false;
        // Logged because it is otherwise invisible: from RaceChrono's side a dropped link looks
        // like every channel holding its last value forever. This record and the gap around it are
        // the only evidence that the data stopped rather than went flat.
        writeEventRecord("warning", std::format("BLE central disconnected: {}", address));
        binc_adapter_start_advertising(adapter, appData.bluetooth.adv);
    }
}

const char* onCharRead(const Application* app, const char* address,
    const char* serviceId, const char* charId, const guint16 mtu, const guint16 offset) {
    if (!g_str_equal(serviceId, BLE_SERVICE_ID) || !g_str_equal(charId, BLE_CHAR_ID_MAIN)) {
        return BLUEZ_ERROR_NOT_PERMITTED;
    }

    // Serve the oldest unsent packet, so a polling central sees every frame in turn rather than
    // whichever one happens to be freshest.
    BlePacket* toSend = NULL;
    forEachBlePacket(packet) {
        if (packet->wasSent) { continue; }
        if (toSend != NULL && packet->updatedBootUs >= toSend->updatedBootUs) { continue; }
        toSend = packet;
    }

    if (toSend == NULL) { return NULL; }

    auto* bytes = buildPacketBytes(toSend);
    binc_application_set_char_value(appData.bluetooth.app, serviceId, charId, bytes);

    return NULL;
}

gboolean sendPacketToBt(gpointer data) {
    auto* packet = (BlePacket*)data;

    if (appData.shutdownRequested) {
        packet->notifySourceId = 0;
        return G_SOURCE_REMOVE;
    }

    if (!appData.bluetooth.isNotifying || packet->wasSent) { return G_SOURCE_CONTINUE; }

    auto* bytes = buildPacketBytes(packet);
    binc_application_notify(appData.bluetooth.app, BLE_SERVICE_ID, BLE_CHAR_ID_MAIN, bytes);
    g_byte_array_free(bytes, TRUE);

    appData.bluetooth.notifiesSent++;
    packet->notifiesSent++;

    return G_SOURCE_CONTINUE;
}

void addNotifySource(GMainContext* context, BlePacket* packet, guint intervalMs) {
    auto* source = g_timeout_source_new(intervalMs);
    g_source_set_callback(source, sendPacketToBt, packet, NULL);
    packet->notifySourceId = g_source_attach(source, context);
    g_source_unref(source);
}

void removeNotifySource(GMainContext* context, BlePacket* packet) {
    if (packet->notifySourceId == 0) { return; }

    auto* source = g_main_context_find_source_by_id(context, packet->notifySourceId);
    if (source != NULL) { g_source_destroy(source); }
    packet->notifySourceId = 0;
}

// Recorded once per link. The negotiated MTU bounds what a single notify can carry, and a 12-byte
// packet fits even the 23-byte ATT default, so this is evidence for item 5a's link qualification
// rather than a constraint to design around.
void logNegotiatedMtu(guint16 mtu) {
    static guint16 lastLoggedMtu = 0;
    if (mtu == lastLoggedMtu) { return; }

    lastLoggedMtu = mtu;
    g_message("Bluetooth: negotiated MTU %u", mtu);
    writeEventRecord("info", std::format(
        "BLE negotiated MTU {}, packet is {} bytes", mtu, CAN_FRAME_ID_LENGTH + CAN_DATA_SIZE));
}

const char* onCharWrite(const Application* app, const char* address, const char* serviceId,
    const char* charId, GByteArray* received, const guint16 mtu, const guint16 offset) {
    if (!g_str_equal(serviceId, BLE_SERVICE_ID) || !g_str_equal(charId, BLE_CHAR_ID_FILTER)) {
        return BLUEZ_ERROR_NOT_PERMITTED;
    }

    // EVERY command is recorded, raw, BEFORE it is validated. The first version refused
    // unrecognised commands silently - no warning, no record - and a subscription that vanishes
    // without evidence is indistinguishable from one the phone never sent. That cost a road test:
    // the session file showed `deny all` followed by nothing at all, and there was no way to tell
    // whether RaceChrono had gone quiet or this logger had refused what it sent.
    auto commandHex = toHexString(received->data, received->len);
    writeSessionRecord("bleFilter", std::format(
        "\"len\":{},\"raw\":\"{}\"", received->len, commandHex));

    if (received->len < 1) { return BLUEZ_ERROR_REJECTED; }

    auto command = received->data[0];

    // Lengths are MINIMA, not equalities, matching the reference DIY implementation. Refusing a
    // command longer than expected buys nothing and would refuse a future protocol revision that
    // appends a field - and a refusal here costs the whole primary data path.
    auto isDenyAll = command == RACECHRONO_DENY_ALL;
    auto isAllowAll = command == RACECHRONO_ALLOW_ALL && received->len >= 3;
    auto isAllowSingle = command == RACECHRONO_ALLOW_SINGLE && received->len >= 7;

    if (!isDenyAll && !isAllowAll && !isAllowSingle) {
        g_warning("Bluetooth: unrecognised filter command [%s], ignoring", commandHex.c_str());
        writeEventRecord("warning", std::format(
            "BLE filter command not understood, ignored: [{}]", commandHex));
        // Ignored rather than refused, deliberately. An ATT error is a protocol-level failure the
        // central may take as "this device is broken" and abandon - taking with it the commands it
        // had not sent yet, and RaceChrono sends its subscriptions as a burst.
        return NULL;
    }

    auto* context = g_main_loop_get_context(appData.bluetooth.mainLoop);

    logNegotiatedMtu(mtu);

    if (isDenyAll) {
        g_message("Bluetooth: RaceChrono requested deny all");
        writeEventRecord("info", "BLE filter: deny all");
        forEachBlePacket(packet) { removeNotifySource(context, packet); }
        return NULL;
    }

    guint intervalMs = received->data[1] << 8 | received->data[2];
    if (intervalMs < BLE_NOTIFY_INTERVAL_MIN_MS || intervalMs > BLE_NOTIFY_INTERVAL_MAX_MS) {
        // Clamped rather than refused, for the reason above: notifying at a rate the phone did not
        // ask for is better than losing the subscription, and the requested value is on record.
        auto clamped = std::clamp<guint>(intervalMs,
            BLE_NOTIFY_INTERVAL_MIN_MS, BLE_NOTIFY_INTERVAL_MAX_MS);
        g_warning("Bluetooth: notify interval %u ms out of range, using %u ms", intervalMs, clamped);
        writeEventRecord("warning", std::format(
            "BLE filter: interval {} ms out of range, clamped to {} ms", intervalMs, clamped));
        intervalMs = clamped;
    }

    if (isAllowAll) {
        g_message("Bluetooth: RaceChrono requested all frames at %u ms", intervalMs);
        // The requested interval is recorded because it IS the sample rate of the primary data
        // path — item 4's 10 Hz target is met or missed by what the phone asks for here and by
        // whether the link sustains it, not by anything the logger decides.
        writeEventRecord("info", std::format("BLE filter: allow all at {} ms", intervalMs));

        forEachBlePacket(packet) {
            removeNotifySource(context, packet);
            addNotifySource(context, packet, intervalMs);
        }
        return NULL;
    }

    guint32 packetId = received->data[3] << 24 | received->data[4] << 16
        | received->data[5] << 8 | received->data[6];

    BlePacket* requested = NULL;
    forEachBlePacket(packet) { if (packet->packetId == packetId) { requested = packet; } }

    if (requested == NULL) {
        // Ignored, NOT refused. RaceChrono sends one allow-single per configured channel as a
        // burst, so answering one of them with an ATT error can lose every subscription behind it
        // - which presents as a few channels updating and the rest frozen. A channel definition
        // for a packet this logger does not publish is the owner's business rather than an error,
        // and the command is on record above either way.
        g_warning("Bluetooth: packet 0x%X requested but not published here, ignoring", packetId);
        writeEventRecord("warning", std::format(
            "BLE filter: 0x{:X} requested but not published by this logger, ignored", packetId));
        return NULL;
    }

    g_message("Bluetooth: RaceChrono requested packet 0x%X at %u ms", packetId, intervalMs);
    writeEventRecord("info", std::format("BLE filter: allow 0x{:X} at {} ms", packetId, intervalMs));

    removeNotifySource(context, requested);
    addNotifySource(context, requested, intervalMs);

    return NULL;
}

void onCharStartNotify(const Application* app, const char* serviceId, const char* charId) {
    if (!g_str_equal(serviceId, BLE_SERVICE_ID) || !g_str_equal(charId, BLE_CHAR_ID_MAIN)) { return; }

    appData.bluetooth.isNotifying = true;
    g_message("Bluetooth: notify start");
    writeEventRecord("info", "BLE notify start");
}

void onCharStopNotify(const Application* app, const char* serviceId, const char* charId) {
    if (!g_str_equal(serviceId, BLE_SERVICE_ID) || !g_str_equal(charId, BLE_CHAR_ID_MAIN)) { return; }

    appData.bluetooth.isNotifying = false;
    g_message("Bluetooth: notify stop");
    writeEventRecord("info", "BLE notify stop");
}

gboolean stopBleWorker(gpointer _) {
    if (!appData.shutdownRequested) { return G_SOURCE_CONTINUE; }

    auto* context = g_main_loop_get_context(appData.bluetooth.mainLoop);
    while (g_main_context_pending(context)) { g_main_context_iteration(context, TRUE); }

    auto* adapter = appData.bluetooth.adapter;

    if (adapter != NULL) {
        auto* connected = binc_adapter_get_connected_devices(adapter);
        for (auto* iterator = connected; iterator; iterator = iterator->next) {
            auto* device = (Device*)iterator->data;
            g_message("Bluetooth: disconnecting central %s", binc_device_get_address(device));
            binc_device_disconnect(device);
        }
        g_list_free(connected);
    }

    if (appData.bluetooth.app != NULL) {
        binc_adapter_unregister_application(adapter, appData.bluetooth.app);
        binc_application_free(appData.bluetooth.app);
        appData.bluetooth.app = NULL;
    }

    if (appData.bluetooth.adv != NULL) {
        binc_adapter_stop_advertising(adapter, appData.bluetooth.adv);
        binc_advertisement_free(appData.bluetooth.adv);
        appData.bluetooth.adv = NULL;
    }

    if (adapter != NULL) {
        binc_adapter_free(adapter);
        appData.bluetooth.adapter = NULL;
    }

    if (appData.bluetooth.dbusConn != NULL) {
        g_dbus_connection_close_sync(appData.bluetooth.dbusConn, NULL, NULL);
        g_object_unref(appData.bluetooth.dbusConn);
        appData.bluetooth.dbusConn = NULL;
    }

    g_main_loop_quit(appData.bluetooth.mainLoop);

    return G_SOURCE_REMOVE;
}

gpointer raceChronoBleLoop(gpointer _) {
    g_message("Bluetooth: starting");

    log_set_level(LOG_WARN);

    // Its own context and loop, pushed thread-default, so binc's D-Bus work and the notify timers
    // all run on this thread. This is KnurDash's shape and it is why main() needs no main loop of
    // its own - it stays iSitePiLogger's plain thread-join.
    //
    // THE PUSH MUST COME BEFORE THE D-BUS CONNECTION AND THE ADAPTER, and it did not until
    // 2026-09-10. GDBus binds each signal subscription to whatever context is thread-default at
    // the moment the subscription is made, so an adapter created first subscribes against the
    // GLOBAL default context - which nothing in this process iterates, main() being a plain
    // thread-join. The adapter callbacks then never fire, silently: no powered-state changes and
    // NO CENTRAL CONNECT OR DISCONNECT. Every session file written before that date is missing its
    // BLE connect and disconnect events for this reason, not because nothing ever connected.
    auto* workerContext = g_main_context_new();
    g_main_context_push_thread_default(workerContext);

    appData.bluetooth.dbusConn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if (appData.bluetooth.dbusConn == NULL) {
        g_critical("Bluetooth: could not reach the system D-Bus, no BLE this session");
        writeEventRecord("error", "BLE unavailable: no system D-Bus");
        g_main_context_pop_thread_default(workerContext);
        g_main_context_unref(workerContext);
        appData.producersRunning--;
        return NULL;
    }

    appData.bluetooth.adapter = binc_adapter_get_default(appData.bluetooth.dbusConn);
    if (appData.bluetooth.adapter == NULL) {
        // Not fatal to the session, deliberately. BLE is the primary data path, but the SD record
        // is what makes a session survivable without it, and a logger that refuses to run because
        // the radio is missing would throw away the run as well as the link.
        g_critical("Bluetooth: no adapter found, no BLE this session");
        writeEventRecord("error", "BLE unavailable: no adapter");
        g_main_context_pop_thread_default(workerContext);
        g_main_context_unref(workerContext);
        appData.producersRunning--;
        return NULL;
    }

    g_message("Bluetooth: adapter '%s'", binc_adapter_get_path(appData.bluetooth.adapter));

    appData.bluetooth.mainLoop = g_main_loop_new(workerContext, FALSE);

    auto* stopSource = g_timeout_source_new(BLE_SHUTDOWN_POLL_MS);
    g_source_set_callback(stopSource, stopBleWorker, NULL, NULL);
    g_source_attach(stopSource, workerContext);
    g_source_unref(stopSource);

    binc_adapter_set_powered_state_cb(appData.bluetooth.adapter, &onPoweredStateChanged);
    if (!binc_adapter_get_powered_state(appData.bluetooth.adapter)) {
        binc_adapter_power_on(appData.bluetooth.adapter);
    }

    binc_adapter_set_remote_central_cb(appData.bluetooth.adapter, &onCentralStateChanged);

    auto* advServiceUuids = g_ptr_array_new();
    g_ptr_array_add(advServiceUuids, (gpointer)BLE_SERVICE_ID);

    appData.bluetooth.adv = binc_advertisement_create();
    binc_advertisement_set_local_name(appData.bluetooth.adv, appConfig.bleDeviceName);
    binc_advertisement_set_services(appData.bluetooth.adv, advServiceUuids);
    g_ptr_array_free(advServiceUuids, TRUE);
    binc_adapter_start_advertising(appData.bluetooth.adapter, appData.bluetooth.adv);

    appData.bluetooth.app = binc_create_application(appData.bluetooth.adapter);
    binc_application_add_service(appData.bluetooth.app, BLE_SERVICE_ID);
    binc_application_add_characteristic(appData.bluetooth.app, BLE_SERVICE_ID, BLE_CHAR_ID_MAIN,
        GATT_CHR_PROP_READ | GATT_CHR_PROP_NOTIFY);
    binc_application_add_characteristic(appData.bluetooth.app, BLE_SERVICE_ID, BLE_CHAR_ID_FILTER,
        GATT_CHR_PROP_WRITE);

    binc_application_set_char_read_cb(appData.bluetooth.app, &onCharRead);
    binc_application_set_char_write_cb(appData.bluetooth.app, &onCharWrite);
    binc_application_set_char_start_notify_cb(appData.bluetooth.app, &onCharStartNotify);
    binc_application_set_char_stop_notify_cb(appData.bluetooth.app, &onCharStopNotify);

    binc_adapter_register_application(appData.bluetooth.adapter, appData.bluetooth.app);

    writeEventRecord("info", std::format(
        "BLE advertising as '{}', service {}", appConfig.bleDeviceName, BLE_SERVICE_ID));

    appData.bluetooth.isRunning = true;
    g_message("Bluetooth: advertising as '%s'", appConfig.bleDeviceName);

    g_main_loop_run(appData.bluetooth.mainLoop);

    g_main_loop_unref(appData.bluetooth.mainLoop);
    appData.bluetooth.mainLoop = NULL;

    appData.bluetooth.isRunning = false;
    g_message("Bluetooth: shutting down, %lu notifications sent",
        (unsigned long)appData.bluetooth.notifiesSent);

    writeEventRecord("info", std::format(
        "BLE stopped, {} notifications sent", (unsigned long)appData.bluetooth.notifiesSent));

    appData.producersRunning--;

    return NULL;
}
