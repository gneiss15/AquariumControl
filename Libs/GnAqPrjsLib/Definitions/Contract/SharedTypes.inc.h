// SharedTypes.inc.h, Version: 1.02
// Repeated scalar/enum definition expansion.
// Compact channel status: each JD_BIT Name defines adjacent Name and NameMsk
// bits. Name carries the bool value; NameMsk says that this packet provides
// Name. A clear NameMsk preserves the receiver's previous value. A set
// NameMsk applies Name, including an explicitly clear value. Positions follow
// definition order; no numeric masks are assigned here.
#ifndef JD_ALIAS
  #define JD_ALIAS(Name, Type, Rule, Description)
#endif
#ifndef JD_ENUM_BEGIN
  #define JD_ENUM_BEGIN(Name, Type, Description)
#endif
#ifndef JD_ENUM
  #define JD_ENUM(Name, Value, Description)
#endif
#ifndef JD_ENUM_END
  #define JD_ENUM_END(Name)
#endif

#ifndef JD_BIT
  #define GN_JSON_OWN_BIT_HOOK
  #define JD_BIT(Name, Description)
#endif

JD_BIT( InState, "Current physical input state" )
JD_BIT( AutomaticState, "Automatic output request" )
JD_BIT( OvrActive, "Whether a manual override is active" )
JD_BIT( OvrState, "State requested by a manual override" )
JD_BIT( CalOvrActive, "Whether a calibration override is active" )

JD_ALIAS( TChannelId, TGNString, ChannelAddress, "Physical address: MAC [ @ ServiceId ] [ / ChannelNr ]" )
JD_ALIAS( TUnixTime, uint64_t, Any, "UTC time in whole seconds since 1970-01-01 00:00:00 UTC" )

JD_ENUM_BEGIN( TMsgType, uint8_t, "UDP message type" )
  JD_ENUM( Unknown, 0, "Unknown message type" )
  JD_ENUM( Request, 1, "Request current status" )
  JD_ENUM( Status, 2, "Current device status" )
  JD_ENUM( InfoRequest, 3, "Request descriptive status fields" )
  JD_ENUM( Info, 4, "Complete descriptive status part" )
  JD_ENUM( SourceRequest, 5, "Request source assignment" )
  JD_ENUM( SourceInfo, 6, "Confirmed source assignment" )
  JD_ENUM( TMsgType_MAX, 7, "End marker" )
JD_ENUM_END( TMsgType )

JD_ENUM_BEGIN( TNextChangeSource, uint8_t, "Origin of a scheduled event" )
  JD_ENUM( Unknown, 0, "Unknown event source" )
  JD_ENUM( SwitchTime, 1, "Scheduled output change" )
  JD_ENUM( OverrideEnd, 2, "Manual override end" )
  JD_ENUM( CalibrationOverride, 3, "Calibration override event" )
  JD_ENUM( FeedTime, 4, "Scheduled feeding" )
  JD_ENUM( TNextChangeSource_MAX, 5, "End marker" )
JD_ENUM_END( TNextChangeSource )

JD_ENUM_BEGIN( TServiceId, uint8_t, "Physical service class" )
  JD_ENUM( Unknown, 0, "Unknown service" )
  JD_ENUM( MultiTimer, 1, "MultiTimer service" )
  JD_ENUM( Dashboard, 2, "Dashboard service" )
  JD_ENUM( Measurement, 3, "Feeder or measurement source" )
  JD_ENUM( TimerSwitch, 4, "TimerSwitch service" )
  JD_ENUM( TServiceId_MAX, 5, "End marker" )
JD_ENUM_END( TServiceId )

JD_ENUM_BEGIN( TCapabilityFlags, uint8_t, "Endpoint capability bits" )
  JD_ENUM( None, 0, "No switching or temperature capability" )
  JD_ENUM( Switch, 1, "Switchable output" )
  JD_ENUM( Temperature, 2, "Temperature measurement" )
  JD_ENUM( Both, 3, "Switching and temperature measurement" )
  JD_ENUM( TCapabilityFlags_MAX, 4, "End marker" )
JD_ENUM_END( TCapabilityFlags )

#undef JD_ALIAS
#undef JD_ENUM_BEGIN
#undef JD_ENUM
#undef JD_ENUM_END

#ifdef GN_JSON_OWN_BIT_HOOK
  #undef JD_BIT
  #undef GN_JSON_OWN_BIT_HOOK
#endif
