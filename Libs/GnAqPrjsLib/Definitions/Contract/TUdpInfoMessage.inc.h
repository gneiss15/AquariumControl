// TUdpInfoMessage.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TUdpInfoMessage)
JD_MESSAGE( TUdpInfoMessage, Info, TMsgType::Info )
JD_BEGIN( TUdpInfoMessage, "Carries one complete descriptive part for a compact status revision." )
  JD_CONST( uint32_t, Ver, 3 )
  JD_CONST( TMsgType, Type, TMsgType::Info )
  JD( TChannelId, ID, Required, ServiceAddress, "", Live )
  JD( TGNString, InfoRev, Required, Hex16, "Revision to which the part belongs", Live )
  JD( TGNString, Field, Required, NonEmpty, "Name of the descriptive part", Live )
  JD( TGNString, Data, Required, Any, "Complete descriptive JSON object encoded as text", Live )
JD_END( TUdpInfoMessage )

#endif
