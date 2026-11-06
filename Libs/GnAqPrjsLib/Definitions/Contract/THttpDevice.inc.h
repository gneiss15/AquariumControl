// THttpDevice.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_THttpDevice)
JD_BEGIN( THttpDevice, "Response to the device HTTP discovery request. It advertises the device, available features and URLs used by its web interface." )
  JD( uint32_t, ApiVer, Required, Any, "Version of the discovery API", Live )
  JD( TGNString, DevType, Required, Any, "Product identifier of the device", Live )
  JD( TGNString, Platform, Optional, Any, "Hardware or operating platform", Live )
  JD( TChannelId, DevId, Required, ChannelAddress, "", Live )
  JD( TGNString, Name, Required, Any, "Display name advertised by the device", Live )
  JD( TGNString, FirmwareVer, Required, Any, "Version of the running firmware", Live )
  JD( TGNString, DataVer, Optional, Any, "Version of the installed data assets", Live )
  JD( TGNString, RescueTargetVer, Optional, Any, "Rescue image version associated with the firmware", Live )
  JD( uint32_t, EndpointAssignments, Optional, Any, "Version of endpoint assignment support", Live )
  JD( ArrayOf(TGNString), Caps, Required, Any, "Features advertised by this service", Live )
  JD( TGNString, CfgUrl, Required, HttpUrl, "Complete configuration interface URL, including scheme and port when required", Live )
  JD( TWsEventEndpoint, Events, Required, Any, "WebSocket endpoint for receiving device status", Live )
  JD( THttpButtonAction, ButtonPress, Optional, Any, "Advertised simulated button action", Live )
  JD( TGNString, StatusClientUrl, Optional, HttpUrl, "Complete status client URL, including scheme and port when required", Live )
  JD( TJsonIdentity, Identity, Optional, Any, "Physical device identity", Live )
JD_END( THttpDevice )

#endif
