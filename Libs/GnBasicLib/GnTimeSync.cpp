// GnTimeSync.cpp, Version: 1.11
#include <GnBasics.h>
#include <GnTimeSync.h>
#include <GnDeviceIdentity.h>

#if defined( GNPlatformESP )

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <ctime>
#include <cstdio>
#include <cstring>

#if defined( GNPlatformESP32 )
  #include <HTTPClient.h>
  #include <WiFi.h>
#elif defined( GNPlatformESP8266 )
  #include <ESP8266HTTPClient.h>
  #include <ESP8266WiFi.h>
#endif

namespace
 {
constexpr TGnCoopTimer_t RetryMilliseconds = 30000;
constexpr TGnCoopTimer_t PeriodicMilliseconds = 300000;
constexpr char SyncUrl[] = "http://192.168.99.51:8090/api/times/sync";

TGNString ReadTextFile( char const * FileName )
 {
  File FileHandle = LittleFS.open( FileName, "r" );
  if( !FileHandle )
    return {};
  const size_t FileSize = FileHandle.size();
  TGNString Result;
  if( !GnReserveStringExact( Result, FileSize ) )
    return {};
  while( FileHandle.available() )
    Result += static_cast<char>( FileHandle.read() );
  if( Result.length() != FileSize )
    return {};
  return Result;
 }

bool WriteTextFile( char const * FileName, TGNString const & Text )
 {
  File FileHandle = LittleFS.open( FileName, "w" );
  if( !FileHandle )
    return false;
  const size_t Written = FileHandle.write(
    reinterpret_cast<uint8_t const *>( Text.data() ), Text.size() );
  FileHandle.close();
  return Written == Text.size();
 }
 } // namespace

TGnTimeSync::TGnTimeSync(void)
: FSyncTimer( Closure( this, &TGnTimeSync::SyncTimerCb ), 5000, false )
 {
 }

void TGnTimeSync::Setup( TGnTimeSyncConfig const & Config,
                         String const & Hostname, TApply Apply )
 {
  if( FSetup )
    return;
  FConfig = Config;
  FHostname = Hostname;
  GnDeviceIdentity.Setup( Hostname.c_str(), strcmp( Config.Type, "FeedTimes" ) == 0 ? 3 : 4, "/Identity.json" );
  FApply = Apply;
  FSetup = true;
  LoadMeta();
  FSyncTimer.Start();
 }

void TGnTimeSync::LoadMeta(void)
 {
  const TGNString Text = ReadTextFile( FConfig.MetaFile );
  if( Text.empty() )
   {
    FTimeStamp = "1970-01-01T00:00:00.000Z";
    return;
   }
  JsonDocument Document;
  if( deserializeJson( Document, Text.c_str() ) )
   {
    FTimeStamp = "1970-01-01T00:00:00.000Z";
    return;
   }
  const char * TimeStamp = Document[ "TimeStamp" ] | "";
  FTimeStamp = TimeStamp;
  FSynced = Document[ "Synced" ] | false;
 }

bool TGnTimeSync::SaveMeta(void) const
 {
  JsonDocument Document;
  Document[ "TimeStamp" ] = FTimeStamp;
  Document[ "Synced" ] = FSynced;
  const size_t JsonSize = measureJson( Document );
  TGNString Text;
  if( !GnReserveStringExact( Text, JsonSize + 1 ) )
    return false;
  serializeJson( Document, Text );
  if( Text.length() != JsonSize )
    return false;
  Text += '\n';
  return Text.length() == JsonSize + 1 && WriteTextFile( FConfig.MetaFile, Text );
 }

TGNString TGnTimeSync::NewTimeStamp(void)
 {
  const time_t Now = time( nullptr );
  struct tm Utc{};
  gmtime_r( &Now, &Utc );
  char Buffer[80];
  snprintf( Buffer, sizeof( Buffer ), "%04d-%02d-%02dT%02d:%02d:%02d.%03luZ",
                 Utc.tm_year + 1900, Utc.tm_mon + 1, Utc.tm_mday,
                 Utc.tm_hour, Utc.tm_min, Utc.tm_sec,
                 static_cast<unsigned long>( millis() % 1000 ) );
  return Buffer;
 }

