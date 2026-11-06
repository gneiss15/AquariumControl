// TimeConfigSync.cpp, Version: 1.20
#define Do_THttpResult
#define Do_THttpTimePayload
#define Do_THttpTimeReply
#define Do_TJsonSchedule
#define Do_TJsonSyncMeta
#define Do_TUdpLastError
#include "BuildConfig.h"
#include "TimeConfigSync.h"
#include <GnJsonResponses.h>
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
  FTimeStamp="1970-01-01T00:00:00.000Z";
  TJsonSyncMeta Meta;
  if(!GnReadJsonValue(ReadTextFile(MetaFile()),Meta))return;
  FTimeStamp=Meta.TimeStamp;FSynced=Meta.Synced;
 }

bool TTimeConfigSync::SaveMeta(void) const
 {
  TJsonSyncMeta Meta;
  Meta.TimeStamp=FTimeStamp;Meta.Synced=FSynced;
  const TGNString Text=GnWriteJson(Meta);
  return !Text.empty() && WriteTextFile(MetaFile(),Text+"\n");
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
#if !defined(SYSTEM_ESP)
  if(std::filesystem::exists(FDataDir+"/Configuration.transaction.json"))return {};
#endif
  TJsonSchedule Content;
  if(!GnReadJsonValue(ReadTextFile(ScheduleFile()),Content))return {};
  if(!GnReadJsonValue(ReadTextFile(FDataDir=="/"?"/Channels.json":FDataDir+"/Channels.json"),Content.Chs))return {};
  THttpTimePayload Payload;
  Payload.Dev=GnDeviceIdentity.DeviceId();
  Payload.Type="SwitchTimes";Payload.Format="MyTimer";
  Payload.TimeStamp=FTimeStamp;Payload.Content=std::move(Content);
  if(!GnReadJsonValue(GnDeviceIdentity.ReportJson(),Payload.Identity))return {};
  return GnWriteJson(Payload);
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
  if(FApplication==nullptr || FTimeStamp.empty())return false;
  const TGNString Body=BuildPayload();
  if(Body.empty())return false;
  int StatusCode=0;TGNString Response;
  if(!HttpPost(Body,StatusCode,Response) || StatusCode<200 || StatusCode>=300)return false;
  THttpTimeReply Reply;
  if(!GnReadJsonValue(Response,Reply) || !Reply.Action.Known() || !Reply.TimeStamp.Known() ||
     !Reply.Identity.Known() || (Reply.Action=="UseCentral" && !Reply.Content.Known()) ||
     !GnDeviceIdentity.ApplyEnvelope(Response))return false;
  if(Reply.Action=="UseCentral")
   {
    TGNString Error;
    return FApplication->ApplyRemoteTimes(GnWriteJson(Reply.Content.Value),Reply.TimeStamp,Error) && GnDeviceIdentity.CommitAssignment();
   }
  if(!GnDeviceIdentity.CommitAssignment())return false;
  if(!FSynced) {FSynced=true;SaveMeta();}
  return true;
 }

int TTimeConfigSync::HandleIncoming(TGNString const & Body,TGNString & Response)
 {
  THttpTimePayload Payload;
  if(!GnReadJsonValue(Body,Payload) || Payload.Dev!=GnDeviceIdentity.DeviceId() ||
     Payload.Type!="SwitchTimes" || Payload.Format!="MyTimer" ||
     !Payload.TimeStamp.Known() || !Payload.Content.Known())
   {Response=GnJsonResult(false,"InvalidPayload");return 400;}
  if(!FTimeStamp.empty() && FTimeStamp>Payload.TimeStamp.Value && !GnDeviceIdentity.AssignmentPending(Body))
   {Response=GnJsonResult(false,{},"OlderTimeStamp");return 409;}
  if(!GnDeviceIdentity.ApplyEnvelope(Body)) {Response=GnJsonResult(false,"InvalidIdentityOrStorage");return 400;}
  if(FTimeStamp==Payload.TimeStamp && !GnDeviceIdentity.AssignmentPending(Body))
   {Response=GnJsonResult(true);return 200;}
  TGNString Error;
  const bool Ok=FApplication && FApplication->ApplyRemoteTimes(GnWriteJson(Payload.Content.Value),Payload.TimeStamp,Error);
  const bool Committed=Ok && GnDeviceIdentity.CommitAssignment();
  Response=GnJsonResult(Committed,Committed?TGNString{}:TGNString("ApplyFailed"));
  return Committed?200:400;
 }

void TTimeConfigSync::Loop(void)
 {
  if( FAttemptTimer.Elapsed() < FAttemptDelayMs )
    return;
  const bool Ok = SyncNow();
  FAttemptDelayMs = Ok ? PeriodicMs : RetryMs;
  FAttemptTimer.Start();
 }
