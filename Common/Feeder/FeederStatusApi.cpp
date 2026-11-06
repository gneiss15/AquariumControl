// FeederStatusApi.cpp, Version: 2.55
#define Do_THttpDevice
#define Do_TUdpChannel
#define Do_TUdpMeasurement
#define Do_TUdpNextChange
#define Do_TUdpStateLabels
#define Do_TUdpStatusData
#define Do_TWsEventEndpoint
#include <GnBasics.h>
#include <GnJsonSchema.h>
#include <DeviceUrl.h>
#include <GnDeviceIdentity.h>
#include "FeederStatusApi.h"

#include "FeederConfig.h"
#include "FeederDrive.h"
#include "FeederTemperature.h"
#include "FeederWifi.h"
#include "FeedTimes.h"
#include "Version.inc.h"
#include "DeviceType.inc.h"
#include <LittleFS.h>
#include <cstdio>
#include <ctime>

namespace
 {
constexpr unsigned ApiVersion = 1;
constexpr unsigned WebSocketPort = 81;
constexpr unsigned PollingFallbackSeconds = 5;

TGNString ReadDataVersion(void)
 {
  File FileHandle = LittleFS.open( "/Versions.txt", "r" );
  if( !FileHandle )
    return {};

  while( FileHandle.available() )
   {
    String Line = FileHandle.readStringUntil( '\n' );
    Line.trim();
    if( Line.startsWith( "LittleFS=" ) )
     {
      TGNString Version = Line.substring( 9 ).c_str();
      FileHandle.close();
      return Version;
     }
   }
  FileHandle.close();
  return {};
 }
 }

TFeederStatusApi::TFeederStatusApi(void)
 {
 }

template <typename TString>
void TFeederStatusApi::AppendJsonChar( TString & Json, char Char )
 {
  switch( Char )
   {
    case '\"':
      Json += F( "\\\"" );
      break;
    case '\\':
      Json += F( "\\\\" );
      break;
    case '\b':
      Json += F( "\\b" );
      break;
    case '\f':
      Json += F( "\\f" );
      break;
    case '\n':
      Json += F( "\\n" );
      break;
    case '\r':
      Json += F( "\\r" );
      break;
    case '\t':
      Json += F( "\\t" );
      break;
    default:
      if( static_cast<unsigned char>( Char ) < 0x20 )
       {
        char Escape[7];
#if defined( GNPlatformESP )
        snprintf_P( Escape, sizeof( Escape ), PSTR( "\\u%04x" ), static_cast<unsigned char>( Char ) );
#else
        std::snprintf( Escape, sizeof( Escape ), "\\u%04x", static_cast<unsigned char>( Char ) );
#endif
        Json += Escape;
       }
       else
       {
        Json += Char;
       }
      break;
   }
 }

template <typename TString>
void TFeederStatusApi::AppendJsonString( TString & Json, char const * Value )
 {
  Json += '"';
  if( Value != nullptr )
   {
    while( *Value != '\0' )
      AppendJsonChar( Json, *Value++ );
   }
  Json += '"';
 }

template <typename TString>
void TFeederStatusApi::AppendJsonString( TString & Json, TGNString const & Value )
 {
  Json += '"';
  for( char const Char : Value )
    AppendJsonChar( Json, Char );
  Json += '"';
 }

#if defined( GNPlatformESP )
template <typename TString>
void TFeederStatusApi::AppendJsonString( TString & Json, __FlashStringHelper const * Value )
 {
  Json += '"';
  PGM_P Position = reinterpret_cast<PGM_P>( Value );
  while( true )
   {
    const char Char = static_cast<char>( pgm_read_byte( Position++ ) );
    if( Char == '\0' )
      break;
    AppendJsonChar( Json, Char );
   }
  Json += '"';
 }
#endif

template <typename TString>
void TFeederStatusApi::AppendInteger( TString & Json, long long Value )
 {
  char Text[32];
  std::snprintf( Text, sizeof( Text ), "%lld", Value );
  Json += Text;
 }

