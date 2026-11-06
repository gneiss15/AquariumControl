// BuildConfig.h, Version: 2.54
#pragma once

#include "../Version.inc.h"

constexpr char const * MultiTimerVersion = ProjectVersion::FirmwareRevision;

#if ( defined( ESP32 ) || defined( ESP8266 ) ) && !defined( SYSTEM_ESP )
  #define SYSTEM_ESP
#endif

#if !defined( SYSTEM_DUMMY ) && !defined( SYSTEM_RASPI ) && !defined( SYSTEM_ESP )
  #define SYSTEM_DUMMY
#endif

#if ( defined( SYSTEM_DUMMY ) + defined( SYSTEM_RASPI ) + defined( SYSTEM_ESP ) ) != 1
  #error Select exactly one target system
#endif

constexpr unsigned MaxEspChannels = 64;

#ifndef GN_DEBUG_FILE_LOG
  #define GN_DEBUG_FILE_LOG 0
#endif
