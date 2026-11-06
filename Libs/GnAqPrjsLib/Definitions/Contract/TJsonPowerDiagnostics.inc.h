// TJsonPowerDiagnostics.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonPowerDiagnostics) || defined(Do_TUdpPowerMeter) || defined(Do_TUdpStatusData) || defined(Do_TUdpStatusMessage) || defined(Do_TWsStatusEvent) || defined(Do_TJsonCachedStatus) || defined(Do_TSseStatusBatch)
JD_BEGIN( TJsonPowerDiagnostics, "Power meter hardware diagnostics, HTTP only" )
  JD( uint64_t, CfEdges, Required, Any, "Power pulse count", Omit )
  JD( uint64_t, Cf1Edges, Required, Any, "Voltage/current pulse count", Omit )
  JD( uint64_t, VoltageIntervals, Required, Any, "Completed voltage intervals", Omit )
  JD( uint64_t, CurrentIntervals, Required, Any, "Completed current intervals", Omit )
  JD( uint32_t, SelectorLevel, Required, Any, "Measurement selector level", Omit )
  JD( bool, LoadEnabled, Required, Any, "Meter measurement gate enabled", Omit )
JD_END( TJsonPowerDiagnostics )

#endif
