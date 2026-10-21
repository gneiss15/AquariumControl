// GnMemory.cpp, Version: 1.00
#include "GnMemory.h"

#if defined( GNPlatformESP )
  #include <Arduino.h>
  #include <stdlib.h>
#endif

namespace
 {
#if defined( GNPlatformESP8266 )
constexpr size_t HeapReserveBytes = 2048;
#elif defined( GNPlatformESP32 )
constexpr size_t HeapReserveBytes = 8192;
#endif
 } // namespace

bool GnHasHeapSpace( size_t RequiredBytes, size_t LargestBlockBytes )
 {
#if defined( GNPlatformESP )
  if ( LargestBlockBytes > RequiredBytes )
    RequiredBytes = LargestBlockBytes;
  const size_t FreeHeap = ESP.getFreeHeap();
  if ( RequiredBytes > FreeHeap || HeapReserveBytes > FreeHeap - RequiredBytes )
    return false;

  if ( LargestBlockBytes == 0 )
    return true;

  void * Probe = malloc( LargestBlockBytes );
  if ( Probe == nullptr )
    return false;
  free( Probe );
  return true;
#else
  (void)RequiredBytes;
  (void)LargestBlockBytes;
  return true;
#endif
 }
