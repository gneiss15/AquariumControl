// TUdpChargeLearning.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TUdpChargeLearning) || defined(Do_TUdpStatusData) || defined(Do_TUdpStatusMessage) || defined(Do_TWsStatusEvent) || defined(Do_TJsonCachedStatus) || defined(Do_TSseStatusBatch)
JD_BEGIN( TUdpChargeLearning, "Result of a completed laptop charging learning cycle." )
  JD_CONST( uint32_t, Ver, 1 )
  JD( uint32_t, Seq, Required, Any, "Event counter within the switch boot, used to identify repeated reports", Live )
  JD( TUnixTime, CompletedAt, Required, Any, "", Live )
  JD( TGNString, Prof, Required, Any, "Name of the charging profile", Live )
  JD( double, BeforePct, Required, Range(0,100), "", Live )
  JD( double, AfterPct, Required, Range(0,100), "", Live )
  JD( uint32_t, AppliedSecPerPct, Required, Any, "", Live )
  JD( uint64_t, ChargeMs, Required, Any, "", Live )
  JD( uint32_t, NewSecPerPct, Required, Any, "", Live )
  JD( TGNString, Persistence, Required, Any, "RamOnly for an in-memory result, Saved for successful persistence, or SaveFailed if persistence failed", Live )
JD_END( TUdpChargeLearning )

#endif
