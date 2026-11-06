// GnTimeSync.cpp, Version: 1.17
#define Do_THttpResult
#define Do_THttpTimePayload
#define Do_THttpTimeReply
#define Do_TJsonFeedTimes
#define Do_TJsonIntervals
#define Do_TJsonSchedule
#define Do_TJsonSwitchTimes
#define Do_TJsonSyncMeta
#define Do_TUdpLastError
#include <GnBasics.h>
#include <GnJsonResponses.h>
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
  FTimeStamp="1970-01-01T00:00:00.000Z";
  TJsonSyncMeta Meta;
  if(!GnReadJsonValue(ReadTextFile(FConfig.MetaFile),Meta))return;
  FTimeStamp=Meta.TimeStamp;
  FSynced=Meta.Synced;
 }

bool TGnTimeSync::SaveMeta(void) const
 {
  TJsonSyncMeta Meta;
  Meta.TimeStamp=FTimeStamp;Meta.Synced=FSynced;
  const TGNString Text=GnWriteJson(Meta);
  return !Text.empty() && WriteTextFile(FConfig.MetaFile,Text+"\n");
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
  THttpTimePayload Payload;
  Payload.Dev=GnDeviceIdentity.DeviceId();
  Payload.Type=FConfig.Type;
  Payload.Format=FConfig.Format;
  Payload.TimeStamp=FTimeStamp;
  if(!GnReadJsonValue(GnDeviceIdentity.ReportJson(),Payload.Identity))return {};
  const TGNString Text=ReadTextFile(FConfig.DataFile);
  if(FConfig.ChannelFile)
   {
    TJsonSchedule Content;
    if(Payload.Format=="TmrSw")
     {
      TJsonSwitchTimes Times;
      if(!GnReadJsonValue(Text,Times))return {};
      Content.SwitchTimes=std::move(Times);
      if(FConfig.IntervalFile && LittleFS.exists(FConfig.IntervalFile))
       {
        TJsonIntervals Intervals;
        if(!GnReadJsonValue(ReadTextFile(FConfig.IntervalFile),Intervals))return {};
        Content.Intervals=Intervals;
       }
     }
    else if(!GnReadJsonValue(Text,Content))return {};
    if(!GnReadJsonValue(ReadTextFile(FConfig.ChannelFile),Content.Chs))return {};
    Payload.Content=std::move(Content);
   }
  else
   {
    TJsonFeedTimes Times;
    if(!GnReadJsonValue(Text,Times))return {};
    Payload.Content=std::move(Times);
   }
  return GnWriteJson(Payload);
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
  if(FTimeStamp.empty())return false;
  const TGNString Body=BuildPayload();
  if(Body.empty())return false;
  int StatusCode=0;TGNString Response;
  if(!HttpPost(SyncUrl,Body,StatusCode,Response) || StatusCode<200 || StatusCode>=300)return false;
  THttpTimeReply Reply;
  if(!GnReadJsonValue(Response,Reply) || !Reply.Action.Known() || !Reply.TimeStamp.Known() || !Reply.Identity.Known())return false;
  if(!GnDeviceIdentity.ApplyEnvelope(Response) || !ApplyFunctionConfig(Response))return false;
  if(Reply.Action=="UseCentral")
   {
    if(!Reply.Content.Known())return false;
    const TGNString Content=GnWriteJson(Reply.Content.Value);
    if(FApply(Content.c_str())!="1" || !GnDeviceIdentity.CommitAssignment())return false;
    RemoteApplied(Reply.TimeStamp);
    return true;
   }
  if(!GnDeviceIdentity.CommitAssignment())return false;
  if(!FSynced) {FSynced=true;SaveMeta();}
  return true;
 }

int TGnTimeSync::HandleIncoming(TGNString const & Body,TGNString & Response)
 {
  THttpTimePayload Payload;
  if(!GnReadJsonValue(Body,Payload) || !Payload.Dev.Known() || !Payload.Type.Known() ||
     !Payload.Format.Known() || !Payload.TimeStamp.Known() || !Payload.Content.Known())
   {Response=GnJsonResult(false,"InvalidPayload");return 400;}
  if(Payload.Dev!=GnDeviceIdentity.DeviceId() || Payload.Type!=FConfig.Type || Payload.Format!=FConfig.Format)
   {Response=GnJsonResult(false,"InvalidPayload");return 400;}
  if(!FTimeStamp.empty() && FTimeStamp>Payload.TimeStamp && !GnDeviceIdentity.AssignmentPending(Body))
   {Response=GnJsonResult(false,{},"OlderTimeStamp");return 409;}
  if(!GnDeviceIdentity.ApplyEnvelope(Body) || !ApplyFunctionConfig(Body))
   {Response=GnJsonResult(false,"InvalidIdentityOrStorage");return 400;}
  if(FTimeStamp==Payload.TimeStamp && !GnDeviceIdentity.AssignmentPending(Body))
   {Response=GnJsonResult(true);return 200;}
  const String Result=FApply(GnWriteJson(Payload.Content.Value).c_str());
  if(Result!="1") {Response=GnJsonResult(false,Result.c_str());return 400;}
  if(!GnDeviceIdentity.CommitAssignment()) {Response=GnJsonResult(false,"IdentityStorageFailed");return 500;}
  RemoteApplied(Payload.TimeStamp);
  Response=GnJsonResult(true);
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
