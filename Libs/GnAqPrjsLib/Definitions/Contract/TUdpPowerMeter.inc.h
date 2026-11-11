// TUdpPowerMeter.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TUdpPowerMeter) || defined(Do_TUdpStatusData) || defined(Do_TUdpStatusMessage) || defined(Do_TWsStatusEvent) || defined(Do_TJsonCachedStatus) || defined(Do_TSseStatusBatch)
JD_BEGIN( TUdpPowerMeter, "Latest voltage, current and active power readings, with validity information." )
  JD( double, Voltage, Optional, Any, "", Live )
  JD( double, Current, Optional, Any, "", Live )
  JD( double, ActivePower, Optional, Any, "", Live )
  JD( bool, Valid, Optional, Any, "", Live )
  JD( TJsonPowerDiagnostics, Diag, Optional, Any, "HTTP diagnostics excluded from compact UDP", Omit )
JD_END( TUdpPowerMeter )

#endif
