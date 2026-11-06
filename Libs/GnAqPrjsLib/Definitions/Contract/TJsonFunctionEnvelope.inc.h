// TJsonFunctionEnvelope.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonFunctionEnvelope)
JD_BEGIN( TJsonFunctionEnvelope, "Function configuration transfer" )
  JD( TJsonFunctionConfig, FuncCfg, Optional, Any, "Function-bound settings. Additional keys are ignored.", Live )
JD_END( TJsonFunctionEnvelope )

#endif
