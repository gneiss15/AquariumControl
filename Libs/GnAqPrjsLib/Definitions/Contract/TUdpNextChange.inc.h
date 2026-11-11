// TUdpNextChange.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TUdpNextChange) || defined(Do_TUdpChannel) || defined(Do_TUdpStatusData) || defined(Do_TUdpStatusMessage) || defined(Do_TWsStatusEvent) || defined(Do_TJsonCachedStatus) || defined(Do_TSseStatusBatch)
JD_BEGIN( TUdpNextChange, "Next scheduled channel event, such as switching an output, ending an override or dispensing food." )
  JD( TUnixTime, Time, Optional, Any, "", Live )
  JD( bool, State, Optional, Any, "Output state after the event, or true for a scheduled feeding", Live )
  JD( TNextChangeSource, Src, Optional, Any, "Origin of the scheduled event", Live )
  JD( uint32_t, Portions, Optional, Any, "Number of portions for the scheduled feeding", Live )
JD_END( TUdpNextChange )

#endif
