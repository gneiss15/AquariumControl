// TUdpLastError.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TUdpLastError) || defined(Do_TUdpStatusData) || defined(Do_TUdpStatusMessage) || defined(Do_TWsStatusEvent) || defined(Do_TJsonCachedStatus) || defined(Do_TSseStatusBatch)
JD_BEGIN( TUdpLastError, "Most recent error recorded by the device, with its timestamp and message." )
  JD( TUnixTime, Time, Optional, Any, "", Live )
  JD( TGNString, Message, Optional, Any, "Recorded error text", Live )
JD_END( TUdpLastError )

#endif
