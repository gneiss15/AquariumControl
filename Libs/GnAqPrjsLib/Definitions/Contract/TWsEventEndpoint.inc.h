// TWsEventEndpoint.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TWsEventEndpoint) || defined(Do_THttpDevice)
JD_BEGIN( TWsEventEndpoint, "WebSocket endpoint for receiving device status. The client connects to the advertised URL and receives status events sent by the device." )
  JD( TGNString, Url, Required, WebSocketUrl, "Complete WebSocket URL including the port when required", Live )
  JD( uint32_t, PollingFallbackSec, Required, Any, "Polling interval when the WebSocket connection is unavailable", Live )
JD_END( TWsEventEndpoint )

#endif