void TGnTimeSync::LocalChanged(void)
 {
  FTimeStamp = NewTimeStamp();
  FSynced = false;
  SaveMeta();
  ScheduleSync( 1 );
 }

void TGnTimeSync::RemoteApplied( TGNString const & TimeStamp )
 {
  FTimeStamp = TimeStamp;
  FSynced = true;
  SaveMeta();
  ScheduleSync( PeriodicMilliseconds );
 }

TGNString TGnTimeSync::BuildPayload(void) const
 {
  const TGNString Content = ReadTextFile( FConfig.DataFile );
  JsonDocument Document;
  Document[ "Device" ] = GnDeviceIdentity.DeviceId();
  Document[ "Identity" ] = serialized( GnDeviceIdentity.ReportJson() );
  Document[ "Type" ] = FConfig.Type;
  Document[ "Format" ] = FConfig.Format;
  Document[ "TimeStamp" ] = FTimeStamp;
  JsonDocument ContentDocument;
  if( deserializeJson( ContentDocument, Content.c_str() ) )
    return {};
  if( FConfig.ChannelFile != nullptr )
   {
    const TGNString ChannelText = ReadTextFile( FConfig.ChannelFile );
    JsonDocument ChannelDocument;
    if( ChannelText.empty() || deserializeJson( ChannelDocument, ChannelText.c_str() ) )
      return {};
    JsonObject ContentObject = Document[ "Content" ].to<JsonObject>();
    if( strcmp( FConfig.Format, "TmrSw" ) == 0 )
     {
      ContentObject[ "SwitchTimes" ].set( ContentDocument.as<JsonVariantConst>() );
      if( FConfig.IntervalFile != nullptr )
       {
        if( LittleFS.exists( FConfig.IntervalFile ) )
         {
          const TGNString IntervalText = ReadTextFile( FConfig.IntervalFile );
          JsonDocument IntervalDocument;
          if( IntervalText.empty() || deserializeJson( IntervalDocument, IntervalText.c_str() ) ||
              !IntervalDocument.is<JsonArrayConst>() )
            return {};
          ContentObject[ "Intervals" ].set( IntervalDocument.as<JsonVariantConst>() );
         }
       }
     }
    else
     {
      ContentObject[ "SwitchTimes" ].set( ContentDocument[ "SwitchTimes" ].as<JsonVariantConst>() );
      ContentObject[ "Intervals" ].set( ContentDocument[ "Intervals" ].as<JsonVariantConst>() );
     }
    ContentObject[ "Channels" ].set( ChannelDocument.as<JsonVariantConst>() );
   }
  else
    Document[ "Content" ].set( ContentDocument.as<JsonVariantConst>() );
  const size_t JsonSize = measureJson( Document );
  TGNString Result;
  if( !GnReserveStringExact( Result, JsonSize ) )
    return {};
  serializeJson( Document, Result );
  if( Result.length() != JsonSize )
    return {};
  return Result;
 }

bool TGnTimeSync::HttpPost( TGNString const & Url, TGNString const & Body,
                            int & StatusCode, TGNString & Response ) const
 {
  if( WiFi.status() != WL_CONNECTED )
    return false;
  WiFiClient Client;
  HTTPClient Http;
  if( !Http.begin( Client, Url.c_str() ) )
    return false;
  Http.setTimeout( FConfig.HttpTimeoutMilliseconds );
  Http.addHeader( "Content-Type", "application/json" );
  StatusCode = Http.POST( String( Body.c_str() ) );
  if( StatusCode > 0 )
    Response = Http.getString().c_str();
  Http.end();
  return StatusCode > 0;
 }

bool TGnTimeSync::SyncNow(void)
 {
  if( FTimeStamp.empty() )
    return false;
  const TGNString Body = BuildPayload();
  if( Body.empty() )
    return false;
  int StatusCode = 0;
  TGNString Response;
  if( !HttpPost( SyncUrl, Body, StatusCode, Response ) || StatusCode < 200 || StatusCode >= 300 )
    return false;

  JsonDocument Document;
  if( deserializeJson( Document, Response.c_str() ) )
    return false;
  const char * Action = Document[ "Action" ] | "";
  if( strcmp( Action, "UseCentral" ) != 0 && strcmp( Action, "Synced" ) != 0 ) return false;
  if( !GnDeviceIdentity.ApplyEnvelope( Response ) || !ApplyFunctionConfig( Response ) ) return false;
  if( strcmp( Action, "UseCentral" ) == 0 )
   {
    TGNString Content;
    serializeJson( Document[ "Content" ], Content );
    if( FApply( Content.c_str() ) != "1" )
      return false;
    if( !GnDeviceIdentity.CommitAssignment() ) return false;
    RemoteApplied( Document[ "TimeStamp" ] | "" );
    return true;
   }
  if( strcmp( Action, "Synced" ) != 0 )
    return false;
  if( !GnDeviceIdentity.CommitAssignment() ) return false;
  if( !FSynced )
   {
    FSynced = true;
    SaveMeta();
   }
  return true;
 }

