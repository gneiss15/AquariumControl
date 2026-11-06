// TJsonChannels.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonChannels) || defined(Do_TJsonSchedule) || defined(Do_THttpTimePayload) || defined(Do_THttpTimeReply)
JD_BEGIN( TJsonChannels, "Bundled device channels and profiles Both TimerSwitch and MultiTimer use this structure for their channel configuration." )
  JD( ArrayOf(TJsonProfile), Profs, Optional, Any, "Profiles available to these channels", Live )
  JD( ArrayOf(TJsonChannel), Chs, Required, Any, "Channel configurations for this device", Live )
JD_END( TJsonChannels )

#endif
