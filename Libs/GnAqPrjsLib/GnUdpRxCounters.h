// GnUdpRxCounters.h, Version: 1.02
#pragma once
#include <GnBasics.h>
#include <GnTemporaryDiagnostics.h>

#if GN_DEBUG_UDP_RX
// Native MultiTimer serves HTTP on another thread. ESP serves it in the device loop.
#if !defined(GNPlatformESP)
#include <atomic>
using TGnUdpRxCounter=std::atomic<uint32_t>;
#else
using TGnUdpRxCounter=uint32_t;
#endif
inline TGnUdpRxCounter GnUdpInvalidContentCount{0};

inline TGNString GnUdpRxCountersJson(void)
 {
  return "{\"InvalidContent\":"+std::to_string(uint32_t(GnUdpInvalidContentCount))+"}";
 }
#endif
