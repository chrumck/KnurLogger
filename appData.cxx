#pragma once

#include "dataContracts.hpp"

AppConfig appConfig = {};

AppData appData = {
    .mode = ModeLog,
    .resetRequested = FALSE,
    .shutdownRequested = false,
    .session = { .fd = -1 },
};
