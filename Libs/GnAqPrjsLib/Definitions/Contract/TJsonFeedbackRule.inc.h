// TJsonFeedbackRule.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonFeedbackRule) || defined(Do_TJsonFeedbackConfiguration) || defined(Do_THttpFeedbackDocument) || defined(Do_THttpFeedbackWrite)
JD_BEGIN( TJsonFeedbackRule, "Independent switch feedback monitoring rule" )
  JD( TGNString, Id, Required, NonEmpty, "Stable monitor identifier", Live )
  JD( TGNString, SwitchFunc, Required, NonEmpty, "Monitored switch function", Live )
  JD( TGNString, Src, Required, NonEmpty, "Feedback sender function", Live )
  JD( TGNString, StatusKind, Required, NonEmpty, "Expected sender status kind", Live )
  JD( TGNString, FeedbackField, Required, NonEmpty, "Configured boolean feedback field", Live )
  JD( uint32_t, GraceSec, Required, Range(1,3600), "Continuous mismatch tolerance in seconds", Live )
JD_END( TJsonFeedbackRule )

#endif
