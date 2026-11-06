// TUdpSourceRequestMessage.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TUdpSourceRequestMessage)
JD_MESSAGE( TUdpSourceRequestMessage, SourceRequest, TMsgType::SourceRequest )
JD_BEGIN( TUdpSourceRequestMessage, "Asks which device supplies data for a logical function." )
  JD_CONST( uint32_t, Ver, 3 )
  JD_CONST( TMsgType, Type, TMsgType::SourceRequest )
  JD( TGNString, Func, Required, Utf8Bytes(80), "Logical function whose source is requested", Live )
JD_END( TUdpSourceRequestMessage )

#endif
