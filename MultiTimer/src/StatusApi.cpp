// StatusApi.cpp, Version: 2.53
#define Do_THttpButtonAction
#define Do_THttpDevice
#define Do_TUdpChannel
#define Do_TUdpLastError
#define Do_TUdpMeasurement
#define Do_TUdpNextChange
#define Do_TUdpStatusData
#define Do_TWsEventEndpoint
#include <GnBasics.h>
#include <GnJsonSchema.h>
#include <DeviceUrl.h>
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
                        unsigned webSocketPort, std::vector<TGNString> configErrors, TRoomTemperature * RoomTemperature )
: FRoomTemperature( RoomTemperature )
, FHardware( hardware )
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

TGNString TStatusApi::DeviceJson(void) const
 {
  THttpDevice Device;
  Device.DevId = GnDeviceIdentity.WireId();
  if( !GnReadJsonValue( GnDeviceIdentity.ReportJson(), Device.Identity ) )return {};
  Device.ApiVer = FApiVersion;
  Device.DevType = FDeviceType;
  Device.Platform = Platform();
  Device.Name = FName;
  Device.FirmwareVer = MultiTimerVersion;
  Device.CfgUrl = DeviceUrl("http",FWebSocketPort-1,"/");
  Device.StatusClientUrl = DeviceUrl("http",FWebSocketPort-1,"/StatusClient.js");
  TWsEventEndpoint Events;
  Events.Url=DeviceUrl("ws",FWebSocketPort,"/api/v1/events");
  Events.PollingFallbackSec = FPollingFallbackSeconds;
  Device.EndpointAssignments = uint32_t( 1 );
  Device.Caps = { "Status", "OverrideStatus", "NextChange", "ErrorStatus", "WebSocket", "PollingFallback", "ConfigPage", "ButtonActions" };
  THttpButtonAction Button;
  Button.Url = DeviceUrl("http",FWebSocketPort-1,"/api/v1/button/{ChannelId}/{Press}");
  Button.Presses = { "Short", "Long" };
  Device.ButtonPress = Button;
  Device.Events=Events;
  return GnWriteJson( Device );
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

  const bool HasRoomTemperature = FRoomTemperature && FRoomTemperature->Enabled();
  const auto RoomTemperature = HasRoomTemperature ? FRoomTemperature->Sample() : TRoomTemperature::TSample{};
  if( HasRoomTemperature && !RoomTemperature.Valid ) errors.push_back("Room temperature: " + RoomTemperature.Error);

  TGNString lastErrorMessage;
  std::time_t lastErrorTime = 0;
  const bool hasLastError = Logger.GetLastError( lastErrorMessage, lastErrorTime );

  std::unique_ptr<TUdpStatusData> Storage( new(std::nothrow) TUdpStatusData{} );
  if( !Storage )return {};
  auto & Status = *Storage;
  Status.DevId = GnDeviceIdentity.WireId();
  if( !GnReadJsonValue( GnDeviceIdentity.ReportJson(), Status.Identity ) )return {};
  Status.OnlineSince = static_cast<uint64_t>( FOnlineSince );
  Status.CurrentTime = static_cast<uint64_t>( std::time( nullptr ) );
  Status.TimeSynchronized = timeSynchronized;
  if( timeSynchronized )Status.TimeSyncErr = nullptr;
  else Status.TimeSyncErr = TTimeSync::Error( FCachedTimeSyncState );
  Status.Healthy = errors.empty();
  Status.Errs = errors;
  if( hasLastError )
   {
    TUdpLastError Error;
    Error.Time = static_cast<uint64_t>( lastErrorTime );
    Error.Message = lastErrorMessage;
    Status.LastErr = Error;
   }
  else Status.LastErr = nullptr;
  Status.Meas = std::vector<TUdpMeasurement>{};
  if( HasRoomTemperature )
   {
    TUdpMeasurement Measurement;
    Measurement.Kind = "RoomTemperature";
    Measurement.Name = "RoomTemperature";
    if( RoomTemperature.Valid )Measurement.Value = double( RoomTemperature.Celsius );
    else Measurement.Value = nullptr;
    Measurement.Unit = "°C";
    Measurement.Decimals = uint8_t( 1 );
    Measurement.Valid = RoomTemperature.Valid;
    if( RoomTemperature.UpdatedAt > 0 )Measurement.UpdatedAt = uint64_t( RoomTemperature.UpdatedAt );
    else Measurement.UpdatedAt = nullptr;
    if( RoomTemperature.Error.empty() )Measurement.Err = nullptr;
    else Measurement.Err = RoomTemperature.Error;
    Status.Meas.Value.push_back( Measurement );
   }
  Status.Chs = std::vector<TUdpChannel>{};
  for( unsigned ChannelNr = 0; ChannelNr < FChannels.size(); ++ChannelNr )
   {
    const auto & Config = FChannels[ChannelNr];
    const auto & ApiStatus = channelStati[ChannelNr];
    TSchedulerChannelStatus Schedule;
    if( ChannelNr < schedulerStati.size() )Schedule = schedulerStati[ChannelNr];
    TUdpChannel Channel;
    Channel.Id.Present = true;
    Channel.Id=GnDeviceIdentity.WireId()+"/"+std::to_string(Config.PhysicalNumber());
    Channel.Name = Config.Id;
    int HardwareChannel = 0;
    if( std::sscanf( Config.PhysPos.c_str(), "Main %d", &HardwareChannel ) != 1 )HardwareChannel = 0;
    Channel.HwCh = uint32_t( HardwareChannel );
    Channel.HasIn = bool( Config.InputPin );
    Channel.HasOut = Config.HasOutput();
    if( Config.InputPin && ApiStatus.InputValid() )Channel.InState = ApiStatus.InputState();
    else Channel.InState = nullptr;
    if( Config.HasOutput() )
     {
      Channel.State = Schedule.State();
      Channel.AutomaticState = Schedule.AutomaticState();
     }
    else { Channel.State = nullptr;Channel.AutomaticState = nullptr; }
    Channel.OvrActive = Schedule.OverrideActive();
    if( Schedule.OverrideActive() )Channel.OvrState = Schedule.OverrideState();
    else Channel.OvrState = nullptr;
    if( Schedule.OverrideActive() && Schedule.OverrideUntilKnown() )Channel.OvrUntil = uint64_t( Schedule.OverrideUntil() );
    else Channel.OvrUntil = nullptr;
    if( Config.HasOutput() && Schedule.NextChangeValid() )
     {
      TUdpNextChange Next;
      Next.Time = uint64_t( Schedule.NextChangeTime() );
      Next.State = Schedule.NextChangeState();
      Channel.NextChange = Next;
     }
    else Channel.NextChange = nullptr;
    if( ApiStatus.Error().empty() )Channel.Err = nullptr;
    else Channel.Err = ApiStatus.Error();
    Status.Chs.Value.push_back( Channel );
   }
  const TGNString Json = GnWriteJson( Status );
  if( signature != nullptr )*signature = StatusSignature( Json );
  return Json;
 }
