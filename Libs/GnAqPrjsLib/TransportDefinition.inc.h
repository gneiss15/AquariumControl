// TransportDefinition.inc.h, Version: 1.00
// Shared Aquarium UDP destination. Repeated definition, no include guard.
#ifndef JD_SETTING
  #error Define JD_SETTING before including TransportDefinition.inc.h
#endif
JD_SETTING( TGNString, BroadcastAddress, "255.255.255.255" )
JD_SETTING( uint16_t, UdpPort, 44444 )
JD_SETTING( uint32_t, SendIntervalSeconds, 10 )
#undef JD_SETTING
