// TmrSwStatusApi.cpp, Version: 3.09
#include <GnBasics.h>
#include <GnDeviceIdentity.h>
#include "TmrSwStatusApi.h"
#include "TmrSwProject.h"

#include "SwitchTimes.h"
#include "TmrSwChannels.h"
#include "TmrSwConfig.h"
#include "TmrSwHw.h"
#include "TmrSwWifi.h"
#include "Version.inc.h"
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

TTmrSwStatusApi::TTmrSwStatusApi(void)
 {
 }

template <typename TString>
void TTmrSwStatusApi::AppendJsonChar( TString & Json, char Char )
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
void TTmrSwStatusApi::AppendJsonString( TString & Json, char const * Value )
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
void TTmrSwStatusApi::AppendJsonString( TString & Json, TGNString const & Value )
 {
  Json += '"';
  for( char const Char : Value )
    AppendJsonChar( Json, Char );
  Json += '"';
 }

#if defined( GNPlatformESP )
template <typename TString>
void TTmrSwStatusApi::AppendJsonString( TString & Json, __FlashStringHelper const * Value )
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
void TTmrSwStatusApi::AppendInteger( TString & Json, long long Value )
 {
  char Text[32];
  std::snprintf( Text, sizeof( Text ), "%lld", Value );
  Json += Text;
 }

template <typename TString>
void TTmrSwStatusApi::AppendFloat( TString & Json, float Value, unsigned Digits )
 {
  char Format[8];
  char Text[32];
  std::snprintf( Format, sizeof( Format ), "%%.%uf", Digits );
  std::snprintf( Text, sizeof( Text ), Format, static_cast<double>( Value ) );
  Json += Text;
 }

TGNString TTmrSwStatusApi::DeviceJson(void) const
 {
  const TGNString DataVersion = ReadDataVersion();
  TGNString Json;
  const auto Build = [&]( auto & Json )
   {
    Json += F( "{\"ApiVersion\":" );
  AppendInteger( Json, ApiVersion );
  Json += F( ",\"DeviceType\":" );
  AppendJsonString( Json, TMR_SW_DEVICE_TYPE );
  Json += F( ",\"DeviceId\":" );
  AppendJsonString( Json, GnDeviceIdentity.WireId().c_str() );
  Json += F( ",\"Identity\":" );
  Json += GnDeviceIdentity.ReportJson();
  Json += F( ",\"Name\":" );
  AppendJsonString( Json, GnDeviceIdentity.Function().c_str() );
  Json += F( ",\"FirmwareVersion\":" );
  AppendJsonString( Json, ProjectVersion::FirmwareRevision );
  if( ProjectVersion::RescueTargetRevision[0] != '\0' )
   {
    Json += F( ",\"RescueTargetVersion\":" );
    AppendJsonString( Json, ProjectVersion::RescueTargetRevision );
   }
    if( !DataVersion.empty() )
   {
    Json += F( ",\"DataVersion\":" );
    AppendJsonString( Json, DataVersion );
   }
  Json += F( ",\"EndpointAssignments\":1,\"Capabilities\":[\"Status\",\"OverrideStatus\",\"NextChange\",\"ErrorStatus\",\"WebSocket\",\"PollingFallback\",\"ConfigPage\",\"ButtonActions\"" );
#if TMR_SW_HAS_POWER_METER
  Json += F( ",\"PowerMeter\"" );
#endif
  Json += F( "],\"ConfigUrl\":\"/Index.html\",\"Actions\":{\"ButtonPress\":{\"Path\":\"/api/v1/button/{ChannelId}/{Press}\",\"Presses\":[\"Short\"]}},\"Events\":{\"Protocol\":\"ws\",\"Port\":" );
  AppendInteger( Json, WebSocketPort );
  Json += F( ",\"Path\":\"/api/v1/events\",\"PollingFallbackSeconds\":" );
  AppendInteger( Json, PollingFallbackSeconds );
    Json += F( "}}" );
   };

  if( !GnBuildStringExact( Json, Build ) )
    return TGNString();
  return Json;
 }