template <typename TString>
void TFeederStatusApi::AppendNumber( TString & Json, float Value, unsigned Decimals )
 {
  uint32_t Scale = 1;
  for( unsigned Decimal = 0;  Decimal < Decimals;  ++Decimal )
    Scale *= 10;

  const bool Negative = Value < 0.0f;
  const double AbsoluteValue = Negative ? -static_cast<double>( Value ) : static_cast<double>( Value );
  const double ScaledFloat = AbsoluteValue * static_cast<double>( Scale );
  uint32_t ScaledValue = static_cast<uint32_t>( ScaledFloat );
  const double FractionPart = ScaledFloat - static_cast<double>( ScaledValue );
  if( FractionPart > 0.5 || ( FractionPart == 0.5 && ( ScaledValue & 1 ) != 0 ) )
    ++ScaledValue;

  if( Negative )
    Json += '-';
  AppendInteger( Json, ScaledValue / Scale );
  if( Decimals == 0 )
    return;

  Json += '.';
  char FractionText[24];
  std::snprintf( FractionText, sizeof( FractionText ), "%0*lu", static_cast<int>( Decimals ),
                 static_cast<unsigned long>( ScaledValue % Scale ) );
  Json += FractionText;
 }

TGNString TFeederStatusApi::DeviceJson(void) const
 {
  THttpDevice Device;
  Device.DevId = GnDeviceIdentity.WireId();
  if( !GnReadJsonValue( GnDeviceIdentity.ReportJson(), Device.Identity ) )return {};
  Device.ApiVer = ApiVersion;
  Device.DevType = ESP_UPDATE_DEVICE_TYPE;
  Device.Name = GnDeviceIdentity.Function();
  Device.FirmwareVer = ProjectVersion::FirmwareRevision;
  const TGNString DataVersion = ReadDataVersion();
  if( !DataVersion.empty() )Device.DataVer = DataVersion;
  Device.CfgUrl = DeviceUrl("http",80,"/Index.html");
  TWsEventEndpoint Events;
  Events.Url=DeviceUrl("ws",WebSocketPort,"/api/v1/events");
  Events.PollingFallbackSec = PollingFallbackSeconds;
  Device.Caps = { "Status", "NextChange", "ErrorStatus", "Measurements", "WebSocket", "PollingFallback", "ConfigPage" };
  const auto ExtraCapabilities = FeederDrive.Capabilities();
  Device.Caps.Value.insert( Device.Caps.Value.end(), ExtraCapabilities.begin(), ExtraCapabilities.end() );
  Device.Events=Events;
  return GnWriteJson( Device );
 }

TGNString TFeederStatusApi::StatusJson(void) const
 {
  return BuildStatusJson( nullptr );
 }

uint64_t TFeederStatusApi::StatusSignature( TGNString const & json )
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

TGNString TFeederStatusApi::StatusJson( uint64_t & signature ) const
 {
  return BuildStatusJson( &signature );
 }

