#include <signal.h>

#include "dataContracts.hpp"
#include "appData.cxx"
#include "helpers.cxx"
#include "config.cxx"
#include "sessionWriter.cxx"
#include "blePackets.cxx"
#include "supplyMonitor.cxx"
#include "oneWireProbes.cxx"
#include "bme280Sensor.cxx"
#include "pressureSensors.cxx"
#include "raceChronoBle.cxx"

void printUsage(const gchar* program) {
    g_print(
        "Usage: %s [--enroll [--reset]]\n"
        "\n"
        "  (no arguments)     log a session\n"
        "  --enroll           bind DS18B20 ROM IDs to temp0-temp3 by sequential plug-in\n"
        "  --enroll --reset   discard the whole binding store first and enroll from temp0\n"
        "\n"
        "Plug the probes in ONE AT A TIME, lowest channel first, allowing ~10 s for the bus\n"
        "to rescan. Two unbound probes in one scan are refused rather than guessed at.\n"
        "Enrollment keeps sampling after the fourth bind, so warming one probe by hand and\n"
        "watching which channel moves confirms the map in situ; stop it with SIGTERM.\n"
        "\n"
        "Binding never happens during a logging run: outside enrollment an unknown ROM ID is\n"
        "invalid data and a logged event, so a dropout mid-session cannot silently re-label a\n"
        "channel.\n",
        program);
}

gboolean parseArguments(int argc, char* argv[]) {
    for (auto i = 1; i < argc; i++) {
        if (g_str_equal(argv[i], "--enroll")) { appData.mode = ModeEnroll; continue; }
        if (g_str_equal(argv[i], "--reset")) { appData.resetRequested = TRUE; continue; }
        if (g_str_equal(argv[i], "--help") || g_str_equal(argv[i], "-h")) { return FALSE; }

        g_printerr("Unknown argument: %s\n", argv[i]);
        return FALSE;
    }

    if (appData.resetRequested && appData.mode != ModeEnroll) {
        g_printerr("--reset is only meaningful with --enroll\n");
        return FALSE;
    }

    return TRUE;
}

int main(int argc, char* argv[])
{
    if (!parseArguments(argc, argv)) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    g_message("KnurLogger starting, app version: %s, mode: %s", APP_VERSION,
        appData.mode == ModeEnroll ? "enroll" : "log");

    appData.bootTimeUs = getSystemBootWallTimeUs();
    appData.sessionStartBootUs = getBootTimeUs();

    loadConfig();

    appData.session.queue = g_async_queue_new();
    if (!openSession()) { logErrorAndKill("Could not open a session file, exiting..."); }

    // Set before any worker starts, so a signal during startup is not lost.
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);

    auto* sessionWriterThread = g_thread_new("sessionWriter", sessionWriterLoop, NULL);
    writeSessionHeader();

    initialiseBlePackets();

    appData.producersRunning++;
    auto* supplyMonitorThread = g_thread_new("supplyMonitor", supplyMonitorLoop, NULL);

    appData.producersRunning++;
    auto* oneWireProbesThread = g_thread_new("oneWireProbes", oneWireProbesLoop, NULL);

    appData.producersRunning++;
    auto* bme280SensorThread = g_thread_new("bme280Sensor", bme280SensorLoop, NULL);

    appData.producersRunning++;
    auto* pressureSensorsThread = g_thread_new("pressureSensors", pressureSensorsLoop, NULL);

    appData.producersRunning++;
    auto* raceChronoBleThread = g_thread_new("raceChronoBle", raceChronoBleLoop, NULL);

    g_thread_join(supplyMonitorThread);
    g_thread_join(oneWireProbesThread);
    g_thread_join(bme280SensorThread);
    g_thread_join(pressureSensorsThread);
    g_thread_join(raceChronoBleThread);
    // Joined last: it owns the session fd and its final act is to drain the queue and fsync, so
    // every other worker's last record has to be enqueued before it stops.
    g_thread_join(sessionWriterThread);

    if (appData.session.fd >= 0) { close(appData.session.fd); }
    g_async_queue_unref(appData.session.queue);

    g_message("KnurLogger terminated, session file: %s", appData.session.filePath.c_str());

    return 0;
}
