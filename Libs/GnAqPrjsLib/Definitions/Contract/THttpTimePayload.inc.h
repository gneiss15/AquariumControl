// THttpTimePayload.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_THttpTimePayload)
JD_BEGIN( THttpTimePayload, "HTTP schedule/profile synchronization payload" )
  JD( TChannelId, Dev, Required, ServiceAddress, "", Live )
  JD( TGNString, Type, Required, Choice(FeedTimes,SwitchTimes), "Schedule kind", Live )
  JD( TGNString, Format, Required, Choice(Feeder,TmrSw,MyTimer), "Device file format", Live )
  JD( TGNString, TimeStamp, Required, NonEmpty, "Configuration revision", Live )
  JD( VariantOf(TJsonFeedTimes,TJsonSchedule), Content, Required, Any, "Typed schedule content", Live )
  JD( TJsonIdentity, Identity, Optional, Any, "Physical identity and source assignments", Live )
  JD( TJsonFunctionConfig, FuncCfg, Optional, Any, "Values transferred with a function", Live )
JD_END( THttpTimePayload )

#endif
