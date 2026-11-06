// TJsonEnergyAssignment.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonEnergyAssignment) || defined(Do_TJsonHistoryMetadata)
JD_BEGIN( TJsonEnergyAssignment, "Historical energy recording assignment" )
  JD( TJsonProfile, Prof, Optional, Any, "Available historical profile values", Live )
  JD( TJsonHistoryEndpoint, Main, Optional, Any, "Main endpoint", Live )
  JD( TJsonHistoryEndpoint, Floor, Optional, Any, "Floor endpoint", Live )
  JD( TGNString, Temp, Optional, Any, "Temperature source", Live )
  JD( TGNString, Room, Optional, Any, "Room temperature source", Live )
  JD( double, FloorW, Optional, Positive, "Measured floor heating power in W", Live )
  JD( double, Factor, Optional, Positive, "Recorded power factor", Live )
  JD( TGNString, Signature, Optional, Any, "Assignment comparison signature", Live )
JD_END( TJsonEnergyAssignment )

#endif
