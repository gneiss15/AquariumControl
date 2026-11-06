// THttpTimeReply.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_THttpTimeReply)
JD_BEGIN( THttpTimeReply, "Configuration synchronization response" )
  JD( bool, Ok, Optional, Any, "Optional success indicator", Live )
  JD( TGNString, Action, Required, Choice(UseCentral,Synced), "Synchronization action", Live )
  JD( TGNString, TimeStamp, Required, NonEmpty, "Applied or offered revision", Live )
  JD( TJsonIdentity, Identity, Required, Any, "Prepared identity/source assignment", Live )
  JD( TJsonFunctionConfig, FuncCfg, Optional, Any, "Function-bound settings. Additional keys are ignored.", Live )
  JD( VariantOf(TJsonFeedTimes,TJsonSchedule), Content, Optional, RequiredWhen(Action,UseCentral,Any), "Central configuration when requested", Live )
  JD( TChannelId, Dev, Optional, ServiceAddress, "", Live )
  JD( TGNString, Type, Optional, Any, "Schedule kind", Live )
  JD( TGNString, Format, Optional, Any, "Device schedule format", Live )
JD_END( THttpTimeReply )

#endif
