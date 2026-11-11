// TmrSwStatusApi.cpp, Version: 3.19
#define Do_THttpButtonAction
#define Do_THttpDevice
#define Do_TJsonPowerDiagnostics
#define Do_TUdpChannel
#define Do_TUdpNextChange
#define Do_TUdpPowerMeter
#define Do_TUdpStatusData
#define Do_TWsEventEndpoint
#define Do_TWsStatusEvent
#include <GnBasics.h>
#include <GnJsonSchema.h>
#include <DeviceUrl.h>
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
#include <esp_ota_ops.h>
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

TGNString TTmrSwStatusApi::RescueVersion(void) const
 {
  const auto * Rescue = esp_partition_find_first(
    ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "safeboot" );
  esp_app_desc_t Description = {};
  if( !Rescue || esp_ota_get_partition_description( Rescue, &Description ) != ESP_OK )
    return {};
  return TGNString( Description.version, strnlen( Description.version, sizeof( Description.version ) ) );
 }

TGNString TTmrSwStatusApi::DeviceJson(void) const
 {
  THttpDevice Device;
  Device.DevId = GnDeviceIdentity.WireId();
  Device.Identity.Present = true;
  GnDeviceIdentity.ReportIdentity( Device.Identity.Value );
  Device.ApiVer = ApiVersion;
  Device.DevType = TMR_SW_DEVICE_TYPE;
  Device.Name = GnDeviceIdentity.Function();
  Device.FirmwareVer = ProjectVersion::FirmwareRevision;
  const TGNString DataVersion = ReadDataVersion();
  if( !DataVersion.empty() )Device.DataVer = DataVersion;
  Device.CfgUrl = DeviceUrl("http",80,"/Index.html");
  TWsEventEndpoint Events;
  Events.Url=DeviceUrl("ws",WebSocketPort,"/api/v1/events");
  Events.PollingFallbackSec = PollingFallbackSeconds;
  Device.EndpointAssignments = uint32_t( 1 );
  Device.Caps = { "Status", "OverrideStatus", "NextChange", "ErrorStatus", "WebSocket", "PollingFallback", "ConfigPage", "ButtonActions" };
  THttpButtonAction Button;
  Button.Url = DeviceUrl("http",80,"/api/v1/button/{ChannelId}/{Press}");
  Button.Presses = { "Short" };
  Device.ButtonPress = Button;
  if( ProjectVersion::RescueTargetRevision[0] != '\0' )Device.RescueTargetVer = ProjectVersion::RescueTargetRevision;
#if TMR_SW_HAS_POWER_METER
  Device.Caps.push_back( "PowerMeter" );
#endif
  Device.Events=Events;
  return GnWriteJson( Device );
 }

TGNString TTmrSwStatusApi::StatusJson(void) const
 {
  std::unique_ptr<TUdpStatusData> Status( new(std::nothrow) TUdpStatusData{} );
  if( !Status )return {};
  FillStatus( *Status );
  return GnWriteJson( *Status );
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

TGNString TTmrSwStatusApi::StatusJson( uint64_t & Signature ) const
 {
  auto Json = StatusJson();
  if( !Json.empty() )Signature = StatusSignature( Json );
  return Json;
 }

TGNString TTmrSwStatusApi::StatusEventJson( uint64_t Sequence ) const
 {
  std::unique_ptr<TWsStatusEvent> Event( new(std::nothrow) TWsStatusEvent{} );
  if( !Event )return {};
  Event->Seq = Sequence;
  Event->Status.Present = true;
  FillStatus( Event->Status.Value );
  Event->Timestamp = Event->Status.Value.CurrentTime.Value;
  return GnWriteJson( *Event );
 }

void TTmrSwStatusApi::FillStatus( TUdpStatusData & Status ) const
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

  Status.DevId = GnDeviceIdentity.WireId();
  Status.Identity.Present = true;
  GnDeviceIdentity.ReportIdentity( Status.Identity.Value );
  Status.Identity.Value.BootCnt = nullptr;
  Status.BootCnt = GnDeviceIdentity.BootCounter();
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
  Channel.Name = TmrSwChannels.Channel().Id.empty() ? GnDeviceIdentity.Function() : TmrSwChannels.Channel().Id;
  Channel.HwCh = uint32_t( 0 );
  Channel.HasIn = TmrSwHw.HasInput();
  Channel.HasOut = TmrSwHw.HasOutput();
  Channel.InState = TmrSwHw.ButtonPressed();
  if( RelayStateKnown )Channel.State = RelayState;
  else Channel.State = nullptr;
  if( TimeSynchronized )Channel.AutomaticState = AutomaticState;
  else Channel.AutomaticState = nullptr;
  Channel.OvrActive = OverrideActive;
  if( OverrideActive )Channel.OvrState = CalibrationOverrideActive ? SwitchTimes.CalibrationOverrideState() : SwitchTimes.OverrideState();
  else Channel.OvrState = nullptr;
  Channel.CalOvrActive = CalibrationOverrideActive;
  if( OverrideActive && !CalibrationOverrideActive && HasScheduleStatus )Channel.OvrUntil = uint64_t( NextChangeTime );
  else Channel.OvrUntil = nullptr;
  if( HasScheduleStatus )
   {
    TUdpNextChange Next;
    Next.Time = uint64_t( NextChangeTime );
    Next.State = NextChangeState;
    Next.Src = CalibrationOverrideActive ? TNextChangeSource::CalibrationOverride : OverrideActive ? TNextChangeSource::OverrideEnd : TNextChangeSource::SwitchTime;
    Channel.NextChange = Next;
   }
  else Channel.NextChange = nullptr;
  Channel.Err = nullptr;
#if TMR_SW_HAS_POWER_METER
  TUdpPowerMeter Power;
  Power.Voltage = double( TmrSwHw.Voltage() );
  Power.Current = double( TmrSwHw.Current() );
  Power.ActivePower = double( TmrSwHw.ActivePower() );
  Power.Valid = TmrSwHw.PowerMeterValid();
  TJsonPowerDiagnostics Diagnostics;
  Diagnostics.CfEdges = TmrSwHw.PowerMeterCfEdges();
  Diagnostics.Cf1Edges = TmrSwHw.PowerMeterCf1Edges();
  Diagnostics.VoltageIntervals = TmrSwHw.PowerMeterVoltageIntervals();
  Diagnostics.CurrentIntervals = TmrSwHw.PowerMeterCurrentIntervals();
  Diagnostics.SelectorLevel = TmrSwHw.PowerMeterSelectorLevel();
  Diagnostics.LoadEnabled = TmrSwHw.PowerMeterLoadEnabled();
  Power.Diag = Diagnostics;
  Status.PowerMeter = Power;
#endif
  Status.HeatControl.Present = true;
  Status.HeatControl.IsNull = !SwitchTimes.HeatControl( Status.HeatControl.Value );
#if GN_DEBUG_HEAT_PI
  Status.HeatDiag.Present = true;
  Status.HeatDiag.IsNull = !SwitchTimes.HeatDiagnostics( Status.HeatDiag.Value );
#endif
#if GN_DEBUG_CHARGE_LEARNING
  Status.ChargeLearn.Present = true;
  Status.ChargeLearn.IsNull = !SwitchTimes.ChargeLearning( Status.ChargeLearn.Value );
#endif
  Status.Chs = std::vector<TUdpChannel>{ Channel };
 }
