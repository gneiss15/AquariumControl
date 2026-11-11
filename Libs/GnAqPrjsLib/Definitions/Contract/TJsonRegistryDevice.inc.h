// TJsonRegistryDevice.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonRegistryDevice) || defined(Do_TJsonRegistryView) || defined(Do_TSseRecordingEvent)
JD_BEGIN( TJsonRegistryDevice, "Dashboard device & channel config device metadata" )
  JD( TGNString, Func, Required, NonEmpty, "Logical device function", Live )
  JD( TChannelId, DevId, Required, ServiceAddress, "", Live )
  JD( TGNString, DevType, Optional, Any, "Hardware/firmware class", Live )
  JD( TGNString, HwName, Optional, Any, "Physical enclosure name", Live )
  JD( TGNString, Url, Optional, HttpUrl, "Complete device base URL, including scheme and port when required", Live )
  JD( TServiceId, ServiceId, Optional, Any, "", Live )
  JD( TCapabilityFlags, Caps, Optional, Any, "", Live )
JD_END( TJsonRegistryDevice )

#endif
