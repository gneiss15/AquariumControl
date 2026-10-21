// GnPlatform.h, Version: 1.02
#ifndef GnPlatformH  // sentry
#define GnPlatformH
#pragma once

//****************************************************************
// Platform detection
//****************************************************************

#if defined( ESP8266 )
  #define GNPlatformArduino
  #define GNPlatformESP
  #define GNPlatformESP8266
#elif defined( ESP32 )
  #define GNPlatformArduino
  #define GNPlatformESP
  #define GNPlatformESP32
#elif defined( ARDUINO_ARCH_AVR ) || defined( __AVR__ )
  #define GNPlatformArduino
  #define GNPlatformAVR
#elif defined( GNTargetRasPi )
  #define GNPlatformLinux
  #define GNPlatformRasPi
#elif defined( __linux__ )
  #define GNPlatformLinux
  #define GNPlatformPC
#else
  #error "Unsupported platform"
#endif

//****************************************************************
// ESP flash-string compatibility on Linux
//****************************************************************

#if defined( GNPlatformLinux )
  #ifndef PROGMEM
    #define PROGMEM
  #endif
  #ifndef PSTR
    #define PSTR( Text ) ( Text )
  #endif
  #ifndef FPSTR
    #define FPSTR( Pointer ) ( Pointer )
  #endif
  #ifndef F
    #define F( Text ) ( Text )
  #endif
  #ifndef pgm_read_byte
    #define pgm_read_byte( Address ) ( *(reinterpret_cast<unsigned char const *>( Address )) )
  #endif
#endif

#endif // GnPlatformH
