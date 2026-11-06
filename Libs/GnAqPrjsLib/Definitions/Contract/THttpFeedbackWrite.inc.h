// THttpFeedbackWrite.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_THttpFeedbackWrite)
JD_BEGIN( THttpFeedbackWrite, "Optimistic feedback configuration update" )
  JD( TJsonFeedbackConfiguration, Cfg, Required, Any, "Current monitor configuration", Live )
  JD( TJsonFeedbackConfiguration, Previous, Required, Any, "Configuration seen by the editor", Live )
JD_END( THttpFeedbackWrite )

#endif
