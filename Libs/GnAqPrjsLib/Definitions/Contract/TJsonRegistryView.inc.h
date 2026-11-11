// TJsonRegistryView.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonRegistryView) || defined(Do_TSseRecordingEvent)
JD_BEGIN( TJsonRegistryView, "Public Dashboard device & channel config view" )
  JD( TGNString, Rev, Optional, Any, "Assignment revision", Live )
  JD( bool, Pending, Required, Any, "Assignment rollout is pending", Live )
  JD( ArrayOf(TJsonRegistryDevice), Devices, Required, Any, "Managed devices", Live )
  JD( MapOf(TGNString), ExternalSrcs, Optional, Each(SourceId), "Unmanaged status senders", Live )
  JD( ArrayOf(TJsonRegistryEndpoint), Endpoints, Optional, Any, "Physical endpoint mappings", Live )
JD_END( TJsonRegistryView )

#endif
