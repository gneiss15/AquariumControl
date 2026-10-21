// FeederConfig.cpp, Version: 2.47
//****************************************************************
// Includes
//****************************************************************

#include "FeederConfig.h"


namespace
 {
TGNString AlignedObjectJson( JsonDocument const & Doc )
 {
  const JsonObjectConst Object = Doc.as<JsonObjectConst>();
  size_t KeyWidth = 0;
  for( JsonPairConst Pair : Object )
   {
    const size_t Width = strlen( Pair.key().c_str() ) + 3; // quotes + colon
    if( Width > KeyWidth )
      KeyWidth = Width;
   }

  TGNString Result;
  const auto Build = [&]( auto & Output )
   {
    Output += "{\n";
    size_t Index = 0;
    for( JsonPairConst Pair : Object )
     {
      const char * const Key = Pair.key().c_str();
      const size_t KeyLength = strlen( Key ) + 3;
      Output += "  \"";
      Output += Key;
      Output += "\":";
      Output.append( KeyWidth - KeyLength, ' ' );
      Output += ' ';
      const size_t ValueLength = measureJson( Pair.value() );
      GnAppendGenerated( Output, ValueLength, [&]( TGNString & Target )
       {
        TGnStringAppendWriter Writer( Target );
        serializeJson( Pair.value(), Writer );
       } );
      if( ++Index < Object.size() )
        Output += ',';
      Output += '\n';
     }
    Output += "}\n";
   };

  if( !GnBuildStringExact( Result, Build ) )
    return TGNString();
  return Result;
 }
} // namespace

//****************************************************************
// Default-Config
//****************************************************************

static TConfig DefaultConfig =
 {
  #define CD( htmlType, typ, name, defValue, ... ) defValue,
  #include "FeederConfig.inc.h"
 };

//****************************************************************
// TFeederConfig
//****************************************************************

TFeederConfig::TFeederConfig(void)
 {
  FConfig = DefaultConfig;
 }

bool TFeederConfig::Setup(void)
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

bool TFeederConfig::Load( String const jsonStr )
 {
  JsonDocument doc;
  DeserializationError error = deserializeJson( doc, jsonStr );
  //D Debug( "TFeederConfig::Load: %s\njson: %s<<<\n", error.c_str(), jsonStr.c_str() );
  if( error != DeserializationError::Ok || !doc.is<JsonObject>() )
    return false;
  TConfig config = DefaultConfig;
  #define CD( htmlType, typ, name, defValue, ... ) if( !LoadValue( doc[ #name ], config.name ) ) return false;
  #include "FeederConfig.inc.h"
  FConfig = config;
  return true;
 }

bool TFeederConfig::Save(void)
 {
  JsonDocument doc;
  #define CD( htmlType, typ, name, defValue, ... ) SaveValue( doc[ #name ], FConfig.name );
  #include "FeederConfig.inc.h"
  if( doc.overflowed() )
   {
    Debug( "Config save failed: JSON document overflow\n" );
    return false;
   }

  const TGNString Json = AlignedObjectJson( doc );
  if( Json.length() == 0 )
   {
    Debug( "Config save failed: JSON serialization returned empty data\n" );
    return false;
   }

  TGNString Error;
  if( !WriteLittleFSTemporary( "/Config.json", Json, Error ) )
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

bool TFeederConfig::Set( String const jsonStr )
 {
  const TConfig Previous = FConfig;
  if( !Load( jsonStr ) )
    return false;
  if( Save() )
    return true;
  FConfig = Previous;
  return false;
 }
