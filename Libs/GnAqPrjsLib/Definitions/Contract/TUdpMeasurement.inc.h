// TUdpMeasurement.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TUdpMeasurement) || defined(Do_TUdpStatusData) || defined(Do_TUdpStatusMessage) || defined(Do_TWsStatusEvent) || defined(Do_TJsonCachedStatus) || defined(Do_TSseStatusBatch)
JD_BEGIN( TUdpMeasurement, "One measured value and its validity, timestamp and descriptive metadata." )
  JD( TGNString, Kind, Required, NonEmpty, "Measurement kind, for example Temperature or BatteryPercent", Live )
  JD( double, Value, Optional, Any, "", Live )
  JD( bool, Valid, Optional, Any, "", Live )
  JD( TUnixTime, UpdatedAt, Optional, Any, "", Live )
  JD( TGNString, Name, Optional, Any, "", Info )
  JD( TGNString, Unit, Optional, Any, "", Info )
  JD( uint8_t, Decimals, Optional, Any, "Preferred number of decimal places for display; does not change the transmitted value", Info )
  JD( TGNString, Err, Optional, Any, "", Info )
JD_END( TUdpMeasurement )

#endif
