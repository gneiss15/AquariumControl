// GnFunctionConfig.h, Version: 1.00
#pragma once
#include <GnBasics.h>
#if defined( GNPlatformESP )
#include <ArduinoJson.h>

// Apply only the explicitly function-bound fields. Network/hardware fields stay.
template<class TConfig, class TSave>
String GnApplyFunctionConfig( char const * Envelope, TConfig & Config, TSave Save )
 {
  JsonDocument Root;
  if( deserializeJson( Root, Envelope ) ) return F( "InvalidAssignment" );
  JsonVariantConst Values = Root["FunctionConfig"];
  if( Values.isUnbound() ) return F( "1" );
  if( !Values.is<JsonObjectConst>() || Values.size() != 4 ) return F( "InvalidFunctionConfig" );
  const TConfig Previous = Config;
  TConfig Next = Config;
  bool Changed = false;
  #define GN_FUNCTION_CONFIG_FIELD( Name ) \
    if( !Values[#Name].is<char const *>() || strlen( Values[#Name].as<char const *>() ) > 1024 ) return F( "InvalidFunctionConfig" ); \
    Next.Name = Values[#Name].as<char const *>(); Changed = Changed || Next.Name != Config.Name;
  #include <GnFunctionConfig.inc.h>
  if( Next.Hostname.length() == 0 || Next.Hostname.length() > 63 || Next.ApSsid.length() > 32 ||
      ( Next.ApPw.length() != 0 && ( Next.ApPw.length() < 8 || Next.ApPw.length() > 63 ) ) ) return F( "InvalidFunctionConfig" );
  if( !Changed ) return F( "1" );
  Config = Next;
  if( !Save() ) { Config = Previous; return F( "ConfigStorageFailed" ); }
  return F( "Restart" );
 }
#endif
