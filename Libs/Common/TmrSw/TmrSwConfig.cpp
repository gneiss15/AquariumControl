// TmrSwConfig.cpp, Version: 1.07
//****************************************************************
// Includes
//****************************************************************

#include "TmrSwConfig.h"

/*
#include "TmrSwConfig.h"
#include "TmrSwHw.h"
#include "SwitchTimes.h"
#include "TmrSwWifi.h"
*/

namespace
 {
TGNString AlignedObjectJson( JsonDocument const & doc )
 {
  const JsonObjectConst object = doc.as<JsonObjectConst>();
  size_t keyWidth = 0;
  for( JsonPairConst pair : object )
   {
    const size_t width = strlen( pair.key().c_str() ) + 3; // quotes + colon
    if( width > keyWidth )
      keyWidth = width;
   }

  TGNString result;
  const auto build = [&]( auto & output )
   {
    output += "{\n";
    size_t index = 0;
    for( JsonPairConst pair : object )
     {
      const char * const key = pair.key().c_str();
      const size_t keyLength = strlen( key ) + 3;
      output += "  \"";
      output += key;
      output += "\":";
      output.append( keyWidth - keyLength, ' ' );
      output += ' ';
      const size_t valueLength = measureJson( pair.value() );
      GnAppendGenerated( output, valueLength, [&]( TGNString & target )
       {
        TGnStringAppendWriter Writer( target );
        serializeJson( pair.value(), Writer );
       } );
      if( ++index < object.size() )
        output += ',';
      output += '\n';
     }
    output += "}\n";
   };

  if( !GnBuildStringExact( result, build ) )
    return TGNString();
  return result;
 }
} // namespace

//****************************************************************
// Default-Config
//****************************************************************

static TConfig DefaultConfig =
 {
  #define CD( htmlType, typ, name, defValue, ... ) defValue,
  #include "TmrSwConfig.inc.h"
 };

//****************************************************************
// TTmrSwConfig
//****************************************************************

TTmrSwConfig::TTmrSwConfig(void)
 {
  FConfig = DefaultConfig;
 }

bool TTmrSwConfig::Setup(void)
 {
  DiscardLittleFSTemporary( "/Config.json" );
  if( Load( ReadFile( F( "/Config.json" ) ) ) )
    return true;

  Debug( "Config.json invalid, trying backup\n" );
  if( !Load( ReadFile( "/Config.json.bak" ) ) )
    return false;

  TGNString Error;
  if( !RestoreLittleFSBackup( "/Config.json", Error ) )
   {
    Debug( "Config backup restore failed: %s\n", Error.c_str() );
    return false;
   }
  Debug( "Config.json restored from backup\n" );
  return true;
 }

bool TTmrSwConfig::Load( String const jsonStr )
 {
  JsonDocument doc;
  DeserializationError error = deserializeJson( doc, jsonStr );
  //D Debug( "TTmrSwConfig::Load: %s, bool:%s\njson: %s<<<\n", error ? "NOK" : "OK", error.c_str(), jsonStr.c_str() ); //D 
  if( error != DeserializationError::Ok || !doc.is<JsonObject>() ) // if( error ) ???
    return false;
  TConfig config = DefaultConfig;
  #define CD( htmlType, typ, name, defValue, ... ) if( !doc[ #name ].is<typ>() ) return false; config.name = doc[ #name ].as<typ>();
  #include "TmrSwConfig.inc.h"
  FConfig = config;
  //D Debug( "TTmrSwConfig::Load: WifiMode: %u\n", FConfig.WifiMode ); //D
  return true;
 }

bool TTmrSwConfig::Save(void)
 {
  JsonDocument doc;
  #define CD( htmlType, typ, name, defValue, ... ) doc[ #name ] = FConfig.name;
  #include "TmrSwConfig.inc.h"
  if( doc.overflowed() )
   {
    Debug( "Config save failed: JSON document overflow\n" );
    return false;
   }

  const TGNString json = AlignedObjectJson( doc );
  if( json.length() == 0 )
   {
    Debug( "Config save failed: JSON serialization returned empty data\n" );
    return false;
   }

  TGNString Error;
  if( !WriteLittleFSTemporary( "/Config.json", json, Error ) )
   {
    Debug( "Config save failed: %s\n", Error.c_str() );
    return false;
   }

  const TConfig CurrentConfig = FConfig;
  if( !Load( ReadFile( "/Config.json.tmp" ) ) )
   {
    FConfig = CurrentConfig;
    DiscardLittleFSTemporary( "/Config.json" );
    Debug( "Config save failed: temporary Config.json does not pass loader validation\n" );
    return false;
   }
  FConfig = CurrentConfig;

  if( !CommitLittleFSTemporary( "/Config.json", Error ) )
   {
    Debug( "Config save failed: %s\n", Error.c_str() );
    return false;
   }
  return true;
 }

bool TTmrSwConfig::Set( String const jsonStr )
 {
  const TConfig Previous = FConfig;
  if( !Load( jsonStr ) )
    return false;
  if( Save() )
    return true;
  FConfig = Previous;
  return false;
 }

void TTmrSwConfig::Reset(void)
 {
  FConfig = DefaultConfig;
  ESP.restart();
 }
