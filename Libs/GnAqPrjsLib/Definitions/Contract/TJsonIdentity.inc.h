// TJsonIdentity.inc.h, Version: 1.02
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonIdentity) || defined(Do_TUdpStatusData) || defined(Do_TUdpStatusMessage) || defined(Do_TJsonIdentityEnvelope) || defined(Do_THttpDevice) || defined(Do_TWsStatusEvent) || defined(Do_TJsonCachedStatus) || defined(Do_TSseStatusBatch) || defined(Do_THttpTimePayload) || defined(Do_THttpTimeReply)
JD_BEGIN( TJsonIdentity, "Shared identity for storage, configuration exchange and UDP status." )
  JD_CONST( uint32_t, Ver, 1 )
  JD( TGNString, Func, Optional, Any, "Logical function assigned to this service", Info )
  JD( TGNString, Rev, Optional, Any, "Revision of the assigned configuration", Info )
  JD( TGNString, AppliedRev, Optional, Any, "Last assignment revision applied by the service", Info )
  JD( TChannelId, ServiceId, Optional, ServiceAddress, "", Omit )
  JD( bool, Prepared, Optional, Any, "Whether a valid identity and assignment state has been loaded or applied", Info )
  JD( bool, LocalSrcs, Optional, Any, "Whether source assignments were written locally", Info )
  JD( uint64_t, BootCnt, Optional, Any, "Device boot generation used to detect restarts", Live )
  JD( MapOf(TChannelId), Srcs, Optional, All(Count(0,32),Each(ChannelAddress)), "Map from logical source name to confirmed device ID. Source mappings are exchanged separately from compact status.", Omit )
JD_END( TJsonIdentity )

#endif
