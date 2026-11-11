// TJsonFeedbackConfiguration.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonFeedbackConfiguration) || defined(Do_THttpFeedbackDocument) || defined(Do_THttpFeedbackWrite)
JD_BEGIN( TJsonFeedbackConfiguration, "Contents of the Dashboard feedback-monitor configuration file `data/FeedbackMonitors.json`, relative to the Dashboard server root directory. In the current Pi installation this is `/home/pi/Prjs/Dashboard/data/FeedbackMonitors.json`. The project-root `FeedbackMonitors.json` provides initial defaults until a runtime configuration exists. The same structure is used by the feedback configuration HTTP interface." )
  JD_CONST( uint32_t, Ver, 1 )
  JD( ArrayOf(TJsonFeedbackRule), Monitors, Required, Count(0,32), "Monitor rules; identifiers must be unique", Live )
JD_END( TJsonFeedbackConfiguration )

#endif
