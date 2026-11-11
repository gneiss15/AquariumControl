// GnUdpProtocol.h, Version: 1.03
#pragma once
#define JD(...)
#define JD_CONST(Type, Name, Value) GN_UDP_CONST_##Name(Value)
#define GN_UDP_CONST_Ver(Value) constexpr unsigned GnUdpStatusVersion=Value;
#define GN_UDP_CONST_Type(Value)
#pragma push_macro("Do_TUdpRequestMessage")
#ifndef Do_TUdpRequestMessage
  #define Do_TUdpRequestMessage
#endif
#include "Definitions/Contract/TUdpRequestMessage.inc.h"
#pragma pop_macro("Do_TUdpRequestMessage")
#include "GnJsonMacroCleanup.inc.h"
#undef GN_UDP_CONST_Type
#undef GN_UDP_CONST_Ver

inline bool GnUdpStatusVersionSupported(unsigned Version)
 {return Version==GnUdpStatusVersion;}
