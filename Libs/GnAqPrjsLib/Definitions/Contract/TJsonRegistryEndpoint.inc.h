// TJsonRegistryEndpoint.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonRegistryEndpoint) || defined(Do_TJsonRegistryView) || defined(Do_TSseRecordingEvent)
JD_BEGIN( TJsonRegistryEndpoint, "Physical endpoint assignment" )
  JD( TChannelId, OutAddress, Required, OutputAddress, "", Live )
  JD( TGNString, Func, Required, NonEmpty, "Logical endpoint function", Live )
  JD( TGNString, DevName, Optional, Any, "Logical name of the containing device", Live )
  JD( TGNString, HwName, Optional, Any, "Physical endpoint label", Live )
  JD( uint32_t, Index, Optional, Any, "Hardware row index", Live )
  JD( TCapabilityFlags, Caps, Required, Any, "", Live )
  JD( TGNString, ChName, Optional, Any, "Logical channel name", Live )
  JD( TGNString, Role, Optional, Any, "Channel role", Live )
  JD( TGNString, Prof, Optional, Any, "Assigned profile or empty", Live )
  JD( bool, UseHwName, Optional, Any, "Display physical label", Live )
  JD( ArrayOf(TGNString), Groups, Optional, Any, "Aquarium groups", Live )
JD_END( TJsonRegistryEndpoint )

#endif
