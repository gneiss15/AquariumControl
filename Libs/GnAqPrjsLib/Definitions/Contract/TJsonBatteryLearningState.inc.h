// TJsonBatteryLearningState.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonBatteryLearningState)
JD_BEGIN( TJsonBatteryLearningState, "Persistent laptop permission and boot counter" )
  JD( TGNString, LastBootId, Required, EmptyOrUuid, "Linux boot UUID last accounted for by the sender", Live )
  JD( uint32_t, ChargeBootCnt, Required, Any, "Persisted count of observed laptop OS boots, sent as Status.BootCounter", Live )
  JD( bool, ChargeLearnAllowed, Required, Any, "Whether charging observations may update the learning factor", Live )
JD_END( TJsonBatteryLearningState )

#endif
