// main.cpp, Version: 1.01
#include <GnBasics.h>
#include "sdkconfig.h"
#include "Libs/Common/TmrSw/TmrSwMain.inc.h"

static_assert(CONFIG_IDF_TARGET_ESP32, "A8T requires ESP32");
static_assert(CONFIG_PARTITION_TABLE_OFFSET == 0x8000, "A8T table offset must match its existing loader");
// Arduino's app_main starts the existing setup/loop task on the configured core.
