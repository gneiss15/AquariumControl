// TJsonHistoryState.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonHistoryState)
JD_BEGIN( TJsonHistoryState, "HeatHistory runtime recording state" )
  JD( TGNString, HostBootId, Optional, Any, "Recorder system boot identifier", Live )
  JD( TGNString, ProfDigest, Optional, Any, "Semantic profile digest", Live )
  JD( TGNString, CsvFile, Optional, Any, "Current temperature CSV basename", Live )
  JD( TGNString, ProfFile, Optional, Any, "Associated profile snapshot basename", Live )
  JD( TJsonHistorySample, LastSample, Optional, Any, "Last stored state", Live )
  JD( TGNString, AssignmentSignature, Optional, Any, "Bound hardware/profile signature", Live )
JD_END( TJsonHistoryState )

#endif
