// TWsStatusEvent.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TWsStatusEvent)
JD_BEGIN( TWsStatusEvent, "Device status update sent over the WebSocket status connection to subscribed clients. Sequence identifies newer events within a server run." )
  JD( uint64_t, Seq, Required, Any, "Sequence within this server run", Live )
  JD( uint64_t, Timestamp, Required, Any, "Server UTC timestamp in seconds", Live )
  JD( TUdpStatusData, Status, Required, Any, "Current complete device status", Live )
JD_END( TWsStatusEvent )

#endif
