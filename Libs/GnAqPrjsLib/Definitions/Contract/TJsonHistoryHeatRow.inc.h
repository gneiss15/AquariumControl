// TJsonHistoryHeatRow.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonHistoryHeatRow) || defined(Do_TJsonHistorySeries) || defined(Do_THttpHistoryReply)
JD_TUPLE_BEGIN( TJsonHistoryHeatRow, "Legacy JSON PI row" )
  JD( TGNString, Time, Required, Revision, "UTC sample timestamp", Live )
  JD( double, DemandW, Required, Any, "Total demand in W", Live )
  JD( double, ActualW, Required, Any, "Actual power in W", Live )
  JD( double, FloorDemandW, Required, Any, "Floor demand in W", Live )
  JD( double, MainDemandW, Required, Any, "Main demand in W", Live )
  JD( double, ProportionalW, Required, Any, "Proportional contribution in W", Live )
  JD( double, IntegralW, Required, Any, "Integral contribution in W", Live )
  JD( double, FeedForwardW, Required, Any, "Estimated heat loss in W", Live )
  JD( double, Setpoint, Required, Any, "Historical setpoint in degrees Celsius", Live )
  JD( bool, Boundary, Required, Any, "First selected row in a segment", Live )
JD_END( TJsonHistoryHeatRow )

#endif
