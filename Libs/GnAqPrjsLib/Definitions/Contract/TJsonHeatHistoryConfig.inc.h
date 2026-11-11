// TJsonHeatHistoryConfig.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonHeatHistoryConfig)
JD_BEGIN( TJsonHeatHistoryConfig, "HeatHistory recorder configuration" )
  JD( TGNString, DashboardUrl, Optional, HttpUrl, "Complete Dashboard base URL, including scheme and port when required", Live )
  JD( TGNString, DataDir, Optional, Any, "Directory for local recordings", Live )
  JD( double, StatusMaxAgeSec, Optional, Range(5,300), "Maximum usable age of the last device status", Live )
  JD( MapOf(double), FloorPowerW, Optional, Each(Positive), "Nominal floor heating power in watts by profile", Live )
  JD( TGNString, RoomTempSrc, Optional, Any, "Service providing ambient temperature", Live )
  JD( MapOf(double), PowerFactors, Optional, Each(Positive), "Power correction factors by source", Live )
  JD( double, EnergyMaxIntervalSec, Optional, Range(10,30), "Maximum interval treated as consecutive energy samples", Live )
  JD( uint32_t, ViewRetentionDays, Optional, Range(1,366), "Maximum history span shown in the browser", Live )
  JD( bool, StorageDeleteExpired, Optional, Any, "Enable automatic deletion of expired recordings", Live )
  JD( uint32_t, StorageRetentionDays, Optional, Any, "Maximum age of closed recordings when cleanup is enabled", Live )
  JD( uint64_t, StorageMaxBytes, Optional, Any, "Storage budget for recordings", Live )
  JD( TGNString, ActiveFrom, Optional, Any, "Earliest timestamp used for active PI history", Live )
JD_END( TJsonHeatHistoryConfig )

#endif
