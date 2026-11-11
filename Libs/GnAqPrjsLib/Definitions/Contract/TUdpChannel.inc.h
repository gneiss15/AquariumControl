// TUdpChannel.inc.h, Version: 1.03
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TUdpChannel) || defined(Do_TUdpStatusData) || defined(Do_TUdpStatusMessage) || defined(Do_TWsStatusEvent) || defined(Do_TJsonCachedStatus) || defined(Do_TSseStatusBatch)
JD_BEGIN( TUdpChannel, "Current state and descriptive data for one device channel." )
  JD( TChannelId, Id, Optional, ChannelAddress, "", Live )
  JD( bool, State, Optional, Any, "Current state (On/Off)", Live )
  JD( uint16_t, Bits, Optional, Any, "Compact bool fields use adjacent value and `<Field>Msk` bits. A clear mask retains the receiver's prior value; a set mask applies the value bit.", Live )
  JD( TUnixTime, OvrUntil, Optional, Any, "Override end time", Live )
  JD( TGNString, Name, Optional, Any, "Channel display name", Info )
  JD( uint32_t, HwCh, Optional, Any, "Physical channel number", Info )
  JD( bool, HasIn, Optional, Any, "Whether the channel has an input", Info )
  JD( bool, HasOut, Optional, Any, "Whether the channel has an output", Info )
  JD( bool, InState, Optional, Any, "Current input state; compact status encodes it in Bits", Omit )
  JD( bool, AutomaticState, Optional, Any, "Automatic output request; compact status encodes it in Bits", Omit )
  JD( bool, OvrActive, Optional, Any, "Whether a manual override is active; compact status encodes it in Bits", Omit )
  JD( bool, OvrState, Optional, Any, "State requested by a manual override; compact status encodes it in Bits", Omit )
  JD( bool, CalOvrActive, Optional, Any, "Whether a calibration override is active; compact status encodes it in Bits", Omit )
  JD( TUdpNextChange, NextChange, Optional, Any, "Next scheduled channel event", Info )
  JD( TUdpStateLabels, StateLabels, Optional, Any, "Translation keys for specialized channel states", Info )
  JD( TGNString, Err, Optional, Any, "Current channel error text", Info )
JD_END( TUdpChannel )

#endif
