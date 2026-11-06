// FeederTimeSync.h, Version: 1.02
#pragma once

#include <GnTimeSync.h>

extern TGnTimeSyncConfig const FeederTimeSyncConfig;
String FeederTimeSyncApply( char const * Content );

String FeederTimeSyncFunctionConfig( char const * Envelope );
