// GnFunctionConfig.h, Version: 1.03
#pragma once
#include <GnBasics.h>
#if defined( GNPlatformESP )
#include <GnJsonSchema.h>

// Apply only the explicitly function-bound fields. Network/hardware fields stay.
template<class TConfig, class TSave>
String GnApplyFunctionConfig( char const * Envelope, TConfig & Config, TSave Save )
 {
  TJsonFunctionEnvelope EnvelopeData;
  if( !GnReadJsonValue( Envelope, EnvelopeData ) )return F( "InvalidFunctionConfig" );
  if( !EnvelopeData.FuncCfg.Present )return F( "1" );
  auto const & Values = EnvelopeData.FuncCfg.Value;
  const TConfig Previous = Config;
  TConfig Next = Config;
  bool Changed = false;
  #define GN_FUNCTION_CONFIG_FIELD( Name ) \
    if(!Values.Name.Known())return F("InvalidFunctionConfig"); \
    Next.Name = Values.Name.Value.c_str(); Changed = Changed || Next.Name != Config.Name;
  #include <GnFunctionConfig.inc.h>
  if( !Changed ) return F( "1" );
  Config = Next;
  if( !Save() ) { Config = Previous; return F( "ConfigStorageFailed" ); }
  return F( "Restart" );
 }
#endif
