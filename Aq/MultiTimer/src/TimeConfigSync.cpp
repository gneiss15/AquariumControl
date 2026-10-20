// TimeConfigSync.cpp, Version: 1.15
#include "BuildConfig.h"
#include "TimeConfigSync.h"
#if !defined( SYSTEM_ESP )
#include <filesystem>
#endif
#include <GnDeviceIdentity.h>
#include "WebApplication.h"
#include <ctime>
#include <cstdio>
#include <cstring>

#if !defined( SYSTEM_ESP )
#include <arpa/inet.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <json-c/json.h>
#include <netdb.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#else
#include <ArduinoJson.h>
#include <LittleFS.h>
#if defined( ESP32 )
#include <HTTPClient.h>
#include <WiFi.h>
#else
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#endif
#endif

namespace
 {
constexpr TGnCoopTimer_t InitialMs = 5000;
constexpr TGnCoopTimer_t RetryMs = 30000;
constexpr TGnCoopTimer_t PeriodicMs = 300000;

TGNString ReadTextFile( TGNString const & FileName )
 {
#if defined( SYSTEM_ESP )
  File FileHandle = LittleFS.open( FileName.c_str(), "r" );
  if( !FileHandle ) return {};
  const size_t FileSize = FileHandle.size();
  TGNString Result;
  if( !GnReserveStringExact( Result, FileSize ) )
    return {};
  while( FileHandle.available() )
    Result += static_cast<char>( FileHandle.read() );
  return Result.length() == FileSize ? Result : TGNString{};
#else
  std::ifstream File( FileName, std::ios::binary );
  if( !File ) return {};
  return TGNString( std::istreambuf_iterator<char>( File ), std::istreambuf_iterator<char>() );
#endif
 }

bool WriteTextFile( TGNString const & FileName, TGNString const & Text )
 {
#if defined( SYSTEM_ESP )
  File FileHandle = LittleFS.open( FileName.c_str(), "w" );
  if( !FileHandle ) return false;
  const size_t Written = FileHandle.write( reinterpret_cast<uint8_t const *>( Text.data() ), Text.size() );
  FileHandle.close();
  return Written == Text.size();
#else
  std::ofstream File( FileName, std::ios::binary | std::ios::trunc );
  if( !File ) return false;
  File.write( Text.data(), static_cast<std::streamsize>( Text.size() ) );
  return static_cast<bool>( File );
#endif
 }
 } // namespace

TTimeConfigSync::TTimeConfigSync( TGNString DeviceId, TGNString DataDir )
: FDeviceId( std::move( DeviceId ) )
, FDataDir( std::move( DataDir ) )
 {
  LoadMeta();
  FAttemptDelayMs = InitialMs;
  FAttemptTimer.Start();
 }

TGNString TTimeConfigSync::MetaFile(void) const
 {
  return FDataDir == "/" ? "/SwitchTimes.sync.json" : FDataDir + "/SwitchTimes.sync.json";
 }

TGNString TTimeConfigSync::ScheduleFile(void) const
 {
  return FDataDir == "/" ? "/EspSwitchTimes.json" : FDataDir + "/SwitchTimes.json";
 }

void TTimeConfigSync::LoadMeta(void)
 {
  FTimeStamp = "1970-01-01T00:00:00.000Z";
  const TGNString Text = ReadTextFile( MetaFile() );
#if defined( SYSTEM_ESP )
  JsonDocument Document;
  if( deserializeJson( Document, Text.c_str() ) ) return;
  const char * TimeStamp = Document[ "TimeStamp" ] | "";
  FTimeStamp = TimeStamp;
  FSynced = Document[ "Synced" ] | false;
#else
  json_object * Root = json_tokener_parse( Text.c_str() );
  json_object * Value = nullptr;
  if( Root == nullptr ) return;
  if( json_object_object_get_ex( Root, "TimeStamp", &Value ) && json_object_is_type( Value, json_type_string ) )
    FTimeStamp = json_object_get_string( Value );
  if( json_object_object_get_ex( Root, "Synced", &Value ) )
    FSynced = json_object_get_boolean( Value );
  json_object_put( Root );
#endif
 }

bool TTimeConfigSync::SaveMeta(void) const
 {
  TGNString Text = "{\"TimeStamp\":\"" + FTimeStamp + "\",\"Synced\":" + ( FSynced ? "true" : "false" ) + "}\n";
  return WriteTextFile( MetaFile(), Text );
 }

