// TJsonHeatConfig.inc.h, Version: 1.02
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonHeatConfig) || defined(Do_TUdpHeatDiagnostics) || defined(Do_TUdpStatusData) || defined(Do_TUdpStatusMessage) || defined(Do_TJsonProfile) || defined(Do_TJsonProfiles) || defined(Do_TJsonChannels) || defined(Do_TJsonSchedule) || defined(Do_TWsStatusEvent) || defined(Do_TJsonCachedStatus) || defined(Do_TSseStatusBatch) || defined(Do_THttpTimePayload) || defined(Do_THttpTimeReply) || defined(Do_TSseRecordingEvent) || defined(Do_TJsonEnergyAssignment) || defined(Do_TJsonHistoryMetadata)
JD_BEGIN( TJsonHeatConfig, "Settings of the proportional-integral heating controller (PI controller)." )
  JD( double, Setpoint, Optional, Any, "Water setpoint in °C", Live )
  JD( double, FloorW, Optional, Any, "Floor heating power in W", Omit )
  JD( double, MainMaxW, Optional, Any, "Main heater power capacity in W", Omit )
  JD( double, CapacityWhPerK, Optional, Any, "Thermal capacity in Wh/K", Omit )
  JD( double, LossWPerK, Optional, Any, "Thermal loss coefficient in W/K", Omit )
  JD( double, Kp, Optional, Any, "Proportional gain in W/K", Omit )
  JD( uint32_t, TiSec, Optional, Any, "Integral time Ti in seconds; Ki = Kp / Ti", Omit )
  JD( uint32_t, WindowSec, Optional, Any, "PWM period in seconds", Omit )
  JD( uint32_t, DelaySec, Optional, Any, "Thermal response delay in seconds", Omit )
  JD( TChannelId, FloorEndpoint, Optional, OutputAddress, "", Omit )
  JD( TChannelId, MainEndpoint, Optional, OutputAddress, "", Omit )
  JD( bool, Active, Optional, Any, "Whether PI control is enabled", Omit )
JD_END( TJsonHeatConfig )

#endif