TGNString TFeederStatusApi::BuildStatusJson( uint64_t * signature ) const
 {
  const std::time_t CurrentTime = std::time( nullptr );
  const bool TimeSynchronized = FeederWifi.TimeIsValid();
  const bool StationRequired = ( FeederConfig.WifiMode & WIFI_STA ) != 0;
  const bool StationConnected = !StationRequired || WiFi.status() == WL_CONNECTED;

  TGNString Errors[7];
  unsigned ErrorCount = 0;
  if( !FFileSystemMounted )
    Errors[ErrorCount++] = F( "LittleFS is not mounted" );
  if( !StationConnected )
    Errors[ErrorCount++] = F( "WiFi station is not connected" );
  if( !TimeSynchronized )
    Errors[ErrorCount++] = F( "System time is not synchronized" );
  if( !FWebSocketStarted )
    Errors[ErrorCount++] = F( "WebSocket server is not running" );
  if( FeederTemperature.Error()[ 0 ] != '\0' && ErrorCount < TableSize( Errors ) )
    Errors[ErrorCount++] = FeederTemperature.Error();
  if( FeederDrive.Error()[ 0 ] != '\0' && ErrorCount < TableSize( Errors ) )
    Errors[ErrorCount++] = FeederDrive.Error();
  if( !FSetupError.empty() && ErrorCount < TableSize( Errors ) )
    Errors[ErrorCount++] = FSetupError;

  std::time_t NextFeedTime = 0;
  uint8_t NextFeedPortions = 0;
  const bool HasNextFeed = TimeSynchronized && FeedTimes.GetNextFeed( NextFeedTime, NextFeedPortions );
  const bool Feeding = FeederDrive.Running();
  if( TimeSynchronized && FOnlineSince == 0 )
    FOnlineSince = CurrentTime - static_cast<time_t>( FStartedTimer.Elapsed() / 1000 );

  std::unique_ptr<TUdpStatusData> Storage( new(std::nothrow) TUdpStatusData{} );
  if( !Storage )return {};
  auto & Status = *Storage;
  Status.DevId = GnDeviceIdentity.WireId();
  if( !GnReadJsonValue( GnDeviceIdentity.ReportJson(), Status.Identity ) )return {};
  Status.OnlineSince = static_cast<uint64_t>( FOnlineSince );
  Status.CurrentTime = static_cast<uint64_t>( CurrentTime );
  Status.TimeSynchronized = TimeSynchronized;
  if( TimeSynchronized )Status.TimeSyncErr = nullptr;
  else Status.TimeSyncErr = "System time is not synchronized";
  Status.Healthy = ErrorCount == 0;
  Status.LastErr = nullptr;
  Status.Errs = std::vector<TGNString>( Errors, Errors + ErrorCount );
  TUdpChannel Channel;
  Channel.Id.Present = true;
  Channel.Id=GnDeviceIdentity.WireId()+"/0";
  Status.Meas = std::vector<TUdpMeasurement>{};
  if( FeederTemperature.Enabled() )
   {
    TUdpMeasurement Measurement;
    Measurement.Kind = "Temperature";
    Measurement.Name = "Temperatur";
    if( FeederTemperature.Valid() )Measurement.Value = double( FeederTemperature.Value() );
    else Measurement.Value = nullptr;
    Measurement.Unit = FeederTemperature.Unit();
    Measurement.Decimals = uint8_t( 1 );
    Measurement.Valid = FeederTemperature.Valid();
    if( FeederTemperature.UpdatedAt() > 0 )Measurement.UpdatedAt = uint64_t( FeederTemperature.UpdatedAt() );
    else Measurement.UpdatedAt = nullptr;
    if( FeederTemperature.Error()[0] == '\0' )Measurement.Err = nullptr;
    else Measurement.Err = FeederTemperature.Error();
    Status.Meas.Value.push_back( Measurement );
   }
  Channel.Name = "Fütterung";
  Channel.HasIn = FeederDrive.HasInput();
  Channel.HasOut = true;
  if( FeederDrive.HasInput() )Channel.InState = FeederDrive.InputState();
  else Channel.InState = nullptr;
  Channel.State = Feeding;
  TUdpStateLabels Labels;
  Labels.True="FeederRunning";
  Labels.False="FeederReady";
  Channel.StateLabels=Labels;
  Channel.AutomaticState = nullptr;
  Channel.OvrActive = false;
  Channel.OvrState = nullptr;
  Channel.OvrUntil = nullptr;
  if( HasNextFeed )
   {
    TUdpNextChange Next;
    Next.Time = uint64_t( NextFeedTime );
    Next.State = true;
    Next.Src = TNextChangeSource::FeedTime;
    Next.Portions = uint32_t( NextFeedPortions );
    Channel.NextChange = Next;
   }
  else Channel.NextChange = nullptr;
  if( FeederDrive.Error()[0] == '\0' )Channel.Err = nullptr;
  else Channel.Err = FeederDrive.Error();
  Status.Chs = std::vector<TUdpChannel>{ Channel };
  const TGNString Json = GnWriteJson( Status );
  if( signature != nullptr )*signature = StatusSignature( Json );
  return Json;
 }