TGNString TTmrSwStatusApi::StatusJson(void) const
 {
  return BuildStatusJson( nullptr );
 }

uint64_t TTmrSwStatusApi::StatusSignature( TGNString const & json )
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

TGNString TTmrSwStatusApi::StatusJson( uint64_t & signature ) const
 {
  return BuildStatusJson( &signature );
 }

TGNString TTmrSwStatusApi::BuildStatusJson( uint64_t * signature ) const
 {
  const std::time_t CurrentTime = std::time( nullptr );
  const bool TimeSynchronized = TmrSwWifi.TimeIsValid();
  const bool StationRequired = ( TmrSwConfig.WifiMode & WIFI_STA ) != 0;
  const bool StationConnected = !StationRequired || WiFi.status() == WL_CONNECTED;

  TGNString Errors[6];
  unsigned ErrorCount = 0;
  if ( !TmrSwHw.HardwareReady() )
    Errors[ErrorCount++] = F( "Hardware is not ready" );
  if ( !FFileSystemMounted )
    Errors[ErrorCount++] = F( "LittleFS is not mounted" );
  if ( !StationConnected )
    Errors[ErrorCount++] = F( "WiFi station is not connected" );
  if ( !TimeSynchronized )
    Errors[ErrorCount++] = F( "System time is not synchronized" );
  if ( !FWebSocketStarted )
    Errors[ErrorCount++] = F( "WebSocket server is not running" );
  if ( !FSetupError.empty() && ErrorCount < 6 )
    Errors[ErrorCount++] = FSetupError;

  bool AutomaticState = false;
  bool NextChangeState = false;
  time_t NextChangeTime = 0;
  const bool HasScheduleStatus = TimeSynchronized &&
    SwitchTimes.GetStatus( TmrSwWifi.GetTm(), CurrentTime, AutomaticState,
                           NextChangeTime, NextChangeState );
  if( TimeSynchronized )
    AutomaticState = SwitchTimes.AutomaticState();
  const bool RelayStateKnown = TmrSwHw.RelayStatusKnown();
  const bool RelayState = TmrSwHw.RelayStatus();
  const bool CalibrationOverrideActive = SwitchTimes.CalibrationOverrideActive();
  const bool OverrideActive = CalibrationOverrideActive ||
    ( TimeSynchronized && SwitchTimes.OverrideActive() );

  if ( TimeSynchronized && FOnlineSince == 0 )
    FOnlineSince = CurrentTime - static_cast<time_t>( FStartedTimer.Elapsed() / 1000 );

  TGNString Json;
  const auto Build = [&]( auto & Json )
   {
    Json += F( "{\"DeviceId\":" );
  AppendJsonString( Json, GnDeviceIdentity.WireId().c_str() );
  Json += F( ",\"Identity\":" );
  Json += GnDeviceIdentity.ReportJson();
  Json += F( ",\"OnlineSince\":" );
  AppendInteger( Json, static_cast<long long>( FOnlineSince ) );
  Json += F( ",\"CurrentTime\":" );
  AppendInteger( Json, static_cast<long long>( CurrentTime ) );
  Json += F( ",\"TimeSynchronized\":" );
  Json += TimeSynchronized ? F( "true" ) : F( "false" );
  Json += F( ",\"TimeSyncError\":" );
  if ( TimeSynchronized )
    Json += F( "null" );
   else
    AppendJsonString( Json, F( "System time is not synchronized" ) );
  Json += F( ",\"Healthy\":" );
  Json += ErrorCount == 0 ? F( "true" ) : F( "false" );
  Json += F( ",\"LastError\":null,\"Errors\":[" );
  for ( unsigned ErrorNr = 0; ErrorNr < ErrorCount; ++ErrorNr )
   {
    if ( ErrorNr != 0 )
      Json += ',';
    AppendJsonString( Json, Errors[ErrorNr] );
   }
  Json += F( "],\"Channels\":[{\"Id\":0,\"Name\":" );
  AppendJsonString( Json, TmrSwChannels.Channel().Id.empty()
                    ? GnDeviceIdentity.Function().c_str() : TmrSwChannels.Channel().Id.c_str() );
  Json += F( ",\"HardwareChannel\":0,\"CapabilityId\":1,\"HasInput\":" );
  Json += TmrSwHw.HasInput() ? F( "true" ) : F( "false" );
  Json += F( ",\"HasOutput\":" );
  Json += TmrSwHw.HasOutput() ? F( "true" ) : F( "false" );
  Json += F( ",\"InputState\":" );
  Json += TmrSwHw.ButtonPressed() ? F( "true" ) : F( "false" );
  Json += F( ",\"State\":" );
  if ( RelayStateKnown )
    Json += RelayState ? F( "true" ) : F( "false" );
   else
    Json += F( "null" );
  Json += F( ",\"AutomaticState\":" );
  if ( TimeSynchronized )
    Json += AutomaticState ? F( "true" ) : F( "false" );
   else
    Json += F( "null" );
  Json += F( ",\"OverrideActive\":" );
  Json += OverrideActive ? F( "true" ) : F( "false" );
  Json += F( ",\"OverrideState\":" );
  if ( OverrideActive )
    Json += ( CalibrationOverrideActive ? SwitchTimes.CalibrationOverrideState() :
              SwitchTimes.OverrideState() ) ? F( "true" ) : F( "false" );
   else
    Json += F( "null" );
  Json += F( ",\"CalibrationOverrideActive\":" );
  Json += CalibrationOverrideActive ? F( "true" ) : F( "false" );
  Json += F( ",\"OverrideUntil\":" );
  if ( OverrideActive && !CalibrationOverrideActive && HasScheduleStatus )
    AppendInteger( Json, static_cast<long long>( NextChangeTime ) );
   else
    Json += F( "null" );
  Json += F( ",\"NextChange\":" );
  if ( HasScheduleStatus )
   {
    Json += F( "{\"Time\":" );
    AppendInteger( Json, static_cast<long long>( NextChangeTime ) );
    Json += F( ",\"State\":" );
    Json += NextChangeState ? F( "true" ) : F( "false" );
    Json += F( ",\"Source\":" );
    AppendJsonString( Json, CalibrationOverrideActive ? F( "CalibrationOverride" ) :
                      OverrideActive ? F( "OverrideEnd" ) : F( "SwitchTime" ) );
    Json += '}';
   }
   else
   {
    Json += F( "null" );
   }
  Json += F( ",\"Error\":null}]" );
#if TMR_SW_HAS_POWER_METER
  Json += F( ",\"PowerMeter\":{\"Voltage\":" );
  AppendFloat( Json, TmrSwHw.Voltage(), 1 );
  Json += F( ",\"Current\":" );
  AppendFloat( Json, TmrSwHw.Current(), 3 );
  Json += F( ",\"ActivePower\":" );
  AppendFloat( Json, TmrSwHw.ActivePower(), 1 );
  Json += F( ",\"Valid\":" );
  Json += TmrSwHw.PowerMeterValid() ? F( "true" ) : F( "false" );
  Json += F( ",\"Diagnostics\":{\"CfEdges\":" );
  AppendInteger( Json, TmrSwHw.PowerMeterCfEdges() );
  Json += F( ",\"Cf1Edges\":" );
  AppendInteger( Json, TmrSwHw.PowerMeterCf1Edges() );
  Json += F( ",\"VoltageIntervals\":" );
  AppendInteger( Json, TmrSwHw.PowerMeterVoltageIntervals() );
  Json += F( ",\"CurrentIntervals\":" );
  AppendInteger( Json, TmrSwHw.PowerMeterCurrentIntervals() );
  Json += F( ",\"SelectorLevel\":" );
  AppendInteger( Json, TmrSwHw.PowerMeterSelectorLevel() );
  Json += F( ",\"LoadEnabled\":" );
  Json += TmrSwHw.PowerMeterLoadEnabled() ? F( "true" ) : F( "false" );
  Json += F( "}}" );
#endif
  Json += '}';

   };

  if( !GnBuildStringExact( Json, Build ) )
    return TGNString();
  if( signature != nullptr )
    *signature = StatusSignature( Json );
  return Json;
 }
