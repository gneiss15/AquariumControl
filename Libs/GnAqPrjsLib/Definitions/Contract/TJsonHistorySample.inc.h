// TJsonHistorySample.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonHistorySample) || defined(Do_TJsonHistoryState)
JD_BEGIN( TJsonHistorySample, "Last recorded temperature and switching state" )
  JD( double, Temp, Required, Any, "Water temperature in degrees Celsius", Live )
  JD( bool, Floor, Required, Any, "Floor output", Live )
  JD( bool, Main, Required, Any, "Main output", Live )
JD_END( TJsonHistorySample )

#endif
