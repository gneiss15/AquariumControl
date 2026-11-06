// TJsonFeedTime.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonFeedTime) || defined(Do_TJsonFeedTimes) || defined(Do_THttpTimePayload) || defined(Do_THttpTimeReply)
JD_TUPLE_BEGIN( TJsonFeedTime, "FeedTimes.json row" )
  JD( TGNString, Time, Required, TimeOfDay, "Local HH:MM", Live )
  JD( uint8_t, Days, Required, Any, "Weekdays and active flag", Live )
  JD( uint8_t, Portions, Required, Range(1,9), "Portion count", Live )
  JD( TJsonValue, Comment, Optional, Any, "Optional editor comment", Live )
JD_END( TJsonFeedTime )

#endif
