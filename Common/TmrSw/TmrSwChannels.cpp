// TmrSwChannels.cpp, Version: 1.13
#define Do_TJsonChannels
#define Do_TJsonHeatConfig
#define Do_TJsonProfile
#include <GnBasics.h>
#include "TmrSwChannels.h"
#include <GnHeatPiJson.h>
#include <GnProfileModel.h>
#include <GnUdpStatus.h>
#include "TmrSwIncludes.h"
#include <cmath>

bool TTmrSwChannels::Parse( String const & Json, TTmrSwChannelConfig & Channel,
                            THeaterProfiles & Profiles, String & Error )
 {
  Error.clear();
  if( !GnHasHeapSpace( Json.length() + 512 ) ) { Error=F("NotEnoughSpace");return false; }
  TJsonChannels Document;
  if( !GnReadJsonValue( Json.c_str(), Document ) )
   { Error=F("Invalid Channels.json");return false; }
  THeaterProfiles NewProfiles;
  if( Document.Profs.Present && !Document.Profs.IsNull )
    for( auto const & Row : Document.Profs.Value )
     {
      THeaterProfile Profile;
      if(!GnApplyHeatProfile(Row,Profile)){Error="Invalid profile values";return false;}
      if( TGnHeaterControl::FindProfile( NewProfiles, Profile.Id ) )
       { Error=String(F("Duplicate heater profile ID: "))+Profile.Id.c_str();return false; }
      NewProfiles.push_back( std::move( Profile ) );
     }
  TTmrSwChannelConfig NewChannel;
  if( !Document.Chs.empty() )
   {
    auto const & Row=Document.Chs.front();
    NewChannel.Id=Row.Id;
    if( Row.Role.Present && !Row.Role.IsNull )TGnHeaterControl::ParseRole( Row.Role.Value, NewChannel.Role );
    if( Row.Prof.Present && !Row.Prof.IsNull )NewChannel.Profile=Row.Prof.Value;
    if( NewChannel.Role==THeaterRole::Timer && !NewChannel.Profile.empty() )
     { Error=F("Profile requires heater Role");return false; }
    if( NewChannel.Role!=THeaterRole::Timer &&
        (NewChannel.Profile.empty() || (!NewProfiles.empty() && !TGnHeaterControl::FindProfile(NewProfiles,NewChannel.Profile))) )
     { Error=F("Unknown or missing heater profile");return false; }
   }
  Channel=std::move(NewChannel);
  Profiles=std::move(NewProfiles);
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

// Change only the learned factor; preserve all extra profile and channel fields.
bool TTmrSwChannels::SaveLearnedChargeFactor( TGNString const & ProfileId, uint32_t Factor )
 {
  if( Factor == 0 ) return false;
  TJsonChannels Document;
  if( !GnReadJsonValue( ReadFile(F("/Channels.json")).c_str(), Document ) ||
      !Document.Profs.Present || Document.Profs.IsNull )return false;
  bool Found = false;
  for( auto & Row : Document.Profs.Value )
    if( ProfileId==Row.ProfName )
     {
      if( !Row.ChargeRunOn.Value )return false;
      Row.ChargeSecPerPct=Factor;Found=true;break;
     }
  if( !Found ) return false;
  const String Updated=GnWriteJson(Document).c_str();
  if( Updated.length()==0 )return false;
  TTmrSwChannelConfig Channel; THeaterProfiles Profiles; String Error;
  if( !Parse( Updated, Channel, Profiles, Error ) ) return false;
  File Out = LittleFS.open( "/Channels.json.new", "w" );
  if( !Out ) return false;
  const bool Written = Out.write( reinterpret_cast<uint8_t const *>( Updated.c_str() ), Updated.length() ) == Updated.length();
  Out.flush(); Out.close();
  if( !Written || !LittleFS.rename( "/Channels.json.new", "/Channels.json" ) ) return false;
  FProfiles = std::move( Profiles );
  return true;
 }
