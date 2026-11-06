// TJsonFeedbackState.inc.h, Version: 1.02
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonFeedbackState) || defined(Do_TJsonFeedbackStates) || defined(Do_TUdpStatusData) || defined(Do_TUdpStatusMessage) || defined(Do_TWsStatusEvent) || defined(Do_TJsonCachedStatus) || defined(Do_TSseStatusBatch)
JD_BEGIN( TJsonFeedbackState, "Evaluated feedback monitor state" )
  JD( TGNString, Id, Required, NonEmpty, "Monitor identifier", Live )
  JD( TGNString, SwitchFunc, Required, NonEmpty, "Monitored switch", Live )
  JD( TGNString, Src, Required, NonEmpty, "Feedback sender", Live )
  JD( TGNString, State, Required, Choice(Unknown,OK,Pending,Mismatch), "Evaluation state", Live )
  JD( TGNString, Reason, Required, Any, "Mismatch or unknown reason; empty when OK", Live )
  JD( bool, SwitchState, Required, Any, "Fresh switch state or unknown", Live )
  JD( bool, FeedbackState, Required, Any, "Fresh feedback or unknown", Live )
  JD( uint32_t, GraceSec, Required, Range(1,3600), "Configured tolerance in seconds", Live )
JD_END( TJsonFeedbackState )

#endif
