// TJsonSchedule.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonSchedule) || defined(Do_THttpTimePayload) || defined(Do_THttpTimeReply)
JD_BEGIN( TJsonSchedule, "Switching schedule stored in the device schedule file or exchanged during synchronization. It specifies when outputs change state and which repeating intervals run. Synchronization may also carry the associated channel configuration." )
  JD( TJsonSwitchTimes, SwitchTimes, Optional, Any, "Scheduled output states", Live )
  JD( TJsonIntervals, Intervals, Optional, Any, "Repeating switching intervals", Live )
  JD( TJsonChannels, Chs, Optional, Any, "Channel configuration and profiles, if included in the sync update", Live )
JD_END( TJsonSchedule )

#endif
