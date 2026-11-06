// TUdpRequestMessage.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TUdpRequestMessage)
JD_MESSAGE( TUdpRequestMessage, Request, TMsgType::Request )
JD_BEGIN( TUdpRequestMessage, "Requests a current Status message from each listed device. An empty ID list requests status from all devices." )
  JD_CONST( uint32_t, Ver, 3 )
  JD_CONST( TMsgType, Type, TMsgType::Request )
  JD( ArrayOf(TChannelId), ID, Required, All(Count(0,64),Each(ChannelAddress)), "Device IDs to query. An empty list requests status from all devices.", Live )
JD_END( TUdpRequestMessage )

#endif
