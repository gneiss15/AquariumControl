// TJsonRecordingChannels.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonRecordingChannels)
JD_BEGIN( TJsonRecordingChannels, "Channel groups projected from the central times response" )
  JD( MapOf(TJsonChannelGroup), Chs, Required, Any, "Per-device channel groups", Live )
JD_END( TJsonRecordingChannels )

#endif
