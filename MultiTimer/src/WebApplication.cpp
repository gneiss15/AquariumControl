// WebApplication.cpp, Version: 2.58
#define Do_THttpResult
#define Do_TJsonConfigurationFiles
#define Do_TJsonSchedule
#define Do_TUdpLastError
#include <GnBasics.h>
#include "BuildConfig.h"
#if !defined( SYSTEM_ESP )
#include "WebApplication.h"
#include "ConfigurationTransaction.h"
#include <GnDeviceIdentity.h>
#include <GnUdpRxCounters.h>
#include <GnJsonResponses.h>
#include <cstdlib>
#include <fstream>
#include <json-c/json.h>
#include <sstream>

namespace
 {
bool ParseButtonActionPath( const TGNString & Path, unsigned & ChannelNr, bool & LongPress )
 {
  constexpr char Prefix[] = "/api/v1/button/";
  if ( Path.rfind( Prefix, 0 ) != 0 )
    return false;

  const TGNString Action = Path.substr( sizeof( Prefix ) - 1 );
  const size_t Separator = Action.find( '/' );
  if ( Separator == TGNString::npos || Separator == 0 || Separator + 1 >= Action.length() )
    return false;

  const TGNString ChannelText = Action.substr( 0, Separator );
  char * End = nullptr;
  const unsigned long Value = strtoul( ChannelText.c_str(), &End, 10 );
  if ( End == ChannelText.c_str() || *End != '\0' )
    return false;

  const TGNString Press = Action.substr( Separator + 1 );
  if ( Press == "short" )
    LongPress = false;
   else if ( Press == "long" )
    LongPress = true;
   else
    return false;

  ChannelNr = static_cast<unsigned>( Value );
  return true;
 }
 } // namespace

TWebApplication::TWebApplication( TScheduler & SchedulerRef, TButtonController & Buttons, TStatusApi & StatusApi,
                                  TChannelConfigs & Channels, TSwitchTimes & SwitchTimes,
                                  TSwitchIntervals & Intervals, TGNString DataDir, TTimeConfigSync & TimeConfigSync )
: SchedulerRef( SchedulerRef )
, Buttons( Buttons )
, StatusApi( StatusApi )
, Channels( Channels )
, SwitchTimes( SwitchTimes )
, Intervals( Intervals )
, DataDir( std::move( DataDir ) )
, TimeConfigSync( TimeConfigSync )
 {
  this->SchedulerRef.SaveLearnedChargeFactor = [this]( TGNString const & ProfileId, uint32_t Factor )
   {
    const TGNString FileName = this->DataDir + "/Channels.json";
    json_object * Root = json_object_from_file( FileName.c_str() );
    json_object * Profiles = nullptr;
    if( !Root || !json_object_object_get_ex( Root, "Profs", &Profiles ) )
     { if( Root ) json_object_put( Root ); return false; }
    bool Found = false;
    for( size_t I = 0; I < json_object_array_length( Profiles ); ++I )
     {
      json_object * Row = json_object_array_get_idx( Profiles, I ), * Id = nullptr;
      if( json_object_object_get_ex( Row, "Id", &Id ) && ProfileId == json_object_get_string( Id ) )
       { json_object_object_add( Row, "ChargeSecPerPct", json_object_new_int64( Factor ) ); Found = true; break; }
     }
    const TGNString Json = json_object_to_json_string_ext( Root, JSON_C_TO_STRING_PLAIN );
    json_object_put( Root );
    TChannelConfigs NewChannels; THeaterProfiles NewProfiles; TGNString Error;
    if( !Found || !ConfigStore.ChannelsFromJson( Json, NewChannels, NewProfiles, Error ) ) return false;
    const TGNString Schedule = "{\"SwitchTimes\":" + ConfigStore.SwitchTimesToJson( this->Channels, this->SwitchTimes ) +
                              ",\"Intervals\":" + ConfigStore.IntervalsToJson( this->Channels, this->Intervals ) + "}";
    if( !ConfigurationTransaction::Commit( this->DataDir, Json, Schedule, Error ) )
     { Logger.Write( TLogLevel::Error, Error ); return false; }
    this->SchedulerRef.SetHeaterProfiles( NewProfiles );
    this->TimeConfigSync.LocalChanged();
    return true;
   };
 }

THttpResponse TWebApplication::StaticFile( const TGNString & Path ) const
 {
  const TGNString Name = Path == "/" ? "/Index.html" : Path;
  const TGNString FileName = DataDir + Name;
  std::ifstream   File( FileName, std::ios::binary );
  if ( !File )
   {
    return { 404, TContentType::PlainText, "Not found" };
   }
  std::ostringstream Body;
  Body << File.rdbuf();
  const TContentType Type = Name.ends_with( ".js" ) ? TContentType::JavaScript :
                         Name.ends_with( ".css" ) ? TContentType::Css :
                         Name.ends_with( ".json" ) ? TContentType::Json : TContentType::Html;
  return { 200, Type, Body.str(), Name == "/StatusClient.js" };
 }

