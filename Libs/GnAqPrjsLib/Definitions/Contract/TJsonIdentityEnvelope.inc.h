// TJsonIdentityEnvelope.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonIdentityEnvelope)
JD_BEGIN( TJsonIdentityEnvelope, "Assignment envelope" )
  JD( TJsonIdentity, Identity, Required, Any, "Assigned identity", Live )
JD_END( TJsonIdentityEnvelope )

#endif
