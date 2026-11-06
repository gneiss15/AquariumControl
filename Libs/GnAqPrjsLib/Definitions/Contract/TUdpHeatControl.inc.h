// TUdpHeatControl.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TUdpHeatControl) || defined(Do_TUdpStatusData) || defined(Do_TUdpStatusMessage) || defined(Do_TWsStatusEvent) || defined(Do_TJsonCachedStatus) || defined(Do_TSseStatusBatch)
JD_BEGIN( TUdpHeatControl, "Heating coordination data used by the main and floor outputs." )
  JD_CONST( uint32_t, Ver, 1 )
  JD( bool, Active, Required, Any, "Enables PI control of the floor output. False withdraws the request.", Live )
  JD( TGNString, Prof, Required, Any, "Name of the heating profile", Live )
  JD( TChannelId, FloorEndpoint, Required, OutputAddress, "", Live )
  JD( bool, FloorRequest, Required, Any, "Requested floor output state. Null denotes no usable request.", Live )
JD_END( TUdpHeatControl )

#endif
