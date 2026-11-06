// TUdpStatusMessage.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TUdpStatusMessage)
JD_MESSAGE( TUdpStatusMessage, Status, TMsgType::Status )
JD_BEGIN( TUdpStatusMessage, "Reports the device state, channel states and measurements. Compact status carries live values and references the descriptive data by revision." )
  JD_CONST( uint32_t, Ver, 3 )
  JD_CONST( TMsgType, Type, TMsgType::Status )
  JD( TChannelId, ID, Required, ServiceAddress, "", Live )
  JD( bool, Compact, Optional, Any, "Whether descriptive fields are sent separately", Live )
  JD( TGNString, InfoRev, Optional, RequiredIf(Compact,Hex16), "Revision of the associated descriptive data", Live )
  JD( TUdpStatusData, Status, Required, ItemsHaveIf(Compact,Chs,Id), "Device status snapshot", Live )
JD_END( TUdpStatusMessage )

#endif
