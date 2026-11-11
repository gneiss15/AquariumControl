// THttpFeedbackDocument.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_THttpFeedbackDocument)
JD_BEGIN( THttpFeedbackDocument, "Feedback configuration HTTP response" )
  JD( TJsonFeedbackConfiguration, Cfg, Required, Any, "Current monitor configuration", Live )
JD_END( THttpFeedbackDocument )

#endif