TGNString TTimeConfigSync::NewTimeStamp(void)
 {
#if defined( SYSTEM_ESP )
  const time_t Now = time( nullptr );
  std::tm Utc{};
  gmtime_r( &Now, &Utc );
  char Buffer[32];
  if( std::strftime( Buffer, sizeof( Buffer ), "%Y-%m-%dT%H:%M:%S", &Utc ) == 0 )
    return TGNString();
  const size_t Length = strlen( Buffer );
  std::snprintf( Buffer + Length, sizeof( Buffer ) - Length, ".%03uZ",
                 static_cast<unsigned>( millis() % 1000 ) );
  return Buffer;
#else
  using namespace std::chrono;
  const auto Now = system_clock::now();
  const auto Ms = duration_cast<milliseconds>( Now.time_since_epoch() ) % 1000;
  const time_t Time = system_clock::to_time_t( Now );
  std::tm Utc{};
  gmtime_r( &Time, &Utc );
  char Buffer[32];
  if( std::strftime( Buffer, sizeof( Buffer ), "%Y-%m-%dT%H:%M:%S", &Utc ) == 0 )
    return TGNString();
  const size_t Length = strlen( Buffer );
  std::snprintf( Buffer + Length, sizeof( Buffer ) - Length, ".%03uZ",
                 static_cast<unsigned>( Ms.count() ) );
  return Buffer;
#endif
 }

void TTimeConfigSync::LocalChanged(void)
 {
  FTimeStamp = NewTimeStamp();
  FSynced = false;
  SaveMeta();
  FAttemptDelayMs = 0;
  FAttemptTimer.Start();
 }

void TTimeConfigSync::RemoteApplied( TGNString const & TimeStamp )
 {
  FTimeStamp = TimeStamp;
  FSynced = true;
  SaveMeta();
  FAttemptDelayMs = PeriodicMs;
  FAttemptTimer.Start();
 }

TGNString TTimeConfigSync::BuildPayload(void) const
 {
#if !defined( SYSTEM_ESP )
  // Never publish partially materialized files while an incoming transaction awaits recovery.
  if( std::filesystem::exists( FDataDir + "/Configuration.transaction.json" ) ) return {};
#endif
  const TGNString Schedule = ReadTextFile( ScheduleFile() );
  const TGNString Channels = ReadTextFile( FDataDir == "/" ? "/Channels.json" : FDataDir + "/Channels.json" );
  const size_t End = Schedule.find_last_not_of( " \t\r\n" );
  if( Channels.empty() || End == TGNString::npos || Schedule[ End ] != '}' ) return {};
  TGNString Content = Schedule.substr( 0, End );
  Content += ",\"Channels\":" + Channels + "}";
  return "{\"Device\":\"" + GnDeviceIdentity.DeviceId() + "\",\"Type\":\"SwitchTimes\",\"Format\":\"MyTimer\",\"TimeStamp\":\"" +
         FTimeStamp + "\",\"Identity\":" + GnDeviceIdentity.ReportJson() + ",\"Content\":" + Content + "}";
 }

