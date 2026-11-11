// TJsonCachedStatus.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonCachedStatus) || defined(Do_TSseStatusBatch)
JD_BEGIN( TJsonCachedStatus, "Dashboard view of the most recently received device status, including reception age and online state. Returned by the HTTP status API and pushed to Dashboard browsers over SSE." )
  JD( TChannelId, ID, Required, ServiceAddress, "", Live )
  JD( bool, Online, Required, Any, "Online state at the Dashboard", Live )
  JD( uint64_t, Rev, Required, Any, "Cache update revision, null after removal", Live )
  JD( uint32_t, AgeMs, Optional, Any, "Age of the last received UDP status", Live )
  JD( TUdpStatusData, Status, Required, Any, "Last known status, not necessarily fresh", Live )
JD_END( TJsonCachedStatus )

#endif
