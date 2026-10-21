// FeederTimeSync.cpp, Version: 1.06
#include <GnBasics.h>
#include "FeederTimeSync.h"

#include "FeedTimes.h"
#include "FeederConfig.h"
#include <GnFunctionConfig.h>

TGnTimeSyncConfig const FeederTimeSyncConfig =
 {
  "/FeedTimes.sync.json",
  "/FeedTimes.json",
  "FeedTimes",
  "Feeder",
  1000,
  nullptr,
  nullptr,
  FeederTimeSyncFunctionConfig
 };

String FeederTimeSyncApply( char const * Content )
 {
  return FeedTimes.Set( Content );
 }

String FeederTimeSyncFunctionConfig( char const * Envelope )
 {
  return GnApplyFunctionConfig( Envelope, FeederConfig,
    [](){ return FeederConfigInstance.Save(); } );
 }
