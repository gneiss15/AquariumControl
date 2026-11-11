// TJsonSwitchTime.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonSwitchTime) || defined(Do_TJsonSwitchTimes) || defined(Do_TJsonSchedule) || defined(Do_THttpTimePayload) || defined(Do_THttpTimeReply)
JD_BEGIN( TJsonSwitchTime, "Scheduled output state at a local time. MultiTimer requires a channel. TimerSwitch uses its single output when the channel is omitted." )
  JD( TGNString, Time, Required, TimeOfDay, "Local HH:MM", Live )
  JD( uint8_t, Days, Required, Any, "Weekdays and active flag", Live )
  JD( TChannelId, Ch, Optional, OutputAddress, "", Live )
  JD( bool, State, Required, Any, "Requested output state", Live )
  JD( TJsonValue, Comment, Optional, Any, "Optional comment", Live )
JD_END( TJsonSwitchTime )

#endif
