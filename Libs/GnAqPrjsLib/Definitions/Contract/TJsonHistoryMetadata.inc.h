// TJsonHistoryMetadata.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonHistoryMetadata)
JD_BEGIN( TJsonHistoryMetadata, "Temperature, energy or PI segment metadata" )
  JD( TGNString, HostBootId, Optional, Any, "Recorder system boot identifier", Live )
  JD_CONST( uint32_t, Ver, 1 )
  JD( TGNString, StartedAt, Optional, Any, "UTC segment start", Live )
  JD( VariantOf(TJsonProfile,TGNString), Prof, Optional, Any, "Profile snapshot or PI profile id", Live )
  JD( TJsonEnergyAssignment, Assignment, Optional, Any, "Bound energy assignment", Live )
  JD( TGNString, Integration, Optional, Any, "Integration method description", Live )
  JD( TGNString, Limitations, Optional, Any, "Recording limitations", Live )
  JD( TJsonValue, PowerCalAtStart, Optional, Any, "Opaque calibration provenance snapshot", Live )
  JD( TGNString, PowerCal, Optional, Any, "Calibration quality description", Live )
  JD( TChannelId, Dev, Optional, ServiceAddress, "", Live )
  JD( uint64_t, BootCnt, Optional, Any, "PI sender boot generation", Live )
  JD( TJsonHeatConfig, Cfg, Optional, Any, "Recorded PI configuration", Live )
  JD( bool, Active, Optional, Any, "Whether heating control was active", Live )
JD_END( TJsonHistoryMetadata )

#endif
