// FeederStatusApi.cpp, Version: 2.50
#include <GnBasics.h>
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
  const TGNString DataVersion = ReadDataVersion();
  const TGNString DriveCapabilities = FeederDrive.CapabilitiesJson();
  TGNString Result;
  const auto Build = [&]( auto & Json )
   {
    Json += F( "{\"ApiVersion\":" );
    AppendInteger( Json, ApiVersion );
    Json += F( ",\"DeviceType\":" );
    AppendJsonString( Json, F( ESP_UPDATE_DEVICE_TYPE ) );
    Json += F( ",\"DeviceId\":" );
    AppendJsonString( Json, GnDeviceIdentity.WireId().c_str() );
  Json += F( ",\"Identity\":" );
  Json += GnDeviceIdentity.ReportJson();
    Json += F( ",\"Name\":" );
    AppendJsonString( Json, GnDeviceIdentity.Function().c_str() );
    Json += F( ",\"FirmwareVersion\":" );
    AppendJsonString( Json, ProjectVersion::FirmwareRevision );
    if( !DataVersion.empty() )
     {
      Json += F( ",\"DataVersion\":" );
      AppendJsonString( Json, DataVersion );
     }
    Json += F( ",\"Capabilities\":[\"Status\",\"NextChange\",\"ErrorStatus\",\"Measurements\",\"WebSocket\",\"PollingFallback\",\"ConfigPage\"" );
    Json += DriveCapabilities;
    Json += F( "],\"ConfigUrl\":\"/Index.html\",\"Events\":{\"Protocol\":\"ws\",\"Port\":" );
    AppendInteger( Json, WebSocketPort );
    Json += F( ",\"Path\":\"/api/v1/events\",\"PollingFallbackSeconds\":" );
    AppendInteger( Json, PollingFallbackSeconds );
    Json += F( "}}" );
   };

  if( !GnBuildStringExact( Result, Build ) )
    return TGNString();
  return Result;
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
  if( TimeSynchronized )
    Json += F( "null" );
   else
    AppendJsonString( Json, F( "System time is not synchronized" ) );
  Json += F( ",\"Healthy\":" );
  Json += ErrorCount == 0 ? F( "true" ) : F( "false" );
  Json += F( ",\"LastError\":null,\"Errors\":[" );
  for( unsigned ErrorNr = 0;  ErrorNr < ErrorCount;  ++ErrorNr )
   {
    if( ErrorNr != 0 )
      Json += ',';
    AppendJsonString( Json, Errors[ErrorNr] );
   }
  Json += F( "],\"Measurements\":[" );
  if( FeederTemperature.Enabled() )
   {
    Json += F( "{\"Id\":\"Temperature\",\"Name\":\"Temperatur\",\"Value\":" );
    if( FeederTemperature.Valid() )
      AppendNumber( Json, FeederTemperature.Value(), 1 );
     else
      Json += F( "null" );
    Json += F( ",\"Unit\":" );
    AppendJsonString( Json, FeederTemperature.Unit() );
    Json += F( ",\"Decimals\":1,\"Valid\":" );
    Json += FeederTemperature.Valid() ? F( "true" ) : F( "false" );
    Json += F( ",\"UpdatedAt\":" );
    if( FeederTemperature.UpdatedAt() > 0 )
      AppendInteger( Json, static_cast<long long>( FeederTemperature.UpdatedAt() ) );
     else
      Json += F( "null" );
    Json += F( ",\"Error\":" );
    if( FeederTemperature.Error()[ 0 ] == '\0' )
      Json += F( "null" );
     else
      AppendJsonString( Json, FeederTemperature.Error() );
    Json += '}';
   }
  Json += F( "],\"Channels\":[{\"Id\":0,\"Name\":\"Fütterung\",\"HasInput\":" );
  Json += FeederDrive.HasInput() ? F( "true" ) : F( "false" );
  Json += F( ",\"HasOutput\":true,\"InputState\":" );
  if( FeederDrive.HasInput() )
    Json += FeederDrive.InputState() ? F( "true" ) : F( "false" );
   else
    Json += F( "null" );
  Json += F( ",\"State\":" );
  Json += Feeding ? F( "true" ) : F( "false" );
  Json += F( ",\"StateLabels\":{\"True\":\"LÄUFT\",\"False\":\"BEREIT\"},\"AutomaticState\":null,\"OverrideActive\":false,\"OverrideState\":null,\"OverrideUntil\":null,\"NextChange\":" );
  if( HasNextFeed )
   {
    Json += F( "{\"Time\":" );
    AppendInteger( Json, static_cast<long long>( NextFeedTime ) );
    Json += F( ",\"State\":true,\"Source\":\"FeedTime\",\"Portions\":" );
    AppendInteger( Json, NextFeedPortions );
    Json += '}';
   }
   else
   {
    Json += F( "null" );
   }
  Json += F( ",\"Error\":" );
  if( FeederDrive.Error()[ 0 ] == '\0' )
    Json += F( "null" );
   else
    AppendJsonString( Json, FeederDrive.Error() );
    Json += F( "}]}" );
   };

  if( !GnBuildStringExact( Json, Build ) )
    return TGNString();
  if( signature != nullptr )
    *signature = StatusSignature( Json );
  return Json;
 }
