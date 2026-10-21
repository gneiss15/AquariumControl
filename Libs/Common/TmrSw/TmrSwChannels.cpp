// TmrSwChannels.cpp, Version: 1.06
#include <GnBasics.h>
#include "TmrSwChannels.h"
#include <GnUdpStatus.h>
#include "TmrSwIncludes.h"
#include <cmath>

namespace
 {
bool RequiredString( JsonObjectConst Row, const char * Name, TGNString & Value )
 {
  const char * Text = Row[ Name ].as<const char *>();
  if( Text == nullptr || *Text == '\0' )
    return false;
  Value = Text;
  return true;
 }

bool RequiredFloat( JsonObjectConst Row, const char * Name, float & Value )
 {
  JsonVariantConst JsonValue = Row[ Name ];
  if( !JsonValue.is<float>() && !JsonValue.is<double>() && !JsonValue.is<int>() && !JsonValue.is<long>() )
    return false;
  const float Result = JsonValue.as<float>();
  if( !std::isfinite( Result ) )
    return false;
  Value = Result;
  return true;
 }
 }

bool TTmrSwChannels::Parse( String const & Json, TTmrSwChannelConfig & Channel,
                            THeaterProfiles & Profiles, String & Error )
 {
  Error.clear();
  if( !GnHasHeapSpace( Json.length() + 512 ) )
   {
    Error = F( "NotEnoughSpace" );
    return false;
   }

  JsonDocument Document;
  const DeserializationError JsonError = deserializeJson( Document, Json );
  JsonObjectConst Root = Document.as<JsonObjectConst>();
  JsonArrayConst Channels = Root[ "Channels" ].as<JsonArrayConst>();
  if( JsonError != DeserializationError::Ok || Root.isNull() || Channels.isNull() || Channels.size() > 1 )
   {
    Error = F( "Channels.json must contain zero or one channel" );
    return false;
   }

  THeaterProfiles NewProfiles;
  JsonVariantConst ProfilesValue = Root[ "Profiles" ];
  if( !ProfilesValue.isNull() )
   {
    JsonArrayConst ProfileArray = ProfilesValue.as<JsonArrayConst>();
    if( ProfileArray.isNull() )
     {
      Error = F( "Profiles must be an array" );
      return false;
     }
    if( !GnHasHeapSpace( ProfileArray.size() * sizeof( THeaterProfile ),
                         ProfileArray.size() * sizeof( THeaterProfile ) ) )
     {
      Error = F( "NotEnoughSpace" );
      return false;
     }
    NewProfiles.reserve( ProfileArray.size() );
    unsigned Index = 0;
    for( JsonVariantConst Value : ProfileArray )
     {
      JsonObjectConst Row = Value.as<JsonObjectConst>();
      THeaterProfile Profile;
      if( Row.isNull() || !RequiredString( Row, "Id", Profile.Id ) ||
          !RequiredString( Row, "TempId", Profile.TempId ) ||
          !RequiredFloat( Row, "FloorOn", Profile.FloorOn ) ||
          !RequiredFloat( Row, "FloorOff", Profile.FloorOff ) ||
          !RequiredFloat( Row, "MainOn", Profile.MainOn ) ||
          !RequiredFloat( Row, "MainOff", Profile.MainOff ) ||
          !Row[ "NoTempOn" ].is<bool>() )
       {
        Error = String( F( "Invalid heater profile in row " ) ) + ( Index + 1 );
        return false;
       }
      if( !Row[ "Measurement" ].isUnbound() )
       {
        if( !Row[ "Measurement" ].is<const char *>() ||
            !GnUdpMeasurementFieldValid( Row[ "Measurement" ].as<const char *>() ) )
         { Error = F( "Invalid profile Measurement" ); return false; }
        Profile.Measurement = Row[ "Measurement" ].as<const char *>();
       }
      if( TGnHeaterControl::FindProfile( NewProfiles, Profile.Id ) != nullptr )
       {
        Error = String( F( "Duplicate heater profile ID: " ) ) + Profile.Id.c_str();
        return false;
       }
      if( Profile.FloorOn >= Profile.FloorOff || Profile.MainOn >= Profile.MainOff )
       {
        Error = String( F( "Invalid heater hysteresis in profile " ) ) + Profile.Id.c_str();
        return false;
       }
      Profile.NoTempOn = Row[ "NoTempOn" ].as<bool>();
      NewProfiles.push_back( std::move( Profile ) );
      ++Index;
     }
   }

  TTmrSwChannelConfig NewChannel;
  if( Channels.size() == 1 )
   {
    JsonObjectConst Row = Channels[ 0 ].as<JsonObjectConst>();
    TGNString RoleText;
    if( Row.isNull() || !RequiredString( Row, "Id", NewChannel.Id ) )
     {
      Error = F( "Channel Id missing/invalid" );
      return false;
     }
    JsonVariantConst RoleValue = Row[ "Role" ];
    if( !RoleValue.isNull() )
     {
      if( !RequiredString( Row, "Role", RoleText ) || !TGnHeaterControl::ParseRole( RoleText, NewChannel.Role ) )
       {
        Error = F( "Invalid Role" );
        return false;
       }
     }
    JsonVariantConst ProfileValue = Row[ "Profile" ];
    if( !ProfileValue.isNull() )
     {
      const char * ProfileText = ProfileValue.as<const char *>();
      if( ProfileText == nullptr || *ProfileText == '\0' )
       {
        Error = F( "Invalid Profile" );
        return false;
       }
      NewChannel.Profile = ProfileText;
     }
    if( NewChannel.Role == THeaterRole::Timer )
     {
      if( !NewChannel.Profile.empty() )
       {
        Error = F( "Profile requires heater Role" );
        return false;
       }
     }
    else
     {
      if( NewChannel.Profile.empty() )
       {
        Error = F( "Profile missing for heater channel" );
        return false;
       }
      if( TGnHeaterControl::FindProfile( NewProfiles, NewChannel.Profile ) == nullptr && !NewProfiles.empty() )
       {
        Error = String( F( "Unknown heater profile: " ) ) + NewChannel.Profile.c_str();
        return false;
       }
     }
   }

  Channel = std::move( NewChannel );
  Profiles = std::move( NewProfiles );
  return true;
 }

bool TTmrSwChannels::Setup(void)
 {
  const String Json = ReadFile( F( "/Channels.json" ) );
  String Error;
  TTmrSwChannelConfig NewChannel;
  THeaterProfiles NewProfiles;
  if( !Parse( Json, NewChannel, NewProfiles, Error ) )
   {
    Debug( "Channels.json: %s\n", Error.c_str() );
    return false;
   }
  FChannel = std::move( NewChannel );
  FProfiles = std::move( NewProfiles );
  return true;
 }

String TTmrSwChannels::Set( String const & Json )
 {
  String Error;
  TTmrSwChannelConfig NewChannel;
  THeaterProfiles NewProfiles;
  if( !Parse( Json, NewChannel, NewProfiles, Error ) )
    return Error;

  File FileOut = LittleFS.open( "/Channels.json", "w" );
  if( !FileOut )
    return F( "WriteFailed" );
  const size_t Written = FileOut.write( reinterpret_cast<const uint8_t *>( Json.c_str() ), Json.length() );
  FileOut.close();
  if( Written != Json.length() )
    return F( "WriteFailed" );

  FChannel = std::move( NewChannel );
  FProfiles = std::move( NewProfiles );
  return F( "1" );
 }