bool TTimeConfigSync::HttpPost( TGNString const & Body, int & StatusCode, TGNString & Response )
 {
#if defined( SYSTEM_ESP )
  if( WiFi.status() != WL_CONNECTED ) return false;
  WiFiClient Client;
  HTTPClient Http;
  if( !Http.begin( Client, "http://127.0.0.1:8090/api/times/sync" ) ) return false;
  Http.addHeader( "Content-Type", "application/json" );
  StatusCode = Http.POST( String( Body.c_str() ) );
  if( StatusCode > 0 ) Response = Http.getString().c_str();
  Http.end();
  return StatusCode > 0;
#else
  addrinfo Hints{};
  Hints.ai_family = AF_UNSPEC;
  Hints.ai_socktype = SOCK_STREAM;
  addrinfo * Addresses = nullptr;
  if( getaddrinfo( "127.0.0.1", "8090", &Hints, &Addresses ) != 0 ) return false;
  int Socket = -1;
  for( addrinfo * Address = Addresses; Address != nullptr; Address = Address->ai_next )
   {
    Socket = socket( Address->ai_family, Address->ai_socktype, Address->ai_protocol );
    if( Socket < 0 ) continue;
    timeval Timeout{}; Timeout.tv_sec = 3;
    setsockopt( Socket, SOL_SOCKET, SO_RCVTIMEO, &Timeout, sizeof( Timeout ) );
    setsockopt( Socket, SOL_SOCKET, SO_SNDTIMEO, &Timeout, sizeof( Timeout ) );
    if( connect( Socket, Address->ai_addr, Address->ai_addrlen ) == 0 ) break;
    close( Socket ); Socket = -1;
   }
  freeaddrinfo( Addresses );
  if( Socket < 0 ) return false;
  std::ostringstream Header;
  Header << "POST /api/times/sync HTTP/1.1\r\nHost: 127.0.0.1:8090\r\nContent-Type: application/json\r\nContent-Length: "
         << Body.size() << "\r\nConnection: close\r\n\r\n";
  const TGNString Request = Header.str() + Body;
  size_t Offset = 0;
  while( Offset < Request.size() )
   {
    const ssize_t Sent = send( Socket, Request.data() + Offset, Request.size() - Offset, MSG_NOSIGNAL );
    if( Sent <= 0 ) { close( Socket ); return false; }
    Offset += static_cast<size_t>( Sent );
   }
  TGNString Raw; char Buffer[4096];
  while( true ) { const ssize_t Count = recv( Socket, Buffer, sizeof(Buffer), 0 ); if( Count <= 0 ) break; Raw.append( Buffer, static_cast<size_t>( Count ) ); }
  close( Socket );
  const size_t Space = Raw.find( ' ' ); if( Space == TGNString::npos ) return false;
  StatusCode = std::atoi( Raw.substr( Space + 1, 3 ).c_str() );
  const size_t HeaderEnd = Raw.find( "\r\n\r\n" );
  Response = HeaderEnd == TGNString::npos ? "" : Raw.substr( HeaderEnd + 4 );
  return true;
#endif
 }

bool TTimeConfigSync::SyncNow(void)
 {
  if( FTimeStamp.empty() || FApplication == nullptr ) return false;
  int StatusCode = 0;
  TGNString Response;
  if( !HttpPost( BuildPayload(), StatusCode, Response ) || StatusCode < 200 || StatusCode >= 300 ) return false;
#if defined( SYSTEM_ESP )
  JsonDocument Document;
  if( deserializeJson( Document, Response.c_str() ) ) return false;
  const char * Action = Document[ "Action" ] | "";
  if( String( Action ) != "UseCentral" && String( Action ) != "Synced" ) return false;
  if( !GnDeviceIdentity.ApplyEnvelope( Response ) ) return false;
  if( String( Action ) == "UseCentral" )
   {
    TGNString Content; serializeJson( Document[ "Content" ], Content );
    TGNString Error;
    const char * TimeStamp = Document[ "TimeStamp" ] | "";
    return FApplication->ApplyRemoteTimes( Content, TimeStamp, Error ) && GnDeviceIdentity.CommitAssignment();
   }
  if( String( Action ) != "Synced" ) return false;
#else
  json_object * Root = json_tokener_parse( Response.c_str() );
  if( Root == nullptr ) return false;
  json_object * Value = nullptr;
  TGNString Action;
  if( json_object_object_get_ex( Root, "Action", &Value ) && json_object_is_type( Value, json_type_string ) ) Action = json_object_get_string( Value );
  if( ( Action != "UseCentral" && Action != "Synced" ) || !GnDeviceIdentity.ApplyEnvelope( Response ) )
   { json_object_put( Root ); return false; }
  if( Action == "UseCentral" )
   {
    json_object * Content = nullptr;
    const TGNString TimeStamp = json_object_object_get_ex( Root, "TimeStamp", &Value ) ? json_object_get_string( Value ) : "";
    if( !json_object_object_get_ex( Root, "Content", &Content ) ) { json_object_put( Root ); return false; }
    const TGNString ContentText = json_object_to_json_string_ext( Content, JSON_C_TO_STRING_PLAIN );
    TGNString Error;
    const bool Ok = FApplication->ApplyRemoteTimes( ContentText, TimeStamp, Error );
    json_object_put( Root );
    return Ok && GnDeviceIdentity.CommitAssignment();
   }
  json_object_put( Root );
  if( Action != "Synced" ) return false;
#endif
  if( !GnDeviceIdentity.CommitAssignment() ) return false;
  if( !FSynced )
   {
    FSynced = true;
    SaveMeta();
   }
  return true;
 }

