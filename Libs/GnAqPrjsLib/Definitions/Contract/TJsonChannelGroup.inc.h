// TJsonChannelGroup.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonChannelGroup) || defined(Do_TJsonRecordingChannels) || defined(Do_TSseRecordingEvent)
JD_BEGIN( TJsonChannelGroup, "Central channel group for one device" )
  JD( ArrayOf(TJsonChannel), Rows, Required, Any, "Logical channel assignments", Live )
  JD( TGNString, TimeStamp, Optional, Any, "Channel revision", Live )
JD_END( TJsonChannelGroup )

#endif
