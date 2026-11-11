// TUdpInfoRequestMessage.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TUdpInfoRequestMessage)
JD_MESSAGE( TUdpInfoRequestMessage, InfoRequest, TMsgType::InfoRequest )
JD_BEGIN( TUdpInfoRequestMessage, "Requests descriptive data for a compact status revision. Only the named parts are requested." )
  JD_CONST( uint32_t, Ver, 3 )
  JD_CONST( TMsgType, Type, TMsgType::InfoRequest )
  JD( TChannelId, ID, Required, ServiceAddress, "", Live )
  JD( TGNString, InfoRev, Required, Hex16, "Revision requested by the receiver", Live )
  JD( ArrayOf(TGNString), Fields, Required, Count(0,64), "Requested part names", Live )
JD_END( TUdpInfoRequestMessage )

#endif
