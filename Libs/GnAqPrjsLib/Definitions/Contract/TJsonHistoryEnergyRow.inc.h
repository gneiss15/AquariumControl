// TJsonHistoryEnergyRow.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonHistoryEnergyRow) || defined(Do_TJsonHistorySeries) || defined(Do_THttpHistoryReply)
JD_TUPLE_BEGIN( TJsonHistoryEnergyRow, "Legacy JSON energy row" )
  JD( TGNString, Time, Required, Revision, "UTC sample timestamp", Live )
  JD( double, WaterC, Required, Any, "Water temperature in degrees Celsius", Live )
  JD( double, RoomC, Required, Any, "Room temperature in degrees Celsius", Live )
  JD( uint32_t, Floor, Required, Range(0,1), "Floor output state (0 or 1)", Live )
  JD( uint32_t, Main, Required, Range(0,1), "Main output state (0 or 1)", Live )
  JD( double, MainW, Required, Any, "Main heating power in W", Live )
  JD( double, FloorW, Required, Any, "Floor heating power in W", Live )
  JD( double, MainWh, Required, Any, "Main interval energy in Wh", Live )
  JD( double, FloorWh, Required, Any, "Floor interval energy in Wh", Live )
  JD( bool, MainValid, Required, Any, "Main data usable", Live )
  JD( bool, FloorValid, Required, Any, "Floor data usable", Live )
  JD( bool, Boundary, Required, Any, "First selected row in a segment", Live )
  JD( double, Setpoint, Required, Any, "Historical setpoint in degrees Celsius", Live )
JD_END( TJsonHistoryEnergyRow )

#endif