int TGnTimeSync::HandleIncoming( TGNString const & Body, TGNString & Response )
 {
  JsonDocument Document;
  if( deserializeJson( Document, Body.c_str() ) )
   {
    Response = "{\"Ok\":false,\"Error\":\"InvalidJson\"}";
    return 400;
   }
  const char * Device = Document[ "Device" ] | "";
  const char * Type = Document[ "Type" ] | "";
  const char * Format = Document[ "Format" ] | "";
  const char * TimeStamp = Document[ "TimeStamp" ] | "";
  if( TGNString( Device ) != GnDeviceIdentity.DeviceId() ||
      strcmp( Type, FConfig.Type ) != 0 || strcmp( Format, FConfig.Format ) != 0 ||
      TimeStamp[0] == '\0' || Document[ "Content" ].isNull() )
   {
    Response = "{\"Ok\":false,\"Error\":\"InvalidPayload\"}";
    return 400;
   }
  if( !FTimeStamp.empty() && FTimeStamp > TimeStamp && !GnDeviceIdentity.AssignmentPending( Body ) )
   {
    Response = "{\"Ok\":false,\"Reason\":\"OlderTimeStamp\"}";
    return 409;
   }
  if( !GnDeviceIdentity.ApplyEnvelope( Body ) || !ApplyFunctionConfig( Body ) )
   {
    Response = "{\"Ok\":false,\"Error\":\"InvalidIdentityOrStorage\"}";
    return 400;
   }
  if( FTimeStamp == TimeStamp && !GnDeviceIdentity.AssignmentPending( Body ) )
   {
    Response = "{\"Ok\":true}";
    return 200;
   }
  TGNString Content;
  serializeJson( Document[ "Content" ], Content );
  const String Result = FApply( Content.c_str() );
  if( Result != "1" )
   {
    Response = TGNString( "{\"Ok\":false,\"Error\":\"" ) + Result.c_str() + "\"}";
    return 400;
   }
  if( !GnDeviceIdentity.CommitAssignment() )
   { Response = "{\"Ok\":false,\"Error\":\"IdentityStorageFailed\"}"; return 500; }
  RemoteApplied( TimeStamp );
  Response = "{\"Ok\":true}";
  return 200;
 }

bool TGnTimeSync::ApplyFunctionConfig( TGNString const & Envelope )
 {
  if( FConfig.ApplyFunctionConfig == nullptr ) return true;
  const String Result = FConfig.ApplyFunctionConfig( Envelope.c_str() );
  if( Result != "1" && Result != "Restart" ) return false;
  if( Result == "Restart" || GnDeviceIdentity.FunctionChanged() )
   {
    FRestartPending = true;
    FRestartAt = millis() + 1500;
   }
  return true;
 }

void TGnTimeSync::ScheduleSync( TGnCoopTimer_t DelayMilliseconds )
 {
  FSyncDue = false;
  FSyncTimer.Interval( DelayMilliseconds );
  FSyncTimer.Start();
 }

void TGnTimeSync::SyncTimerCb( TGnCoopTimerBase *, uint16_t )
 {
  FSyncDue = true;
 }

void TGnTimeSync::Loop(void)
 {
  // Let the HTTP response leave before restarting the updated services.
  if( FRestartPending && static_cast<int32_t>( millis() - FRestartAt ) >= 0 ) ESP.restart();
  if( !FSetup || !FSyncDue )
    return;
  FSyncDue = false;
  const bool Ok = SyncNow();
  ScheduleSync( Ok ? PeriodicMilliseconds : RetryMilliseconds );
 }

#endif
