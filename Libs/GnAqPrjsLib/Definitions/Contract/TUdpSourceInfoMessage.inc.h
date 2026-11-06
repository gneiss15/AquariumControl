// TUdpSourceInfoMessage.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TUdpSourceInfoMessage)
JD_MESSAGE( TUdpSourceInfoMessage, SourceInfo, TMsgType::SourceInfo )
JD_BEGIN( TUdpSourceInfoMessage, "Announces a confirmed mapping from a logical function to its data source." )
  JD_CONST( uint32_t, Ver, 3 )
  JD_CONST( TMsgType, Type, TMsgType::SourceInfo )
  JD( TChannelId, ID, Required, ServiceAddress, "", Live )
  JD( TGNString, Func, Required, Utf8Bytes(80), "Logical function that consumes the data", Live )
  JD( TChannelId, Src, Required, ChannelAddress, "", Live )
  JD( TGNString, Rev, Required, Revision, "Revision of the confirmed source mapping", Live )
JD_END( TUdpSourceInfoMessage )

#endif
