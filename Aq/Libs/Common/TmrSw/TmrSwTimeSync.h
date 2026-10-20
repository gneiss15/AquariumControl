// TmrSwTimeSync.h, Version: 1.05
#pragma once

#include <GnTimeSync.h>

extern TGnTimeSyncConfig const TmrSwTimeSyncConfig;
bool TmrSwRecoverConfiguration(void);
String TmrSwTimeSyncApply( char const * Content );

String TmrSwTimeSyncFunctionConfig( char const * Envelope );
