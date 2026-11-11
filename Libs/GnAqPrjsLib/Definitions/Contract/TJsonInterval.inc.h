// TJsonInterval.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonInterval) || defined(Do_TJsonIntervals) || defined(Do_TJsonSchedule) || defined(Do_THttpTimePayload) || defined(Do_THttpTimeReply)
JD_TUPLE_BEGIN( TJsonInterval, "Switching interval row" )
  JD( TGNString, Start, Required, TimeOfDay, "Local start HH:MM", Live )
  JD( TGNString, Period, Required, PositiveDuration, "Cycle duration HH:MM", Live )
  JD( TGNString, OnDuration, Required, DurationBefore(Period), "On duration HH:MM", Live )
  JD( uint8_t, Days, Required, Any, "Weekdays and active flag", Live )
  JD( TChannelId, Ch, Required, OutputAddress, "", Live )
  JD( TJsonValue, Comment, Optional, Any, "Optional comment", Live )
JD_END( TJsonInterval )

#endif
