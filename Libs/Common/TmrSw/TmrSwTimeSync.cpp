// TmrSwTimeSync.cpp, Version: 1.15
#include <GnBasics.h>
#include "TmrSwTimeSync.h"
#include <GnDeviceIdentity.h>
#include <GnFunctionConfig.h>
#include "TmrSwConfig.h"

#include "SwitchTimes.h"
#include "TmrSwChannels.h"

TGnTimeSyncConfig const TmrSwTimeSyncConfig =
 {
  "/SwitchTimes.sync.json",
  "/SwitchTimes.json",
  "SwitchTimes",
  "TmrSw",
  4000,
  "/Channels.json",
  "/Intervals.json",
  TmrSwTimeSyncFunctionConfig
 };

namespace
 {
constexpr char TransactionFile[] = "/TimeConfig.transaction.json";
bool WriteConfiguration( char const * Name, String const & Text )
 {
  // Keep the journal temporary basename within legacy LittleFS name_max=32.
  const String Temporary = String( Name ) + ".new";
  File Out = LittleFS.open( Temporary.c_str(), "w" );
  if( !Out ) return false;
  const bool Ok = Out.write( reinterpret_cast<uint8_t const *>( Text.c_str() ), Text.length() ) == Text.length();
  Out.flush(); Out.close();
  return Ok && LittleFS.rename( Temporary.c_str(), Name );
 }
bool MaterializeConfiguration( JsonDocument const & Document )
 {
  for( char const * Name : { "/Channels.json", "/SwitchTimes.json", "/Intervals.json" } )
   {
    if( !Document[Name].is<const char *>() ) return false;
    if( !WriteConfiguration( Name, Document[Name].as<String>() ) ) return false;
   }
  return true;
 }
 }

bool TmrSwRecoverConfiguration(void)
 {
  if( !LittleFS.exists( TransactionFile ) ) return true;
  JsonDocument Document;
  File In = LittleFS.open( TransactionFile, "r" );
  const bool Parsed = In && deserializeJson( Document, In ) == DeserializationError::Ok;
  In.close();
  if( !Parsed ) return false;
  String Error; TTmrSwChannelConfig Channel; THeaterProfiles Profiles;
  if( !TTmrSwChannels::Validate( Document["/Channels.json"].as<String>(), Channel, Profiles, Error ) ||
      TSwitchTimes::ValidateBundle( Document["/SwitchTimes.json"].as<String>(), Document["/Intervals.json"].as<String>(), Channel.Id.c_str() ) != "1" ) return false;
  return MaterializeConfiguration( Document ) && LittleFS.remove( TransactionFile );
 }

String TmrSwTimeSyncApply( char const * Content )
 {
  JsonDocument Document;
  if( deserializeJson( Document, Content ) )
    return F( "InvalidContent" );
  if( Document.is<JsonArrayConst>() )
   {
    if( Document.size() != 0 )
      return F( "InvalidContent" );
    const String SwitchTimesResult = SwitchTimes.Set( "[]" );
    if( SwitchTimesResult != "1" ) return SwitchTimesResult;
    return SwitchTimes.SetIntervals( "[]" );
   }
  if( !Document.is<JsonObjectConst>() )
    return F( "InvalidContent" );
  JsonVariantConst ChannelConfigJson = Document[ "Channels" ];
  JsonVariantConst SwitchTimesJson = Document[ "SwitchTimes" ];
  JsonVariantConst IntervalsJson = Document[ "Intervals" ];
  if( ( !ChannelConfigJson.isUnbound() && !ChannelConfigJson.is<JsonObjectConst>() ) ||
      ( !SwitchTimesJson.isUnbound() && !SwitchTimesJson.is<JsonArrayConst>() ) ||
      ( !IntervalsJson.isUnbound() && !IntervalsJson.is<JsonArrayConst>() ) )
    return F( "InvalidContent" );
  if( ChannelConfigJson.is<JsonObjectConst>() && SwitchTimesJson.is<JsonArrayConst>() && IntervalsJson.is<JsonArrayConst>() )
   {
    String Channels, Schedule, Intervals, Error;
    serializeJson( ChannelConfigJson, Channels ); serializeJson( SwitchTimesJson, Schedule ); serializeJson( IntervalsJson, Intervals );
    TTmrSwChannelConfig Channel; THeaterProfiles Profiles;
    if( !TTmrSwChannels::Validate( Channels, Channel, Profiles, Error ) ) return Error;
    const String Checked = TSwitchTimes::ValidateBundle( Schedule, Intervals, Channel.Id.c_str() );
    if( Checked != "1" ) return Checked;
    const bool Changed = TmrSwChannels.Channel().Id != Channel.Id || TmrSwChannels.Channel().Role != Channel.Role || TmrSwChannels.Channel().Profile != Channel.Profile;
    JsonDocument Transaction;
    Transaction["/Channels.json"] = Channels; Transaction["/SwitchTimes.json"] = Schedule; Transaction["/Intervals.json"] = Intervals;
    String Journal; serializeJson( Transaction, Journal );
    if( !WriteConfiguration( TransactionFile, Journal ) || !MaterializeConfiguration( Transaction ) ) return F( "ConfigurationCommitFailed" );
    if( !TmrSwChannels.Setup() ) return F( "ConfigurationLoadFailed" );
    if( Changed || GnDeviceIdentity.FunctionChanged() ) SwitchTimes.ResetFunctionState();
    const String Loaded = SwitchTimes.LoadBundle( Schedule, Intervals );
    if( Loaded != "1" ) return Loaded;
    if( !LittleFS.remove( TransactionFile ) ) return F( "ConfigurationCompletionFailed" );
    return "1";
   }
  bool Applied = false;
  if( ChannelConfigJson.is<JsonObjectConst>() )
   {
    String ChannelsJson;
    serializeJson( Document[ "Channels" ], ChannelsJson );
    const String Result = TmrSwChannels.Set( ChannelsJson );
    if( Result != "1" ) return Result;
    SwitchTimes.RefreshProfiles();
    Applied = true;
   }
  if( SwitchTimesJson.is<JsonArrayConst>() )
   {
    String ScheduleJson;
    serializeJson( SwitchTimesJson, ScheduleJson );
    const String Result = SwitchTimes.Set( ScheduleJson );
    if( Result != "1" ) return Result;
    Applied = true;
   }
  if( IntervalsJson.is<JsonArrayConst>() )
   {
    String Intervals;
    serializeJson( IntervalsJson, Intervals );
    const String Result = SwitchTimes.SetIntervals( Intervals );
    if( Result != "1" ) return Result;
    Applied = true;
   }
  if( Applied && GnDeviceIdentity.FunctionChanged() ) SwitchTimes.ResetFunctionState();
  return Applied ? String( "1" ) : String( "InvalidContent" );
 }

String TmrSwTimeSyncFunctionConfig( char const * Envelope )
 {
  return GnApplyFunctionConfig( Envelope, TmrSwConfig,
    [](){ return TmrSwConfigInstance.Save(); } );
 }
