// TJsonHistoryTemperatureRow.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonHistoryTemperatureRow) || defined(Do_TJsonHistorySeries) || defined(Do_THttpHistoryReply)
JD_TUPLE_BEGIN( TJsonHistoryTemperatureRow, "Legacy JSON temperature row" )
  JD( TGNString, Time, Required, Revision, "UTC sample timestamp", Live )
  JD( double, WaterC, Required, Any, "Water temperature in degrees Celsius", Live )
  JD( uint32_t, Floor, Required, Range(0,1), "Floor output state (0 or 1)", Live )
  JD( uint32_t, Main, Required, Range(0,1), "Main output state (0 or 1)", Live )
JD_END( TJsonHistoryTemperatureRow )

#endif
