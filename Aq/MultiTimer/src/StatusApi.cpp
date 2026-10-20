// StatusApi.cpp, Version: 2.47
#include <GnBasics.h>
#include "BuildConfig.h"
#include "Logger.h"
#include "StatusApi.h"
#include <GnDeviceIdentity.h>
#include "TimeSync.h"
#include <cstdio>
#include <utility>
#include <vector>

class TChannelApiStatus
 {
 private:
  bool      FInputValid = false;
  bool      FInputState = false;
  TGNString FError;

 public:
  void              Init( bool inputValid, bool inputState, TGNString error );
  bool              InputValid(void) const { return FInputValid; }
  bool              InputState(void) const { return FInputState; }
  const TGNString & Error(void) const { return FError; }
 };

void TChannelApiStatus::Init( bool inputValid, bool inputState, TGNString error )
 {
  FInputValid = inputValid;
  FInputState = inputState;
  FError = std::move( error );
 }

TStatusApi::TStatusApi( TIHardware & hardware, TScheduler & scheduler,
                        const TChannelConfigs & channels, TGNString deviceId, TGNString name,
                        unsigned webSocketPort, std::vector<TGNString> configErrors )
: FHardware( hardware )
, FScheduler( scheduler )
, FChannels( channels )
, FDeviceId( std::move( deviceId ) )
, FName( std::move( name ) )
, FWebSocketPort( webSocketPort )
, FConfigErrors( std::move( configErrors ) )
, FOnlineSince( std::time( nullptr ) )
 {
 }

void TStatusApi::ClearConfigErrors(void)
 {
  std::lock_guard<std::mutex> lock( FStatusMutex );
  FConfigErrors.clear();
 }

const char * TStatusApi::Platform(void)
 {
  #if defined( SYSTEM_ESP )
    return "ESP";
  #elif defined( SYSTEM_RASPI )
    return "RasPi";
  #else
    return "Dummy";
  #endif
 }

void TStatusApi::AppendJsonString( TGNString & json, const TGNString & value )
 {
  json += '"';
  for( const char character : value )
   {
    switch( character )
     {
      case '"':
        json += "\\\"";
        break;
      case '\\':
        json += "\\\\";
        break;
      case '\b':
        json += "\\b";
        break;
      case '\f':
        json += "\\f";
        break;
      case '\n':
        json += "\\n";
        break;
      case '\r':
        json += "\\r";
        break;
      case '\t':
        json += "\\t";
        break;
      default:
        if( static_cast<unsigned char>( character ) < 0x20 )
         {
          char escape[ 7 ];
          std::snprintf( escape, sizeof( escape ), "\\u%04x", static_cast<unsigned char>( character ) );
          json += escape;
         }
        else
          json += character;
        break;
     }
   }
  json += '"';
 }

void TStatusApi::AppendInteger( TGNString & json, long long value )
 {
  char text[ 32 ];
  std::snprintf( text, sizeof( text ), "%lld", value );
  json += text;
 }

TGNString TStatusApi::DeviceJson(void) const
 {
  TGNString json = "{\"ApiVersion\":";
  AppendInteger( json, FApiVersion );
  json += ",\"DeviceType\":";
  AppendJsonString( json, FDeviceType );
  json += ",\"Platform\":";
  AppendJsonString( json, Platform() );
  json += ",\"DeviceId\":";
  AppendJsonString( json, GnDeviceIdentity.WireId() );
  json += ",\"Identity\":" + GnDeviceIdentity.ReportJson();
  json += ",\"Name\":";
  AppendJsonString( json, FName );
  json += ",\"FirmwareVersion\":";
  AppendJsonString( json, MultiTimerVersion );
  json += ",\"EndpointAssignments\":1,\"Capabilities\":[\"Status\",\"OverrideStatus\",\"NextChange\",\"ErrorStatus\",\"WebSocket\",\"PollingFallback\",\"ConfigPage\",\"ButtonActions\"],\"ConfigUrl\":\"/\",\"Actions\":{\"ButtonPress\":{\"Path\":\"/api/v1/button/{ChannelId}/{Press}\",\"Presses\":[\"Short\",\"Long\"]}},\"Events\":{\"Protocol\":\"ws\",\"Port\":";
  AppendInteger( json, FWebSocketPort );
  json += ",\"Path\":\"/api/v1/events\",\"PollingFallbackSeconds\":";
  AppendInteger( json, FPollingFallbackSeconds );
  json += "},\"StatusClientUrl\":\"/StatusClient.js\"}";
  return json;
 }

void TStatusApi::UpdateTimeSyncStatus(void)
 {
  if( FTimeSyncCached && FTimeSyncCacheTimer.Elapsed() < FTimeSyncCacheMs )
    return;

  FCachedTimeSyncState = TTimeSync::State();
  FTimeSyncCached = true;
  FTimeSyncCacheTimer.Start();
 }

TGNString TStatusApi::StatusJson(void)
 {
  return BuildStatusJson( nullptr );
 }

uint64_t TStatusApi::StatusSignature( TGNString const & json )
 {
  constexpr char marker[] = ",\"CurrentTime\":";
  const size_t valuePos = json.find( marker );
  if( valuePos == TGNString::npos )
    return GnHash64( json.data(), json.length() );

  const size_t numberPos = valuePos + sizeof( marker ) - 1;
  const size_t numberEnd = json.find( ',', numberPos );
  if( numberEnd == TGNString::npos )
    return GnHash64( json.data(), numberPos );

  uint64_t signature = GnHash64( json.data(), numberPos );
  return GnHash64( json.data() + numberEnd, json.length() - numberEnd, signature );
 }

TGNString TStatusApi::StatusJson( uint64_t & signature )
 {
  return BuildStatusJson( &signature );
 }