int TTimeConfigSync::HandleIncoming( TGNString const & Body, TGNString & Response )
 {
#if defined( SYSTEM_ESP )
  JsonDocument Document;
  if( deserializeJson( Document, Body.c_str() ) ) { Response = "{\"Ok\":false}"; return 400; }
  const char * TimeStamp = Document[ "TimeStamp" ] | "";
  if( TGNString( Document["Device"] | "" ) != GnDeviceIdentity.DeviceId() ||
      strcmp( Document["Type"] | "", "SwitchTimes" ) != 0 ||
      strcmp( Document["Format"] | "", "MyTimer" ) != 0 || TimeStamp[0] == '\0' || Document["Content"].isNull() )
   { Response = "{\"Ok\":false,\"Error\":\"InvalidPayload\"}"; return 400; }
  if( FTimeStamp > TimeStamp && !GnDeviceIdentity.AssignmentPending( Body ) ) { Response = "{\"Ok\":false,\"Reason\":\"OlderTimeStamp\"}"; return 409; }
  if( !GnDeviceIdentity.ApplyEnvelope( Body ) ) { Response = "{\"Ok\":false,\"Error\":\"InvalidIdentityOrStorage\"}"; return 400; }
  if( FTimeStamp == TimeStamp && !GnDeviceIdentity.AssignmentPending( Body ) ) { Response = "{\"Ok\":true}"; return 200; }
  TGNString Content; serializeJson( Document[ "Content" ], Content );
#else
  json_object * Root = json_tokener_parse( Body.c_str() );
  json_object * Value = nullptr;
  json_object * ContentObject = nullptr;
  if( Root == nullptr || !json_object_object_get_ex( Root, "TimeStamp", &Value ) || !json_object_is_type( Value, json_type_string ) ||
      !json_object_object_get_ex( Root, "Content", &ContentObject ) )
   { if( Root != nullptr ) json_object_put( Root ); Response = "{\"Ok\":false}"; return 400; }
  const TGNString TimeStamp = json_object_get_string( Value );
  auto Matches = [&]( char const * Key, TGNString const & Expected )
   {
    json_object * Field = nullptr;
    return json_object_object_get_ex( Root, Key, &Field ) && json_object_is_type( Field, json_type_string ) &&
      Expected == json_object_get_string( Field );
   };
  if( TimeStamp.empty() || !ContentObject || !Matches( "Device", GnDeviceIdentity.DeviceId() ) ||
      !Matches( "Type", "SwitchTimes" ) || !Matches( "Format", "MyTimer" ) )
   { json_object_put( Root ); Response = "{\"Ok\":false,\"Error\":\"InvalidPayload\"}"; return 400; }
  if( !FTimeStamp.empty() && FTimeStamp > TimeStamp && !GnDeviceIdentity.AssignmentPending( Body ) ) { json_object_put( Root ); Response = "{\"Ok\":false,\"Reason\":\"OlderTimeStamp\"}"; return 409; }
  if( !GnDeviceIdentity.ApplyEnvelope( Body ) ) { json_object_put( Root ); Response = "{\"Ok\":false,\"Error\":\"InvalidIdentityOrStorage\"}"; return 400; }
  if( FTimeStamp == TimeStamp && !GnDeviceIdentity.AssignmentPending( Body ) ) { json_object_put( Root ); Response = "{\"Ok\":true}"; return 200; }
  const TGNString Content = json_object_to_json_string_ext( ContentObject, JSON_C_TO_STRING_PLAIN );
#endif
  TGNString Error;
  const bool Ok = FApplication != nullptr && FApplication->ApplyRemoteTimes( Content, TimeStamp, Error );
#if !defined( SYSTEM_ESP )
  json_object_put( Root );
#endif
  const bool Committed = Ok && GnDeviceIdentity.CommitAssignment();
  Response = Committed ? "{\"Ok\":true}" : "{\"Ok\":false,\"Error\":\"ApplyFailed\"}";
  return Committed ? 200 : 400;
 }

void TTimeConfigSync::Loop(void)
 {
  if( FAttemptTimer.Elapsed() < FAttemptDelayMs )
    return;
  const bool Ok = SyncNow();
  FAttemptDelayMs = Ok ? PeriodicMs : RetryMs;
  FAttemptTimer.Start();
 }
