// TUdpStatusData.inc.h, Version: 1.03
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TUdpStatusData) || defined(Do_TUdpStatusMessage) || defined(Do_TWsStatusEvent) || defined(Do_TJsonCachedStatus) || defined(Do_TSseStatusBatch)
JD_BEGIN( TUdpStatusData, "Periodic snapshot used by other services to track device availability, current outputs and measurements. Devices also send it when relevant states change." )
  JD( TChannelId, DevId, Optional, ChannelAddress, "", Omit )
  JD( TJsonIdentity, Identity, Optional, Any, "Service identity, logical assignment and restart information", Omit )
  JD( uint64_t, BootCnt, Optional, Any, "Boot generation of the reporting service. LaptopBattery counts operating system boots, independently of sender restarts.", Live )
  JD( TGNString, Kind, Optional, Any, "Application-specific status category, for example LaptopPower", Info )
  JD( uint64_t, OnlineSince, Optional, Any, "", Info )
  JD( TUnixTime, CurrentTime, Optional, Any, "", Live )
  JD( bool, TimeSynchronized, Optional, Any, "", Live )
  JD( bool, Healthy, Optional, Any, "Whether the sender considers its application status healthy", Live )
  JD( TGNString, TimeSyncErr, Optional, Any, "Explanation when the device clock is not synchronized", Info )
  JD( TUdpLastError, LastErr, Optional, Any, "", Info )
  JD( ArrayOf(TGNString), Errs, Optional, Count(0,64), "", Info )
  JD( ArrayOf(TUdpChannel), Chs, Optional, Count(0,64), "", Live )
  JD( ArrayOf(TUdpMeasurement), Meas, Optional, Count(0,64), "", Live )
  JD( TUdpPowerMeter, PowerMeter, Optional, Any, "", Live )
  JD( double, Temp, Optional, Any, "", Live )
  JD( double, BatteryPct, Optional, Range(0,100), "", Live )
  JD( bool, AcOnline, Optional, Any, "", Live )
  JD( TGNString, BatteryState, Optional, Any, "", Live )
  JD( bool, ChargeLearnAllowed, Optional, Any, "", Live )
  JD( TUdpHeatControl, HeatControl, Optional, Any, "", Live )
  JD( TUdpHeatDiagnostics, HeatDiag, Optional, Any, "", Live )
  JD( TUdpChargeLearning, ChargeLearn, Optional, Any, "", Live )
  JD( TGNString, AppVer, Optional, Any, "Sender program version", Info )
  JD( ArrayOf(TJsonFeedbackState), FeedbackMonitors, Optional, Count(0,64), "Dashboard feedback monitor evaluations", Omit )
JD_END( TUdpStatusData )

#endif
