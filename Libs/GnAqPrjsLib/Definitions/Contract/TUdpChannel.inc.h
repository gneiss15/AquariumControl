// TUdpChannel.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TUdpChannel) || defined(Do_TUdpStatusData) || defined(Do_TUdpStatusMessage) || defined(Do_TWsStatusEvent) || defined(Do_TJsonCachedStatus) || defined(Do_TSseStatusBatch)
JD_BEGIN( TUdpChannel, "Current state and descriptive data for one device channel." )
  JD( TChannelId, Id, Optional, ChannelAddress, "", Live )
  JD( bool, State, Optional, Any, "Current state (On/Off)", Live )
  JD( uint8_t, Bits, Optional, Any, "See [UDP bit flags](#udp-bit-flags).", Live )
  JD( TUnixTime, Until, Optional, Any, "", Live )
  JD( TGNString, Name, Optional, Any, "Channel display name", Info )
  JD( uint32_t, HwCh, Optional, Any, "Physical channel number", Info )
  JD( bool, HasIn, Optional, Any, "Whether the channel has an input", Info )
  JD( bool, HasOut, Optional, Any, "Whether the channel has an output", Info )
  JD( bool, InState, Optional, Any, "Current input state", Live )
  JD( bool, AutomaticState, Optional, Any, "State requested by automatic control", Live )
  JD( bool, OvrActive, Optional, Any, "Whether a manual override is active", Live )
  JD( bool, OvrState, Optional, Any, "State requested by the manual override", Live )
  JD( bool, CalOvrActive, Optional, Any, "Whether calibration currently overrides the output", Live )
  JD( TUnixTime, OvrUntil, Optional, Any, "", Live )
  JD( TUdpNextChange, NextChange, Optional, Any, "Next scheduled channel event", Info )
  JD( TUdpStateLabels, StateLabels, Optional, Any, "Translation keys for specialized channel states", Info )
  JD( TGNString, Err, Optional, Any, "Current channel error text", Info )
JD_END( TUdpChannel )

#endif