bool TWebApplication::ApplyRemoteTimes( const TGNString & Content, const TGNString & TimeStamp, TGNString & Error )
 {
  TJsonSchedule Update;
  if(!GnReadJsonValue(Content,Update) ||
     (!Update.Chs.Present && !Update.SwitchTimes.Present && !Update.Intervals.Present) ||
     (Update.Chs.Present && !Update.Chs.Known()))
   {Error="Invalid remote configuration";return false;}
  const bool HasChannels=Update.Chs.Known();
  TChannelConfigs NewChannels=Channels;
  THeaterProfiles NewProfiles;
  TGNString ChannelsJson;
  if(HasChannels)
   {
    ChannelsJson=GnWriteJson(Update.Chs.Value);
    if(!ConfigStore.ChannelsFromJson(ChannelsJson,NewChannels,NewProfiles,Error))return false;
   }
  TGNString SwitchJson,IntervalJson;
  // Preserve unchanged records, including comments and extension fields.
  if((!Update.SwitchTimes.Present || !Update.Intervals.Present) &&
     !ConfigStore.LoadScheduleJson(DataDir+"/SwitchTimes.json",SwitchJson,IntervalJson,Error))return false;
  if(Update.SwitchTimes.Present)SwitchJson=Update.SwitchTimes.IsNull ? "[]" : GnWriteJson(Update.SwitchTimes.Value);
  if(Update.Intervals.Present)IntervalJson=Update.Intervals.IsNull ? "[]" : GnWriteJson(Update.Intervals.Value);

  TSwitchTimes NewSwitchTimes;
  TSwitchIntervals NewIntervals;
  if( !ConfigStore.SwitchTimesFromJson( SwitchJson, NewChannels, NewSwitchTimes, Error ) ||
      !ConfigStore.IntervalsFromJson( IntervalJson, NewChannels, NewIntervals, Error ) )
    return false;
  std::vector<TChannelIndex> ChangedChannels;
  if( HasChannels )
   {
    // Function changes may never move the physical array slots or GPIO topology.
    if( NewChannels.size() != Channels.size() ) { Error = "Physical channel count changed"; return false; }
    for( size_t I = 0; I < Channels.size(); ++I )
     {
      const auto & Old = Channels[I]; const auto & New = NewChannels[I];
      if( Old.PhysPos != New.PhysPos || Old.InputPin != New.InputPin || Old.OutputPin != New.OutputPin ||
          Old.InputActiveHigh != New.InputActiveHigh || Old.OutputActiveHigh != New.OutputActiveHigh || Old.InputPullup != New.InputPullup )
       { Error = "Physical channel topology changed"; return false; }
      if( Old.Id != New.Id || Old.Role != New.Role || Old.Profile != New.Profile ) ChangedChannels.push_back( static_cast<TChannelIndex>( I ) );
     }
   }
  #if !defined( SYSTEM_ESP )
    if( HasChannels )
     {
      const TGNString Schedule = "{\"SwitchTimes\":" + SwitchJson + ",\"Intervals\":" + IntervalJson + "}";
      if( !ConfigurationTransaction::Commit( DataDir, ChannelsJson, Schedule, Error ) ) return false;
     }
    else if( !ConfigStore.SaveScheduleJson( DataDir + "/SwitchTimes.json", SwitchJson, IntervalJson, Error ) ) return false;
  #else
    if( HasChannels && !ConfigStore.SaveChannels( DataDir + "/Channels.json", ChannelsJson, Error ) ) return false;
    if( !ConfigStore.SaveScheduleJson( DataDir + "/SwitchTimes.json", SwitchJson, IntervalJson, Error ) ) return false;
  #endif
  for( auto Channel : ChangedChannels ) SchedulerRef.ResetFunctionState( Channel );
  if( HasChannels )
   {
    Channels = std::move( NewChannels );
    SchedulerRef.SetHeaterProfiles( NewProfiles );
   }
  SwitchTimes = std::move( NewSwitchTimes );
  Intervals = std::move( NewIntervals );
  SchedulerRef.SetSchedule( SwitchTimes, Intervals );
  if( !SchedulerRef.ReconstructState( Error ) ) return false;
  StatusApi.ClearConfigErrors();
  TimeConfigSync.RemoteApplied( TimeStamp );
  Logger.Write( TLogLevel::Info, "Remote SwitchTimes applied" );
  return true;
 }