TGNString TStatusApi::BuildStatusJson( uint64_t * signature )
 {
  std::lock_guard<std::mutex> lock( FStatusMutex );
  UpdateTimeSyncStatus();

  const bool timeSynchronized = FCachedTimeSyncState == TTimeSyncState::Synchronized;
  const TSchedulerChannelStati schedulerStati = FScheduler.ChannelStati();

  std::vector<TChannelApiStatus> channelStati( FChannels.size() );
  std::vector<TGNString> errors = FConfigErrors;
  if( !timeSynchronized )
    errors.push_back( TTimeSync::Error( FCachedTimeSyncState ) );

  for( unsigned channelNr = 0;  channelNr < FChannels.size();  ++channelNr )
   {
    const TChannelConfig & channel = FChannels[ channelNr ];
    TChannelApiStatus & status = channelStati[ channelNr ];
    TGNString error;

    if( channel.InputPin )
     {
      bool inputState = false;
      const bool inputValid = FHardware.GetPin( channelNr, inputState, error );
      status.Init( inputValid, inputState, inputValid ? TGNString() : "Input: " + error );
     }
    if( !status.Error().empty() )
      errors.push_back( channel.Id + ": " + status.Error() );
   }

  TGNString lastErrorMessage;
  std::time_t lastErrorTime = 0;
  const bool hasLastError = Logger.GetLastError( lastErrorMessage, lastErrorTime );

  TGNString json = "{\"DeviceId\":";
  AppendJsonString( json, GnDeviceIdentity.WireId() );
  json += ",\"Identity\":" + GnDeviceIdentity.ReportJson();
  json += ",\"OnlineSince\":";
  AppendInteger( json, static_cast<long long>( FOnlineSince ) );
  json += ",\"CurrentTime\":";
  AppendInteger( json, static_cast<long long>( std::time( nullptr ) ) );
  json += ",\"TimeSynchronized\":";
  json += timeSynchronized ? "true" : "false";
  json += ",\"TimeSyncError\":";
  if( timeSynchronized )
    json += "null";
  else
    AppendJsonString( json, TTimeSync::Error( FCachedTimeSyncState ) );
  json += ",\"Healthy\":";
  json += errors.empty() ? "true" : "false";
  json += ",\"LastError\":";
  if( hasLastError )
   {
    json += "{\"Time\":";
    AppendInteger( json, static_cast<long long>( lastErrorTime ) );
    json += ",\"Message\":";
    AppendJsonString( json, lastErrorMessage );
    json += '}';
   }
  else
    json += "null";

  json += ",\"Errors\":[";
  for( unsigned errorNr = 0;  errorNr < errors.size();  ++errorNr )
   {
    if( errorNr != 0 )
      json += ',';
    AppendJsonString( json, errors[ errorNr ] );
   }
  json += "],\"Channels\":[";

  for( unsigned channelNr = 0;  channelNr < FChannels.size();  ++channelNr )
   {
    if( channelNr != 0 )
      json += ',';

    const TChannelConfig & channel = FChannels[ channelNr ];
    const TChannelApiStatus & apiStatus = channelStati[ channelNr ];
    TSchedulerChannelStatus schedulerStatus;
    if( channelNr < schedulerStati.size() )
      schedulerStatus = schedulerStati[ channelNr ];

    json += "{\"Id\":";
    AppendInteger( json, channelNr );
    json += ",\"Name\":";
    AppendJsonString( json, channel.Id );
    int HardwareChannel = 0;
    if( std::sscanf( channel.PhysPos.c_str(), "Main %d", &HardwareChannel ) != 1 ) HardwareChannel = 0;
    json += ",\"HardwareChannel\":"; AppendInteger( json, HardwareChannel );
    json += ",\"CapabilityId\":"; AppendInteger( json, channel.HasOutput() ? 1 : 0 );
    json += ",\"HasInput\":";
    json += channel.InputPin ? "true" : "false";
    json += ",\"HasOutput\":";
    json += channel.HasOutput() ? "true" : "false";
    json += ",\"InputState\":";
    if( channel.InputPin && apiStatus.InputValid() )
      json += apiStatus.InputState() ? "true" : "false";
    else
      json += "null";

    json += ",\"State\":";
    if( channel.HasOutput() )
      json += schedulerStatus.State() ? "true" : "false";
    else
      json += "null";

    json += ",\"AutomaticState\":";
    if( channel.HasOutput() )
      json += schedulerStatus.AutomaticState() ? "true" : "false";
    else
      json += "null";

    json += ",\"OverrideActive\":";
    json += schedulerStatus.OverrideActive() ? "true" : "false";
    json += ",\"OverrideState\":";
    if( schedulerStatus.OverrideActive() )
      json += schedulerStatus.OverrideState() ? "true" : "false";
    else
      json += "null";
    json += ",\"OverrideUntil\":";
    if( schedulerStatus.OverrideActive() && schedulerStatus.OverrideUntilKnown() )
      AppendInteger( json, static_cast<long long>( schedulerStatus.OverrideUntil() ) );
    else
      json += "null";

    json += ",\"NextChange\":";
    if( channel.HasOutput() && schedulerStatus.NextChangeValid() )
     {
      json += "{\"Time\":";
      AppendInteger( json, static_cast<long long>( schedulerStatus.NextChangeTime() ) );
      json += ",\"State\":";
      json += schedulerStatus.NextChangeState() ? "true" : "false";
      json += '}';
     }
    else
      json += "null";

    json += ",\"Error\":";
    if( apiStatus.Error().empty() )
      json += "null";
    else
      AppendJsonString( json, apiStatus.Error() );
    json += '}';
   }

  json += "]}";
  if( signature != nullptr )
    *signature = StatusSignature( json );
  return json;
 }
