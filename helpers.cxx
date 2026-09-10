#pragma once

#include "dataContracts.hpp"
#include "appData.cxx"

#define logErrorAndKill(...) g_critical(__VA_ARGS__), exit(EXIT_FAILURE)

#define logError(...) appData.shutdownRequested = true, g_critical(__VA_ARGS__)

#define getLength(value) (sizeof(value) / sizeof(value[0]))

// CLOCK_TAI for the wall clock, per iSitePiLogger, so sample timestamps carry no leap-second
// discontinuity.
guint64 getCurrentTimeUs() {
    struct timespec timeSpec;
    clock_gettime(CLOCK_TAI, &timeSpec);
    return (guint64)timeSpec.tv_sec * 1000000 + timeSpec.tv_nsec / 1000;
}

// CLOCK_BOOTTIME for everything that measures an interval. The car has no NTP and the Pi has no
// RTC, so the wall clock is wrong-but-plausible and can also be stepped at the bench; a stepped
// clock would stall or storm a cadence gated on it. Every record carries both, which is also what
// the plan's elapsed-since-boot fields need.
guint64 getBootTimeUs() {
    struct timespec timeSpec;
    clock_gettime(CLOCK_BOOTTIME, &timeSpec);
    return (guint64)timeSpec.tv_sec * 1000000 + timeSpec.tv_nsec / 1000;
}

guint64 getSystemBootWallTimeUs() {
    struct timespec bootTs, taiTs;
    if (clock_gettime(CLOCK_BOOTTIME, &bootTs) != 0 || clock_gettime(CLOCK_TAI, &taiTs) != 0) {
        logErrorAndKill("Failed to get system boot time");
    }
    return (guint64)taiTs.tv_sec * 1000000 + taiTs.tv_nsec / 1000
        - (guint64)bootTs.tv_sec * 1000000 - bootTs.tv_nsec / 1000;
}

std::string getIsoTimestamp(guint64 timeUs) {
    chr::sys_time<chr::microseconds> timePoint{ chr::microseconds{timeUs} };
    return std::format("{:%FT%H:%M:%S}Z", timePoint);
}

std::string trim(const std::string& input) {
    auto start = std::find_if_not(input.begin(), input.end(), [](unsigned char c) { return std::isspace(c); });
    auto end = std::find_if_not(input.rbegin(), input.rend(), [](unsigned char c) { return std::isspace(c); }).base();
    return start < end ? std::string(start, end) : "";
}

std::string escapeJson(const std::string& input) {
    std::string result;
    result.reserve(input.size() + 8);
    for (auto c : input) {
        switch (c) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if ((unsigned char)c < 0x20) { result += std::format("\\u{:04x}", (int)(unsigned char)c); }
            else { result += c; }
        }
    }
    return result;
}

std::string toHexString(const guint8* data, guint length) {
    std::string result;
    result.reserve(length * 3);
    for (guint i = 0; i < length; i++) {
        result += std::format("{}{:02X}", i == 0 ? "" : " ", data[i]);
    }
    return result;
}

std::optional<std::string> readWholeFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) { return std::nullopt; }

    std::stringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) { return std::nullopt; }

    return buffer.str();
}

// vcgencmd costs ~3 ms per call, measured, so 1 Hz is free. It needs /dev/vcio, which the `video`
// group grants; a failure here is reported as a missing reading rather than as a fatal error,
// because supply telemetry must never be able to take the logger down.
std::optional<std::string> runCommand(const gchar* const* argv) {
    gchar* stdoutText = NULL;
    gint exitStatus = 0;
    GError* error = NULL;

    if (!g_spawn_sync(NULL, (gchar**)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
        &stdoutText, NULL, &exitStatus, &error)) {
        if (appConfig.verboseMode) { g_warning("runCommand: spawn failed: %s", error->message); }
        g_clear_error(&error);
        return std::nullopt;
    }

    std::optional<std::string> result = std::nullopt;
    if (exitStatus == 0 && stdoutText != NULL) { result = trim(stdoutText); }

    g_free(stdoutText);
    return result;
}

void requestShutdown(const gchar* reason) {
    if (appData.shutdownRequested) { return; }
    g_message("Shutdown requested: %s", reason);
    appData.shutdownRequested = true;
}

// Deliberately does nothing but set the flag. g_message and friends are not async-signal-safe, and
// SIGTERM arrives here from systemd on every ordinary stop, so the logging happens in main once
// the workers have joined.
void onSignal(gint signalNumber) {
    appData.shutdownRequested = true;
}