THttpResponse TWebApplication::Handle( const TGNString & Method, const TGNString & Path, const TGNString & Body )
 {
  const bool ConfigurationPending = std::filesystem::exists( DataDir + "/Configuration.transaction.json" );
  if( Method == "GET" && Path == "/api/v1/config-revision" )
   {
    auto Root = json_object_new_object();
    json_object_object_add( Root, "Rev", json_object_new_string( TimeConfigSync.TimeStamp().c_str() ) );
    json_object_object_add( Root, "Pending", json_object_new_boolean( ConfigurationPending ) );
    const TGNString Text = json_object_to_json_string_ext( Root, JSON_C_TO_STRING_PLAIN ); json_object_put( Root );
    return { 200, TContentType::Json, Text };
   }
  if( ConfigurationPending && ( Path == "/Channels.json" || Path == "/SwitchTimes.json" || Path == "/Intervals.json" ||
      Path == "/SetChannels/" || Path == "/SetSwitchTimes/" || Path == "/SetIntervals/" ) )
    return { 409, TContentType::Json, GnJsonResult(false,"ConfigurationPending") };
  if( Path == "/api/v1/sources" && ( Method == "GET" || Method == "POST" ) )
   {
    const bool Locked = GnDashboardAvailable();
    if( Method == "POST" )
     {
      if( !GnDeviceIdentity.SetSources( Body, Locked ) )
        return { Locked ? 409 : 400, TContentType::Json,
          Locked ? "{\"Ok\":false,\"Err\":\"UseDashboard\"}" : "{\"Ok\":false,\"Err\":\"InvalidSourcesOrStorage\"}", true };
      TimeConfigSync.RequestSync();
      return { 200, TContentType::Json, "{\"Ok\":true}", true };
     }
    json_object * Identity = json_tokener_parse( GnDeviceIdentity.ReportJson().c_str() ), * Sources = nullptr;
    json_object_object_get_ex( Identity, "Srcs", &Sources );
    json_object * Result = json_object_new_object();
    json_object_object_add( Result, "Srcs", json_object_get( Sources ) );
    json_object_object_add( Result, "Editable", json_object_new_boolean( !Locked ) );
    const TGNString Text = json_object_to_json_string_ext( Result, JSON_C_TO_STRING_PLAIN );
    json_object_put( Result ); json_object_put( Identity );
    return { 200, TContentType::Json, Text, true };
   }
#if GN_DEBUG_UDP_RX
  if ( Method == "GET" && Path == "/api/v1/udp-rx-counters" )
    return { 200, TContentType::Json, GnUdpRxCountersJson() };
#endif
  if ( Method == "GET" && Path == "/api/v1/device" )
   {
    return { 200, TContentType::Json, StatusApi.DeviceJson(), true };
   }
  if ( Method == "GET" && Path == "/api/v1/status" )
   {
    return { 200, TContentType::Json, StatusApi.StatusJson(), true };
   }
  if ( Method == "OPTIONS" && Path.rfind( "/api/v1/", 0 ) == 0 )
   {
    return { 200, TContentType::PlainText, "", true };
   }
  if ( Method == "POST" && Path == "/api/v1/times" )
   {
    TGNString Response;
    const int Status = TimeConfigSync.HandleIncoming( Body, Response );
    return { Status, TContentType::Json, Response, true };
   }
  if ( Method == "POST" && Path == "/Restart/" )
   {
    Logger.Write( TLogLevel::Info, "MultiTimer restart requested by web UI" );
    return { 200, TContentType::PlainText, "1", false, true };
   }
  if ( Method == "POST" && Path.rfind( "/api/v1/button/", 0 ) == 0 )
   {
    unsigned  ChannelNr = 0;
    bool      LongPress = false;
    TGNString Error;
    if ( !ParseButtonActionPath( Path, ChannelNr, LongPress ) )
      return { 400, TContentType::PlainText, "Invalid button action path", true };
    if ( !Buttons.SimulatePress( ChannelNr, LongPress, Error ) )
      return { 400, TContentType::PlainText, Error, true };
    return { 200, TContentType::Json, "{\"Ok\":true}", true };
   }
  if ( Method == "GET" && ( Path == "/SwitchTimes.json" || Path == "/Intervals.json" ) )
   {
    TGNString SwitchTimesJson;
    TGNString IntervalsJson;
    TGNString Error;
    if( ConfigStore.LoadScheduleJson( DataDir + "/SwitchTimes.json", SwitchTimesJson, IntervalsJson, Error ) )
      return { 200, TContentType::Json, Path == "/SwitchTimes.json" ? SwitchTimesJson : IntervalsJson };
    return { 500, TContentType::PlainText, Error };
   }
  if ( Method == "POST" && Path == "/SetChannels/" )
   {
    TChannelConfigs NewChannels;
    THeaterProfiles NewProfiles;
    TGNString Error;
    if( !ConfigStore.ChannelsFromJson( Body, NewChannels, NewProfiles, Error ) )
     {
      Logger.Write( TLogLevel::Error, Error );
      return { 400, TContentType::PlainText, Error };
     }

    TSwitchTimes CheckedSwitchTimes;
    TSwitchIntervals CheckedIntervals;
    const TGNString SwitchTimesJson = ConfigStore.SwitchTimesToJson( Channels, SwitchTimes );
    const TGNString IntervalsJson = ConfigStore.IntervalsToJson( Channels, Intervals );
    if( !ConfigStore.SwitchTimesFromJson( SwitchTimesJson, NewChannels, CheckedSwitchTimes, Error ) ||
        !ConfigStore.IntervalsFromJson( IntervalsJson, NewChannels, CheckedIntervals, Error ) )
     {
      Error = "Channels.json conflicts with current schedule: " + Error;
      Logger.Write( TLogLevel::Error, Error );
      return { 400, TContentType::PlainText, Error };
     }

    const TGNString FileName = DataDir == "/" ? "/Channels.json" : DataDir + "/Channels.json";
    if( !ConfigStore.SaveChannels( FileName, Body, Error ) )
     {
      Logger.Write( TLogLevel::Error, Error );
      return { 500, TContentType::PlainText, Error };
     }
    TimeConfigSync.LocalChanged();
    Logger.Write( TLogLevel::Info, "Channels.json saved; restart required" );
    return { 200, TContentType::PlainText, "1" };
   }
  if ( Method == "POST" && Path == "/SetSwitchTimes/" )
   {
    TSwitchTimes NewSwitchTimes;
    TGNString               Error;
    if ( !ConfigStore.SwitchTimesFromJson( Body, Channels, NewSwitchTimes, Error ) )
     {
      Logger.Write( TLogLevel::Error, Error );
      return { 400, TContentType::PlainText, Error };
     }
    TGNString OldSwitchJson;
    TGNString IntervalJson;
    if( !ConfigStore.LoadScheduleJson( DataDir + "/SwitchTimes.json", OldSwitchJson, IntervalJson, Error ) ||
        !ConfigStore.SaveScheduleJson( DataDir + "/SwitchTimes.json", Body, IntervalJson, Error ) )
     {
      Logger.Write( TLogLevel::Error, Error );
      return { 500, TContentType::PlainText, Error };
     }
    StatusApi.ClearConfigErrors();
    SwitchTimes = std::move( NewSwitchTimes );
    SchedulerRef.SetSchedule( SwitchTimes, Intervals );
    if ( !SchedulerRef.ReconstructState( Error ) )
     {
      Logger.Write( TLogLevel::Error, Error );
      return { 500, TContentType::PlainText, Error };
     }
    Logger.Write( TLogLevel::Info, "SwitchTimes saved" );
    TimeConfigSync.LocalChanged();
    return { 200, TContentType::PlainText, "1" };
   }
  if ( Method == "POST" && Path == "/SetIntervals/" )
   {
    TSwitchIntervals NewIntervals;
    TGNString                   Error;
    if ( !ConfigStore.IntervalsFromJson( Body, Channels, NewIntervals, Error ) )
     {
      Logger.Write( TLogLevel::Error, Error );
      return { 400, TContentType::PlainText, Error };
     }
    TGNString SwitchJson;
    TGNString OldIntervalJson;
    if( !ConfigStore.LoadScheduleJson( DataDir + "/SwitchTimes.json", SwitchJson, OldIntervalJson, Error ) ||
        !ConfigStore.SaveScheduleJson( DataDir + "/SwitchTimes.json", SwitchJson, Body, Error ) )
     {
      Logger.Write( TLogLevel::Error, Error );
      return { 500, TContentType::PlainText, Error };
     }
    StatusApi.ClearConfigErrors();
    Intervals = std::move( NewIntervals );
    SchedulerRef.SetSchedule( SwitchTimes, Intervals );
    if ( !SchedulerRef.ReconstructState( Error ) )
     {
      Logger.Write( TLogLevel::Error, Error );
      return { 500, TContentType::PlainText, Error };
     }
    Logger.Write( TLogLevel::Info, "Intervals saved" );
    TimeConfigSync.LocalChanged();
    return { 200, TContentType::PlainText, "1" };
   }
  if ( Method == "GET" )
   {
    return StaticFile( Path );
   }
  return { 404, TContentType::PlainText, "Not found" };
 }
#endif
