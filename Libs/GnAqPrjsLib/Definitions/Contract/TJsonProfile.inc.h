// TJsonProfile.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonProfile) || defined(Do_TJsonProfiles) || defined(Do_TJsonChannels) || defined(Do_TJsonSchedule) || defined(Do_THttpTimePayload) || defined(Do_THttpTimeReply) || defined(Do_TSseRecordingEvent) || defined(Do_TJsonEnergyAssignment) || defined(Do_TJsonHistoryMetadata)
JD_BEGIN( TJsonProfile, "Heating or battery charging profile. Battery charging uses its own start and stop thresholds." )
  JD( TGNString, ProfName, Required, NonEmpty, "Unique name used by channel configuration", Live )
  JD( TChannelId, MeasSrc, Required, ChannelAddress, "", Live )
  JD( TGNString, Meas, Optional, MeasurementName, "Kind of measurement consumed by this profile", Live )
  JD( bool, OnIfNoMeas, Required, Any, "Fallback output state when a required measurement is unavailable", Live )
  JD( double, ChargeStartPct, Optional, Range(0,100), "Battery threshold for switching charging on", Live )
  JD( double, ChargeStopPct, Optional, Range(0,100), "Battery threshold for switching charging off", Live )
  JD( bool, ChargeRunOn, Optional, Any, "Enables charging after the laptop stops reporting and learning the charging time factor", Live )
  JD( uint32_t, ChargeSecPerPct, Optional, RequiredIf(ChargeRunOn,Range(1,4294967295)), "Charging time in whole seconds per percentage point", Live )
  JD( TJsonHeatConfig, Heat, Optional, TemperatureProfileIfActive, "", Live )
JD_END( TJsonProfile )

#endif
