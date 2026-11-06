// TJsonChannel.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonChannel) || defined(Do_TJsonChannels) || defined(Do_TJsonSchedule) || defined(Do_THttpTimePayload) || defined(Do_THttpTimeReply) || defined(Do_TJsonChannelGroup) || defined(Do_TJsonRecordingChannels) || defined(Do_TSseRecordingEvent)
JD_BEGIN( TJsonChannel, "Physical channel configuration and logical role" )
  JD( TGNString, Id, Required, NonEmpty, "Logical channel name used in configuration and assignments", Live )
  JD( TGNString, Role, Optional, Choice(Timer,MainSw,HeaterMain,HeaterFloor), "Control role assigned to the channel", Live )
  JD( TGNString, Prof, Optional, NonEmpty, "Name of the profile assigned to the channel", Live )
  JD( TGNString, PhysPos, Optional, Any, "Fixed physical position, for example Main 3 for socket 3; retained during a logical channel swap", Live )
  JD( uint32_t, InPin, Optional, Any, "Hardware pin used to read the input", Live )
  JD( uint32_t, OutPin, Optional, Any, "Hardware pin driving the output", Live )
  JD( bool, InPullup, Optional, Any, "Enable the input pull-up resistor", Live )
  JD( bool, InActiveHigh, Optional, Any, "Whether a high input level is active", Live )
  JD( bool, OutActiveHigh, Optional, Any, "Whether a high output level switches the output on", Live )
JD_END( TJsonChannel )

#endif
