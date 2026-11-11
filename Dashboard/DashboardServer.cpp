// DashboardServer.cpp, Version: 1.44
#define Do_Device_RX
#define Do_THttpDevice
#define Do_THttpFeedbackDocument
#define Do_THttpFeedbackWrite
#define Do_THttpResult
#define Do_TJsonCachedStatus
#define Do_TJsonChannelGroup
#define Do_TJsonFeedbackConfiguration
#define Do_TJsonIdentity
#define Do_TJsonProfile
#define Do_TJsonProfiles
#define Do_TJsonRecordingChannels
#define Do_TJsonRegistryView
#define Do_TSseRecordingEvent
#define Do_TSseStatusBatch
#define Do_TUdpChannel
#define Do_TUdpLastError
#define Do_TUdpMeasurement
#define Do_TUdpStatusData
#include <GnBasics.h>
#include "Version.inc.h"
#include <GnUdpStatus.h>
#include <GnUdpCompact.h>
#include <GnUdpControl.h>
#include <GnUdpSources.h>
#include <GnJsonResponses.h>
#include <DeviceUrl.h>
#include "TimeStore.h"
#include "AssignmentControl.h"
#include "HeatProfileStore.h"
#include "FeedbackMonitor.h"
#include "ChargeLearningDebug.h"

#include <arpa/inet.h>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <iomanip>
#include <ifaddrs.h>
#include <iostream>
#include <json-c/json.h>
#include <map>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <ctime>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace
 {
constexpr std::size_t MaxRequestHeaderSize = 16384;
constexpr std::size_t MaxRequestBodySize = 262144;
constexpr uint32_t DeviceOnlineMaxAgeMs = 20000;
constexpr uint32_t DeviceRequestMinimumIntervalMs = 1000;
volatile std::sig_atomic_t StopRequested = 0;
constexpr char SystemDeviceId[] = "Dashboard";
constexpr char SystemDeviceName[] = "GN-Pi System";

bool ReadThrottledFlags( uint32_t & Flags )
 {
  FILE * Pipe = popen( "/usr/bin/vcgencmd get_throttled 2>/dev/null", "r" );
  if( !Pipe )
    return false;

  char Buffer[128]{};
  TGNString Output;
  while( fgets( Buffer, sizeof( Buffer ), Pipe ) != nullptr )
    Output += Buffer;
  const int Result = pclose( Pipe );
  if( Result != 0 )
    return false;

  constexpr char Prefix[] = "throttled=0x";
  const size_t Pos = Output.find( Prefix );
  if( Pos == TGNString::npos )
    return false;

  char * End = nullptr;
  errno = 0;
  const char * Start = Output.c_str() + Pos + sizeof( Prefix ) - 1;
  const unsigned long Value = std::strtoul( Start, &End, 16 );
  if( errno != 0 || End == Start )
    return false;
  Flags = static_cast<uint32_t>( Value );
  return true;
 }

bool ReadCpuTemperature( double & Temperature )
 {
  std::ifstream Stream( "/sys/class/thermal/thermal_zone0/temp" );
  long MilliCelsius = 0;
  if( !( Stream >> MilliCelsius ) )
    return false;
  Temperature = static_cast<double>( MilliCelsius ) / 1000.0;
  return true;
 }

bool ReadSystemLoad( double & OneMinute, double & FiveMinutes, double & FifteenMinutes )
 {
  std::ifstream Stream( "/proc/loadavg" );
  return static_cast<bool>( Stream >> OneMinute >> FiveMinutes >> FifteenMinutes );
 }

void AddThrottledError( std::vector<TGNString> & Errors, uint32_t Flags,
                        uint32_t CurrentMask, uint32_t HistoricMask,
                        char const * CurrentText, char const * HistoricText )
 {
  if( ( Flags & CurrentMask ) != 0 )
    Errors.emplace_back( CurrentText );
   else if( ( Flags & HistoricMask ) != 0 )
    Errors.emplace_back( HistoricText );
 }

enum class TReadRequestResult
 {
  Complete,
  Incomplete,
  Invalid
 };

void HandleSignal( int )
 {
  StopRequested = 1;
 }

struct TUdpDeviceState
 {
  TGNString StatusJson;
  uint64_t Revision = 0;
  std::chrono::steady_clock::time_point ReceivedAt{};
 };

struct TStatusStream
 {
  int Fd;
  TGNString Pending;
  std::map<TGNString, TGNString> Tokens;
  bool Configuration = false;
  bool Initialized = false;
  uint64_t ConfigurationGeneration = 0;
  uint64_t TimesGeneration = 0;
  std::chrono::steady_clock::time_point Heartbeat{};
 };

class TDashboardServer
 {
 public:
                                      TDashboardServer( unsigned Port,
                                                        std::filesystem::path RootDirectory );
                                     ~TDashboardServer(void);

  bool                                Run(void);

 private:
  TGNString                           DeviceStatusJson( TGNString const & Id );
  void                                OpenStatusStream( int ClientFd, bool Configuration = false );
  void                                UpdateRecordingConfiguration(void);
  void                                UpdateStatusStreams(void);
  bool                                OpenSocket(void);
  bool                                OpenUdpSocket(void);
  void                                CloseSocket(void);
  void                                CloseUdpSocket(void);
  void                                HandleUdpPackets(void);
  void CacheUdpStatus(TGNString const & Packet,uint64_t ReceivedMs);
  void FlushPendingUdp(void);
  void                                HandleDeviceStatusRequest( int ClientFd, TGNString const & Method,
                                                                 TGNString const & Id, bool SendBody );
  void                                HandleSystemDeviceInfoRequest( int ClientFd, TGNString const & Method,
                                                                     bool SendBody ) const;
  TGNString                           ReadDataVersion(void) const;
  TGNString                            BuildSystemStatusJson(void);
  void                                SendUdpRequest( std::vector<TGNString> const & Ids );
  void                                RequestDeviceIfNeeded( TGNString const & Id );
  void                                HandleClient( int ClientFd, sockaddr_in const & ClientAddress );
  TReadRequestResult                  ReadRequest( int ClientFd, TGNString & Request, TGNString & Body ) const;
  void                                HandleLayoutsRequest( int ClientFd, TGNString const & Method,
                                                            TGNString const & Body, bool SendBody ) const;
  void                                SendFile( int ClientFd, std::filesystem::path const & FileName,
                                                bool SendBody ) const;
  void                                SendJson( int ClientFd, TGNString const & Json, bool SendBody ) const;
  void                                SendError( int ClientFd, unsigned StatusCode, char const * StatusText,
                                                 char const * Message, bool SendBody ) const;
  void                                SendHeader( int ClientFd, unsigned StatusCode, char const * StatusText,
                                                  char const * ContentType, std::uintmax_t ContentLength ) const;
  bool                                SendAll( int ClientFd, char const * Data, std::size_t Size ) const;
  std::filesystem::path               ResolvePath( TGNString const & RequestPath ) const;
  static TGNString                    DecodeUrl( TGNString const & Value );
  static char const *                 GetContentType( std::filesystem::path const & FileName );
  static TGNString                    GetClientAddress( sockaddr_in const & Address );

  unsigned                            Port;
  std::filesystem::path               RootDirectory;
  TDeviceRegistry                     Registry;
  TTimeStore                           TimeStore;
  THeatProfileStore                    HeatProfileStore;
  TAssignmentControl                   AssignmentControl;
  TFeedbackMonitor                    FeedbackMonitor;
  TGNString                           FeedbackRevision;
  TGNString                           FeedbackConfiguration = GnWriteJson(TJsonFeedbackConfiguration{});
  std::vector<TStatusStream> StatusStreams;
  std::map<TGNString,TGNString> RecordingConfiguration;
  bool RecordingDirty = true;
  uint64_t RecordingGeneration = 0;
  uint64_t TimesGeneration = 0;
  TGNString RecordingRegistryRevision;
  bool RecordingRegistryPending = false;
  std::chrono::steady_clock::time_point SystemStatusAt{};
  TGNString SystemStatus;
  int                                 ServerFd;
  int                                 UdpFd;
  TGnUdpControl UdpControl;
  TGnUdpDescriptions UdpDescriptions;
  std::map<TGNString,std::pair<TGNString,uint64_t>> PendingUdp;
  std::map<TGNString, TUdpDeviceState> Devices;
  std::map<TGNString, std::chrono::steady_clock::time_point> LastDeviceRequests;
 };

TDashboardServer::TDashboardServer( unsigned Port, std::filesystem::path RootDirectory )
: Port( Port )
, RootDirectory( std::filesystem::weakly_canonical( std::move( RootDirectory ) ) )
, Registry( this->RootDirectory )
, TimeStore( this->RootDirectory, Registry )
, HeatProfileStore( this->RootDirectory, Registry )
, AssignmentControl( Registry, TimeStore )
, ServerFd( -1 )
, UdpFd( -1 )
 {
 }

TDashboardServer::~TDashboardServer(void)
 {
  for( auto const & Stream : StatusStreams ) close( Stream.Fd );
  CloseUdpSocket();
  CloseSocket();
 }

TGNString TDashboardServer::ReadDataVersion(void) const
 {
  std::ifstream File( RootDirectory / "data" / "Version.json" );
  if( !File )
    return "?";

  std::ostringstream Content;
  Content << File.rdbuf();
  json_object * Root = json_tokener_parse( Content.str().c_str() );
  if( !Root )
    return "?";

  json_object * Version = nullptr;
  TGNString Result = "?";
  if( json_object_object_get_ex( Root, "DataVersion", &Version ) &&
      json_object_get_type( Version ) == json_type_string )
   {
    const char * Value = json_object_get_string( Version );
    if( Value && *Value )
     {
      bool Valid = true;
      for( unsigned char Character : TGNString( Value ) )
        if( !std::isalnum( Character ) && Character != '.' && Character != '_' &&
            Character != '+' && Character != '-' )
         {
          Valid = false;
          break;
         }
      if( Valid )
        Result = Value;
     }
   }
  json_object_put( Root );
  return Result;
 }

bool TDashboardServer::Run(void)
 {
  if( !OpenSocket() || !OpenUdpSocket() || !UdpControl.Setup(GnUdpDescriptionPort(Registry.WireId(SystemDeviceId))) )
    return false;

  std::cout << "Dashboard server: http://0.0.0.0:" << Port << "/\n";
  std::cout << "Document root: " << RootDirectory << '\n';
  std::cout << "UDP status port: " << GnUdpStatusPort << '\n';
  SendUdpRequest( {} );

  auto FeedbackFile = RootDirectory / "data/FeedbackMonitors.json";
  if( !std::filesystem::exists( FeedbackFile ) ) FeedbackFile = RootDirectory / "FeedbackMonitors.json";
  if( std::filesystem::exists( FeedbackFile ) )
   {
    auto Configuration = json_object_from_file( FeedbackFile.c_str() );
    if( !Configuration ) throw std::runtime_error( "Cannot load FeedbackMonitors.json" );
    FeedbackMonitor.Configure( Configuration );
    FeedbackConfiguration = json_object_to_json_string_ext( Configuration, JSON_C_TO_STRING_PLAIN );
    json_object_put( Configuration );
   }
  while( !StopRequested )
   {
    if( FeedbackRevision != Registry.Revision() )
     {
      auto View = Registry.View(); FeedbackMonitor.Bind( View ); json_object_put( View );
      FeedbackRevision = Registry.Revision();
     }
    TGNString Info, Address; uint16_t ReplyPort;
    for( unsigned I=0; I<64 && UdpControl.Read(Info,Address,ReplyPort); ++I )
     {
      TGnWireDocument Packet(Info);
      const auto Root=Packet.Root();
      const auto Type=MessageType(Root);
      if(ValidateReceivedMessage(Root,Type) && Type==TMsgType::Info && UdpDescriptions.AcceptInfo(Root))
        FlushPendingUdp(); // Retain original reception time.
     }
    pollfd PollFds[2]{};
    PollFds[0].fd = ServerFd;
    PollFds[0].events = POLLIN;
    PollFds[1].fd = UdpFd;
    PollFds[1].events = POLLIN;
    const int PollResult = poll( PollFds, 2, 500 );
    if( PollResult < 0 )
     {
      if( errno == EINTR )
        continue;
      std::cerr << "poll failed: " << strerror( errno ) << '\n';
      return false;
     }
    TimeStore.Tick();
    AssignmentControl.Tick();
    HeatProfileStore.Tick();
    UpdateStatusStreams();
    if( PollResult == 0 )
      continue;

    if( PollFds[1].revents & POLLIN )
      { HandleUdpPackets(); UpdateStatusStreams(); }
    if( !( PollFds[0].revents & POLLIN ) )
      continue;

    sockaddr_in ClientAddress{};
    socklen_t ClientLength = sizeof( ClientAddress );
    int ClientFd = accept( ServerFd, reinterpret_cast<sockaddr *>( &ClientAddress ), &ClientLength );
    if( ClientFd < 0 )
     {
      if( errno == EINTR )
        continue;
      std::cerr << "accept failed: " << strerror( errno ) << '\n';
      continue;
     }

    timeval Timeout{};
    Timeout.tv_sec = 5;
    setsockopt( ClientFd, SOL_SOCKET, SO_RCVTIMEO, &Timeout, sizeof( Timeout ) );
    setsockopt( ClientFd, SOL_SOCKET, SO_SNDTIMEO, &Timeout, sizeof( Timeout ) );
    HandleClient( ClientFd, ClientAddress );
    bool Streaming = false;
    for( auto const & Stream : StatusStreams ) Streaming = Streaming || Stream.Fd == ClientFd;
    if( !Streaming ) close( ClientFd );
   }

  return true;
 }

bool TDashboardServer::OpenSocket(void)
 {
  ServerFd = socket( AF_INET, SOCK_STREAM, 0 );
  if ( ServerFd < 0 )
   {
    std::cerr << "socket failed: " << strerror( errno ) << '\n';
    return false;
   }

  int ReuseAddress = 1;
  if ( setsockopt( ServerFd, SOL_SOCKET, SO_REUSEADDR,
                   &ReuseAddress, sizeof( ReuseAddress ) ) < 0 )
   {
    std::cerr << "setsockopt failed: " << strerror( errno ) << '\n';
    return false;
   }

  sockaddr_in Address{};
  Address.sin_family = AF_INET;
  Address.sin_addr.s_addr = htonl( INADDR_ANY );
  Address.sin_port = htons( static_cast<uint16_t>( Port ) );
  if ( bind( ServerFd, reinterpret_cast<sockaddr *>( &Address ), sizeof( Address ) ) < 0 )
   {
    std::cerr << "bind failed: " << strerror( errno ) << '\n';
    return false;
   }
  if ( listen( ServerFd, 16 ) < 0 )
   {
    std::cerr << "listen failed: " << strerror( errno ) << '\n';
    return false;
   }
  return true;
 }

bool TDashboardServer::OpenUdpSocket(void)
 {
  UdpFd = socket( AF_INET, SOCK_DGRAM, 0 );
  if( UdpFd < 0 )
   {
    std::cerr << "UDP socket failed: " << strerror( errno ) << '\n';
    return false;
   }

  int Enabled = 1;
  if( setsockopt( UdpFd, SOL_SOCKET, SO_REUSEADDR, &Enabled, sizeof( Enabled ) ) < 0 ||
      setsockopt( UdpFd, SOL_SOCKET, SO_BROADCAST, &Enabled, sizeof( Enabled ) ) < 0 )
   {
    std::cerr << "UDP setsockopt failed: " << strerror( errno ) << '\n';
    return false;
   }

  sockaddr_in Address{};
  Address.sin_family = AF_INET;
  Address.sin_addr.s_addr = htonl( INADDR_ANY );
  Address.sin_port = htons( GnUdpStatusPort );
  if( bind( UdpFd, reinterpret_cast<sockaddr *>( &Address ), sizeof( Address ) ) < 0 )
   {
    std::cerr << "UDP bind failed: " << strerror( errno ) << '\n';
    return false;
   }
  return true;
 }

void TDashboardServer::CloseUdpSocket(void)
 {
  if( UdpFd < 0 )
    return;
  close( UdpFd );
  UdpFd = -1;
 }

void TDashboardServer::SendUdpRequest( std::vector<TGNString> const & Ids )
 {
  if( UdpFd < 0 )
    return;

  TUdpRequestMessage Request;
  Request.ID = Ids;
  const TGNString Json = GnWriteUdpMessage( Request );
  if( Json.empty() )
    return;

  ifaddrs * Interfaces = nullptr;
  if( getifaddrs( &Interfaces ) != 0 )
    return;
  for( ifaddrs * Interface = Interfaces; Interface != nullptr; Interface = Interface->ifa_next )
   {
    if( Interface->ifa_addr == nullptr || Interface->ifa_addr->sa_family != AF_INET ||
        ( Interface->ifa_flags & IFF_UP ) == 0 || ( Interface->ifa_flags & IFF_LOOPBACK ) != 0 ||
        ( Interface->ifa_flags & IFF_BROADCAST ) == 0 || Interface->ifa_broadaddr == nullptr )
      continue;
    sockaddr_in Destination = *reinterpret_cast<sockaddr_in *>( Interface->ifa_broadaddr );
    Destination.sin_port = htons( GnUdpStatusPort );
    sendto( UdpFd, Json.data(), Json.size(), 0,
            reinterpret_cast<sockaddr *>( &Destination ), sizeof( Destination ) );
   }
  freeifaddrs( Interfaces );
 }

void TDashboardServer::RequestDeviceIfNeeded( TGNString const & Id )
 {
  const auto Now = std::chrono::steady_clock::now();
  auto & LastRequest = LastDeviceRequests[Id];
  if( LastRequest.time_since_epoch().count() != 0 &&
      Now - LastRequest < std::chrono::milliseconds( DeviceRequestMinimumIntervalMs ) )
    return;
  LastRequest = Now;
  SendUdpRequest( { Id, Registry.WireId( Id ) } );
 }

void TDashboardServer::HandleUdpPackets(void)
 {
  while( true )
   {
    sockaddr_in From{};
    TGNString Packet;
    if(!GnReadUdpDatagram(UdpFd,Packet,From))
     {
      if(errno != EAGAIN && errno != EWOULDBLOCK)
        std::cerr << "UDP receive failed: " << strerror(errno) << '\n';
      return;
     }
    if(Packet.empty())continue;
    char Sender[INET_ADDRSTRLEN]; inet_ntop(AF_INET,&From.sin_addr,Sender,sizeof(Sender));
    TGnWireDocument Wire(Packet); auto W=Wire.Root();
    const auto Type=MessageType(W);
    if(!ValidateReceivedMessage(W,Type))continue;
    if(Type==TMsgType::SourceRequest)
     {
      if(Registry.Pending())continue;
      for(auto const & Device:Registry.Devices())
       {
        if(Device.second.Id.size()<2 || (Device.second.Id.substr(17)!="@1" && Device.second.Id.substr(17)!="@4"))continue;
        auto * Envelope=json_object_new_object(); Registry.AddPreparation(Device.first,Envelope);
        TGnWireValue E(Envelope);
        const auto Source=E.Get("Identity").Get("Srcs").Get(W.Get("Func").Text()).Text();
        const auto Reply=GnUdpSourceReply(Registry.WireId(SystemDeviceId),W.Get("Func").Text(),Source,Registry.Revision());
        json_object_put(Envelope);
        if(!Reply.empty())UdpControl.Send(Reply,Sender,ntohs(From.sin_port));
        break;
       }
      continue;
     }
    if(Type!=TMsgType::Status)continue;
    const auto WireId=W.Get("ID").Text(); bool Known=Registry.AcceptWireId(WireId);
    for(auto const & Source:Registry.ExternalSources())Known=Known||Source.second==WireId;
    if(!Known)continue;
    TGNString Request;
    const auto Now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const auto Expanded=UdpDescriptions.Expand(W,Now,Request);
    if(!Request.empty())UdpControl.Send(Request,Sender,GnUdpDescriptionPort(WireId));
    if(Expanded.empty()){if(PendingUdp.size()<32||PendingUdp.count(WireId))PendingUdp[WireId]={Packet,Now};continue;}
    PendingUdp.erase(WireId);
    CacheUdpStatus(Expanded,Now);
   }
 }

void TDashboardServer::CacheUdpStatus(TGNString const & Expanded,uint64_t ReceivedMs)
 {
    json_object * Root = json_tokener_parse( Expanded.c_str() );
    if( Root == nullptr || !json_object_is_type( Root, json_type_object ) )
     {
      if( Root != nullptr )
        json_object_put( Root );
      return;
     }

    json_object * Id=nullptr,*Status=nullptr;
    // ReceiveUdp validates the envelope. Here NULL is evaluated according to
    // the cache operation: an ID and an object snapshot are necessary.
    const bool Valid=json_object_object_get_ex(Root,"ID",&Id) &&
      json_object_is_type(Id,json_type_string) &&
      json_object_object_get_ex(Root,"Status",&Status) && json_object_is_type(Status,json_type_object);
    if( !Valid )
     {
      json_object_put( Root );
      return;
     }

    const TGNString DeviceId = json_object_get_string( Id );
    if( DeviceId.empty() )
     {
      json_object_put( Root );
      return;
     }
    const auto Received = std::chrono::steady_clock::time_point(std::chrono::milliseconds(ReceivedMs));
    FeedbackMonitor.Receive( Root, std::chrono::duration_cast<std::chrono::milliseconds>( Received.time_since_epoch() ).count() );
    // Only registered external senders enter the display cache; they remain
    // separate from managed devices and never require configuration APIs.
    bool External = false;
    for( auto const & Source : Registry.ExternalSources() )
      External = External || Source.second == DeviceId;
    if( !External && !Registry.AcceptWireId( DeviceId ) ) { json_object_put( Root ); return; }
    ChargeLearningDebug::Receive(RootDirectory,Root);
    TUdpDeviceState & State = Devices[External ? DeviceId : Registry.Function( DeviceId )];
    State.StatusJson = json_object_to_json_string_ext( Status, JSON_C_TO_STRING_PLAIN );
    State.ReceivedAt = Received;
    ++State.Revision;
    json_object_put( Root );
 }

void TDashboardServer::FlushPendingUdp(void)
 {
  for(auto I=PendingUdp.begin();I!=PendingUdp.end();)
   {
    TGNString Request;
    const auto Expanded=UdpDescriptions.Expand(I->second.first,I->second.second,Request);
    if(Expanded.empty()){++I;continue;}
    CacheUdpStatus(Expanded,I->second.second);
    I=PendingUdp.erase(I);
   }
 }

void TDashboardServer::HandleSystemDeviceInfoRequest( int ClientFd, TGNString const & Method,
                                                         bool SendBody ) const
 {
  if( Method != "GET" && Method != "HEAD" )
   {
    SendError( ClientFd, 405, "Method Not Allowed", "Only GET and HEAD are supported.", SendBody );
    return;
   }

  THttpDevice Device;
  Device.ApiVer=1;
  Device.DevType="System";
  Device.DevId=Registry.WireId(SystemDeviceId);
  Device.Name=SystemDeviceName;
  Device.FirmwareVer=ProjectVersion::ServerRevision;
  Device.Caps=std::vector<TGNString>{"Status","ErrorStatus","Measurements"};
  Device.CfgUrl=DeviceUrl("http",Port,"/");
  Device.Events=nullptr; // System status is distributed by the Dashboard SSE stream.
  SendJson(ClientFd,GnWriteJson(Device)+"\n",SendBody);
 }

TGNString TDashboardServer::BuildSystemStatusJson(void)
 {
  uint32_t ThrottledFlags = 0;
  const bool ThrottledValid = ReadThrottledFlags( ThrottledFlags );
  double CpuTemperature = 0.0;
  const bool TemperatureValid = ReadCpuTemperature( CpuTemperature );
  double LoadOneMinute = 0.0;
  double LoadFiveMinutes = 0.0;
  double LoadFifteenMinutes = 0.0;
  const bool LoadValid = ReadSystemLoad( LoadOneMinute, LoadFiveMinutes, LoadFifteenMinutes );

  std::vector<TGNString> Errors;
  if( !ThrottledValid )
    Errors.emplace_back( "Raspberry-Pi-Spannungs-/Drosselstatus konnte nicht gelesen werden." );
   else
   {
    AddThrottledError( Errors, ThrottledFlags, 1u << 0, 1u << 16,
                       "Unterspannung liegt aktuell an.",
                       "Unterspannung ist seit dem Systemstart aufgetreten." );
    AddThrottledError( Errors, ThrottledFlags, 1u << 1, 1u << 17,
                       "CPU-Takt ist aktuell begrenzt.",
                       "CPU-Taktbegrenzung ist seit dem Systemstart aufgetreten." );
    AddThrottledError( Errors, ThrottledFlags, 1u << 2, 1u << 18,
                       "System wird aktuell gedrosselt.",
                       "Drosselung ist seit dem Systemstart aufgetreten." );
    AddThrottledError( Errors, ThrottledFlags, 1u << 3, 1u << 19,
                       "Temperaturbegrenzung ist aktuell aktiv.",
                       "Temperaturbegrenzung ist seit dem Systemstart aufgetreten." );
   }
  if( !TemperatureValid )
    Errors.emplace_back( "CPU-Temperatur konnte nicht gelesen werden." );
  if( !LoadValid )
    Errors.emplace_back( "Systemlast konnte nicht gelesen werden." );

  const std::time_t CurrentTime = std::time( nullptr );
  TUdpStatusData Status;
  Status.DevId=Registry.WireId(SystemDeviceId);
  Status.CurrentTime=static_cast<uint64_t>(CurrentTime);
  Status.Healthy=Errors.empty();
  Status.LastErr=nullptr;
  Status.Errs=Errors;
  Status.Chs=std::vector<TUdpChannel>{};
  const auto AddMeasurement=[&](char const * Kind,char const * Name,double Value,
                                bool Valid,uint8_t Decimals,char const * Unit,char const * Error)
   {
    TUdpMeasurement Measurement;
    Measurement.Kind=Kind;Measurement.Name=Name;
    if(Valid)Measurement.Value=Value;else Measurement.Value=nullptr;
    Measurement.Valid=Valid;Measurement.Decimals=Decimals;
    Measurement.UpdatedAt=static_cast<uint64_t>(CurrentTime);
    if(Unit && *Unit)Measurement.Unit=Unit;
    if(Valid)Measurement.Err=nullptr;else Measurement.Err=Error;
    Status.Meas.push_back(std::move(Measurement));
   };
  AddMeasurement("CpuTemperature","CPU-Temperatur",CpuTemperature,TemperatureValid,1,"°C","CPU-Temperatur konnte nicht gelesen werden.");
  AddMeasurement("Load1","Systemlast (1 min)",LoadOneMinute,LoadValid,2,"","Systemlast konnte nicht gelesen werden.");
  AddMeasurement("Load5","Systemlast (5 min)",LoadFiveMinutes,LoadValid,2,"","Systemlast konnte nicht gelesen werden.");
  AddMeasurement("Load15","Systemlast (15 min)",LoadFifteenMinutes,LoadValid,2,"","Systemlast konnte nicht gelesen werden.");
  const auto Now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
  json_object * Evaluations=FeedbackMonitor.Status(Now);
  TGNString Error;
  const bool Valid=GnRead(TGnWireValue(Evaluations),Status.FeedbackMonitors,Error);
  json_object_put(Evaluations);
  if(!Valid)return {};
  return GnWriteJson(Status);
 }

void TDashboardServer::HandleDeviceStatusRequest( int ClientFd, TGNString const & Method,
                                                   TGNString const & Id, bool SendBody )
 {
  if( Method != "GET" && Method != "HEAD" )
   {
    SendError( ClientFd, 405, "Method Not Allowed", "Only GET and HEAD are supported.", SendBody );
    return;
   }
  if( Id.empty() )
   {
    SendError( ClientFd, 400, "Bad Request", "Device ID is missing.", SendBody );
    return;
   }

  SendJson( ClientFd, DeviceStatusJson( Id ), SendBody );
 }

TGNString TDashboardServer::DeviceStatusJson( TGNString const & Id )
 {
  const auto Now = std::chrono::steady_clock::now();
  if( Registry.Function( Id ) == SystemDeviceId )
   {
    if( SystemStatusAt.time_since_epoch().count() == 0 || Now - SystemStatusAt >= std::chrono::seconds( 5 ) )
     { SystemStatus = BuildSystemStatusJson(); SystemStatusAt = Now; }
    const auto Revision = std::chrono::duration_cast<std::chrono::milliseconds>( SystemStatusAt.time_since_epoch() ).count();
    TJsonCachedStatus Reply;
    Reply.ID = Registry.WireId( SystemDeviceId );
    Reply.Online = true;
    Reply.Rev = uint64_t( Revision );
    Reply.AgeMs = uint32_t( 0 );
    if( !GnReadJsonValue( SystemStatus, Reply.Status ) )return {};
    return GnWriteJson( Reply ) + "\n";
   }

  const auto Entry = Devices.find( Registry.Function( Id ) );
  uint64_t Revision = 0;
  uint32_t AgeMs = UINT32_MAX;
  bool Online = false;
  TGNString StatusJson = "null";
  if( Entry != Devices.end() )
   {
    Revision = Entry->second.Revision;
    if( Entry->second.ReceivedAt.time_since_epoch().count() != 0 )
     {
      const auto Age = std::chrono::duration_cast<std::chrono::milliseconds>( Now - Entry->second.ReceivedAt ).count();
      AgeMs = Age < 0 ? 0 : static_cast<uint32_t>( std::min<long long>( Age, UINT32_MAX ) );
      Online = AgeMs <= DeviceOnlineMaxAgeMs;
     }
    if( !Entry->second.StatusJson.empty() )
      StatusJson = Entry->second.StatusJson;
   }

  bool External = false;
  for( auto const & Source : Registry.ExternalSources() )
    External = External || Source.second == Id;
  if( !External && ( Entry == Devices.end() || !Online ) )
    RequestDeviceIfNeeded( Id );

  TJsonCachedStatus Reply;
  Reply.ID = Id;
  Reply.Online = Online;
  Reply.Rev = Revision;
  if( AgeMs == UINT32_MAX )Reply.AgeMs = nullptr;
  else Reply.AgeMs = AgeMs;
  if( !GnReadJsonValue( StatusJson, Reply.Status ) )return {};
  return GnWriteJson( Reply ) + "\n";
 }

void TDashboardServer::CloseSocket(void)
 {
  if ( ServerFd < 0 )
    return;
  close( ServerFd );
  ServerFd = -1;
 }


void TDashboardServer::OpenStatusStream( int ClientFd, bool Configuration )
 {
  if( StatusStreams.size() >= 32 )
   { SendError( ClientFd, 503, "Service Unavailable", "Too many status streams", true ); return; }
  const int Flags = fcntl( ClientFd, F_GETFL, 0 );
  if( Flags < 0 || fcntl( ClientFd, F_SETFL, Flags | O_NONBLOCK ) < 0 ) return;
  StatusStreams.push_back( { ClientFd,
    "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\n"
    "X-Content-Type-Options: nosniff\r\nX-Accel-Buffering: no\r\nConnection: keep-alive\r\n\r\nretry: 2000\n\n", {} } );
  StatusStreams.back().Configuration = Configuration;
  StatusStreams.back().TimesGeneration = TimesGeneration;
  UpdateStatusStreams();
 }

// Read configuration only on subscription or a known configuration change.
// Each profile/channel group has its own token, so clients receive only changed parts.
void TDashboardServer::UpdateRecordingConfiguration(void)
 {
  if( !RecordingDirty && RecordingRegistryRevision == Registry.Revision() &&
      RecordingRegistryPending == Registry.Pending() ) return;
  unsigned Code = 0; TGNString Profiles, Channels;
  HeatProfileStore.HandleCentralRequest("GET","",Code,Profiles);
  if( Code != 200 ) return;
  TimeStore.HandleCentralRequest("GET","",Code,Channels);
  if( Code != 200 ) return;
  TJsonProfiles ProfileRows;
  TJsonRecordingChannels ChannelRows;
  if(!GnReadJsonValue(Profiles,ProfileRows) || !GnReadJsonValue(Channels,ChannelRows))return;
  std::map<TGNString,TGNString> Current;
  for(auto const & Profile:ProfileRows.Profs)
    Current["p:"+Profile.ProfName.Value]=GnWriteJson(Profile);
  for(auto const & Channel:ChannelRows.Chs)
    Current["c:"+Channel.first]=GnWriteJson(Channel.second);
  auto View=Registry.View();
  Current["registry"]=TGnWireValue(View).Json();
  json_object_put(View);
  RecordingConfiguration = std::move(Current);
  RecordingRegistryRevision = Registry.Revision();
  RecordingRegistryPending = Registry.Pending();
  RecordingDirty = false;
  ++RecordingGeneration;
 }

void TDashboardServer::UpdateStatusStreams(void)
 {
  if( StatusStreams.empty() ) return;
  bool ConfigurationWanted = false, StatusWanted = false;
  for( auto const & Stream : StatusStreams )
   { ConfigurationWanted |= Stream.Configuration; StatusWanted |= !Stream.Configuration; }
  if( ConfigurationWanted ) UpdateRecordingConfiguration();
  // Resolve current identities every time; removed devices must not linger.
  std::map<TGNString,TJsonCachedStatus> Envelopes;
  if(StatusWanted)
   {
    const auto AddEnvelope=[&](TGNString const & Id)
     {
      TJsonCachedStatus State;
      if(GnReadJsonValue(DeviceStatusJson(Id),State))Envelopes[State.ID]=std::move(State);
     };
    AddEnvelope(SystemDeviceId);
    for(auto const & Device:Registry.Devices())AddEnvelope(Device.second.Id);
    for(auto const & Source:Registry.ExternalSources())AddEnvelope(Source.second);
   }
  for( auto It = StatusStreams.begin(); It != StatusStreams.end(); )
   {
    pollfd State{ It->Fd, POLLIN, 0 };
    bool Failed = poll( &State, 1, 0 ) < 0 || ( State.revents & ( POLLHUP | POLLERR | POLLNVAL ) );
    if( State.revents & POLLIN )
     {
      char Byte;
      if( recv( It->Fd, &Byte, 1, MSG_PEEK | MSG_DONTWAIT ) == 0 ) Failed = true;
     }
    if( It->Configuration )
     {
      if( !RecordingDirty && It->ConfigurationGeneration != RecordingGeneration )
       {
        TSseRecordingEvent Event;
        Event.Snapshot=!It->Initialized;
        Event.Profs=std::map<TGNString,std::variant<TJsonProfile,TGnJsonNull>>{};
        Event.Chs=std::map<TGNString,std::variant<TJsonChannelGroup,TGnJsonNull>>{};
        bool Changed=!It->Initialized;
        const auto Add=[&](TGNString const & Key,TGNString const & Value)
         {
          if(Key=="registry")
           {
            if(Value.empty())Event.Registry=nullptr;
            else
             {
              TJsonRegistryView Row;
              if(!GnReadJsonValue(Value,Row))throw std::runtime_error("Invalid recording registry");
              Event.Registry=std::move(Row);
             }
           }
          else if(Key.rfind("p:",0)==0)
           {
            auto & Row=Event.Profs.Value[Key.substr(2)];
            if(Value.empty())Row=TGnJsonNull{};
            else if(!GnReadJsonValue(Value,Row))throw std::runtime_error("Invalid recording profile");
           }
          else
           {
            auto & Row=Event.Chs.Value[Key.substr(2)];
            if(Value.empty())Row=TGnJsonNull{};
            else if(!GnReadJsonValue(Value,Row))throw std::runtime_error("Invalid recording channels");
           }
          Changed=true;
         };
        for(auto const & Entry:RecordingConfiguration)
          if(!It->Tokens.count(Entry.first) || It->Tokens[Entry.first]!=Entry.second)Add(Entry.first,Entry.second);
        for(auto const & Previous:It->Tokens)
          if(!RecordingConfiguration.count(Previous.first))Add(Previous.first,"");
        if(Changed)It->Pending+=TGNString("event: configuration\ndata: ")+GnWriteJson(Event)+"\n\n";
        It->Tokens = RecordingConfiguration; It->Initialized = true;
        It->ConfigurationGeneration = RecordingGeneration;
       }
      const auto Now = std::chrono::steady_clock::now();
      if( Now-It->Heartbeat >= std::chrono::seconds(15) )
       { It->Pending += ": keepalive\n\n"; It->Heartbeat = Now; }
     }
    else
     {
    if( It->TimesGeneration != TimesGeneration )
     {
      It->Pending += TGNString("event: times\ndata: ") + std::to_string(TimesGeneration) + "\n\n";
      It->TimesGeneration = TimesGeneration;
     }
    TSseStatusBatch Batch;
    for(auto const & Entry:Envelopes)
     {
      auto const & State=Entry.second;
      const TGNString Token=(State.Rev.IsNull?TGNString("null"):std::to_string(State.Rev.Value))+':'+(State.Online?"true":"false");
      if(It->Tokens[Entry.first]!=Token)
       {
        It->Tokens[Entry.first]=Token;
        Batch.Value.push_back(State);
       }
     }
    for( auto Old = It->Tokens.begin(); Old != It->Tokens.end(); )
      if( !Envelopes.count( Old->first ) )
       {
        TJsonCachedStatus Removed;
        Removed.ID = Old->first;
        Removed.Online = false;
        Removed.Rev = nullptr;
        Removed.Status = nullptr;
        Batch.Value.push_back(std::move(Removed)); Old = It->Tokens.erase(Old);
       }
      else ++Old;
    if(!Batch.Value.empty())It->Pending+="data: "+GnWriteJson(Batch)+"\n\n";
     }
    if( It->Pending.size() > 262144 ) Failed = true;
    if( !Failed && !It->Pending.empty() )
     {
      const ssize_t Count = send( It->Fd, It->Pending.data(), It->Pending.size(), MSG_NOSIGNAL | MSG_DONTWAIT );
      if( Count > 0 ) It->Pending.erase( 0, size_t( Count ) );
      else if( Count == 0 || ( errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR ) ) Failed = true;
     }
    if( Failed ) { close( It->Fd ); It = StatusStreams.erase( It ); }
    else ++It;
   }
 }

void TDashboardServer::HandleClient( int ClientFd, sockaddr_in const & ClientAddress )
 {
  TGNString Request;
  TGNString Body;
  TReadRequestResult ReadResult = ReadRequest( ClientFd, Request, Body );
  if( ReadResult == TReadRequestResult::Incomplete )
    return;
  if( ReadResult == TReadRequestResult::Invalid )
   {
    SendError( ClientFd, 400, "Bad Request", "Invalid HTTP request.", true );
    return;
   }

  std::istringstream RequestStream( Request );
  TGNString Method;
  TGNString RequestPath;
  TGNString Protocol;
  RequestStream >> Method >> RequestPath >> Protocol;
  bool SendBody = Method != "HEAD";

  std::cout << GetClientAddress( ClientAddress ) << "  " << Method << "  " << RequestPath << '\n';

  if ( Protocol.rfind( "HTTP/", 0 ) != 0 )
   {
    SendError( ClientFd, 400, "Bad Request", "Invalid HTTP protocol.", SendBody );
    return;
   }

  std::size_t QueryPos = RequestPath.find( '?' );
  if ( QueryPos != TGNString::npos )
    RequestPath.erase( QueryPos );

  TGNString DecodedPath = DecodeUrl( RequestPath );
  if( DecodedPath == "/api/recording-events" )
   {
    if( Method != "GET" ) SendError(ClientFd,405,"Method Not Allowed","GET supported",SendBody);
    else { RecordingDirty = true; OpenStatusStream(ClientFd,true); }
    return;
   }
  if( DecodedPath == "/api/events" )
   {
    if( Method != "GET" ) SendError( ClientFd, 405, "Method Not Allowed", "GET supported", SendBody );
    else OpenStatusStream( ClientFd );
    return;
   }
  if( DecodedPath == "/Devices.json" )
   {
    if( Method != "GET" && Method != "HEAD" )
     {
      SendError( ClientFd, 405, "Method Not Allowed", "Only GET and HEAD are supported.", SendBody );
      return;
     }
    json_object * Configuration = json_object_from_file( ( RootDirectory / "Devices.json" ).c_str() );
    if( !Configuration )
     {
      SendError( ClientFd, 500, "Internal Server Error", "Invalid Devices.json", SendBody );
      return;
     }
    Registry.ApplyAddresses( Configuration );
    json_object * External = json_object_new_object();
    for( auto const & Source : Registry.ExternalSources() )
      json_object_object_add( External, Source.first.c_str(), json_object_new_string( Source.second.c_str() ) );
    json_object_object_add( Configuration, "ExternalSrcs", External );
    SendJson( ClientFd, json_object_to_json_string_ext( Configuration, JSON_C_TO_STRING_PLAIN ), SendBody );
    json_object_put( Configuration );
    return;
   }

#if GN_DEBUG_UDP_RX
  if( DecodedPath == "/api/v1/udp-rx-counters" && ( Method == "GET" || Method == "HEAD" ) )
   {
    SendJson(ClientFd,GnUdpRxCountersJson(),SendBody);
    return;
   }
#endif

  if( DecodedPath == "/api/v1/device" )
   {
    HandleSystemDeviceInfoRequest( ClientFd, Method, SendBody );
    return;
   }

  if( DecodedPath == "/api/version" )
   {
    if( Method != "GET" && Method != "HEAD" )
     {
      SendError( ClientFd, 405, "Method Not Allowed", "Only GET and HEAD are supported.", SendBody );
      return;
     }
    SendJson( ClientFd, TGNString( "{\n  \"ServerVersion\": \"" ) +
                          ProjectVersion::ServerRevision + "\",\n  \"DataVer\": \"" +
                          ReadDataVersion() + "\"\n}\n", SendBody );
    return;
   }

  constexpr char DeviceStatusPrefix[] = "/api/device-status/";
  if( DecodedPath.rfind( DeviceStatusPrefix, 0 ) == 0 )
   {
    HandleDeviceStatusRequest( ClientFd, Method,
                               DecodedPath.substr( sizeof( DeviceStatusPrefix ) - 1 ), SendBody );
    return;
   }

  if( DecodedPath == "/api/layouts" )
   {
    HandleLayoutsRequest( ClientFd, Method, Body, SendBody );
    return;
   }

#if GN_DEBUG_CHARGE_LEARNING
  if( DecodedPath == "/api/debug/charge-learning" )
   {
    if(Method!="GET" && Method!="HEAD") { SendError(ClientFd,405,"Method Not Allowed","Read only",SendBody);return; }
    SendJson(ClientFd,ChargeLearningDebug::Read(RootDirectory),SendBody);return;
   }
#endif
  if( DecodedPath == "/api/feedback-monitors" )
   {
    if( Method == "GET" || Method == "HEAD" )
     {
      THttpFeedbackDocument Document;
      if(!GnReadJsonValue(FeedbackConfiguration,Document.Cfg))
       {SendError(ClientFd,500,"Internal Server Error","Invalid feedback configuration",SendBody);return;}
      SendJson(ClientFd,GnWriteJson(Document),SendBody);return;
     }
    if( Method != "PUT" ) { SendError( ClientFd, 405, "Method Not Allowed", "GET and PUT supported", SendBody ); return; }
    if( Registry.Pending() ) { SendError( ClientFd, 409, "Conflict", "Assignment distribution pending", SendBody ); return; }
    THttpFeedbackWrite Request;
    if(!GnReadJsonValue(Body,Request) || !Request.Previous.Known() || !Request.Cfg.Known())
     {SendError(ClientFd,400,"Bad Request","Invalid feedback configuration update",SendBody);return;}
    TGnWireDocument Current(FeedbackConfiguration),Previous(GnWriteJson(Request.Previous.Value));
    if(!json_object_equal(Current.Root().V,Previous.Root().V))
     {SendError(ClientFd,409,"Conflict","Reload feedback monitor configuration first",SendBody);return;}
    const TGNString Text=GnWriteJson(Request.Cfg.Value);
    TGnWireDocument Configuration(Text);
    TFeedbackMonitor Candidate;
    try {Candidate.Configure(Configuration.Root().V);auto View=Registry.View();Candidate.Bind(View);json_object_put(View);}
    catch(std::exception const & Error)
     {SendError(ClientFd,400,"Bad Request",Error.what(),SendBody);return;}
    auto Directory = RootDirectory / "data"; std::filesystem::create_directories( Directory );
    auto Pattern = ( Directory / ".FeedbackMonitors.XXXXXX" ).string();
    std::vector<char> Temporary( Pattern.begin(), Pattern.end() ); Temporary.push_back( '\0' );
    const int File = mkstemp( Temporary.data() ); bool Saved = File >= 0;
    size_t Offset = 0;
    while( Saved && Offset < Text.size() )
     {
      const auto Written = write( File, Text.data() + Offset, Text.size() - Offset );
      if( Written < 0 && errno == EINTR ) continue;
      if( Written <= 0 ) Saved = false; else Offset += static_cast<size_t>( Written );
     }
    if( File >= 0 ) { Saved = Saved && fsync( File ) == 0; if( close( File ) != 0 ) Saved = false; }
    if( Saved ) Saved = rename( Temporary.data(), ( Directory / "FeedbackMonitors.json" ).c_str() ) == 0;
    if( !Saved )
     { if( File >= 0 ) unlink( Temporary.data() ); SendError( ClientFd, 500, "Internal Server Error", "Cannot save feedback monitor configuration", SendBody ); return; }
    const int Parent = open( Directory.c_str(), O_RDONLY | O_DIRECTORY );
    if( Parent >= 0 ) { fsync( Parent ); close( Parent ); }
    FeedbackMonitor = std::move( Candidate ); FeedbackConfiguration = Text; FeedbackRevision = Registry.Revision();
    SendJson( ClientFd, GnJsonResult(true), SendBody ); return;
   }

  if( DecodedPath == "/api/device-registry" )
   {
    if( Method != "GET" && Method != "HEAD" ) { SendError( ClientFd, 405, "Method Not Allowed", "GET supported", SendBody ); return; }
    json_object * View = Registry.View();
    const TGNString Response = json_object_to_json_string_ext( View, JSON_C_TO_STRING_PLAIN );
    json_object_put( View ); SendJson( ClientFd, Response, SendBody ); return;
   }

  if( DecodedPath == "/api/device-editor" )
   {
    if( Method != "GET" && Method != "HEAD" ) { SendError( ClientFd, 405, "Method Not Allowed", "GET supported", SendBody ); return; }
    json_object * View = Registry.EditorView();
    const TGNString Response = json_object_to_json_string_ext( View, JSON_C_TO_STRING_PLAIN );
    json_object_put( View ); SendJson( ClientFd, Response, SendBody ); return;
   }

  if( DecodedPath == "/api/device-assignments" )
   {
    unsigned Code = 500; TGNString Response;
    AssignmentControl.Handle( Method, Body, Code, Response );
    if( Method == "POST" && Code == 202 ) { Devices.clear(); LastDeviceRequests.clear(); }
    SendHeader( ClientFd, Code, Code == 200 ? "OK" : Code == 202 ? "Accepted" : "Request Failed",
      "application/json; charset=utf-8", Response.size() );
    if( SendBody ) SendAll( ClientFd, Response.data(), Response.size() );
    return;
   }

  if( Registry.Pending() && Method == "PUT" && ( DecodedPath == "/api/times" || DecodedPath == "/api/heatprofiles" ) )
   { SendError( ClientFd, 409, "Conflict", "Assignment distribution pending", SendBody ); return; }

  if( DecodedPath == "/api/times" )
   {
    unsigned StatusCode = 500;
    TGNString Response;
    TimeStore.HandleCentralRequest( Method, Body, StatusCode, Response );
    if( Method == "PUT" && StatusCode == 200 )
     {
      RecordingDirty = true;
      ++TimesGeneration;
     }
    const char * StatusText = StatusCode == 200 ? "OK" : StatusCode == 400 ? "Bad Request" :
                              StatusCode == 405 ? "Method Not Allowed" : "Internal Server Error";
    SendHeader( ClientFd, StatusCode, StatusText, "application/json; charset=utf-8", Response.size() );
    if( SendBody ) SendAll( ClientFd, Response.data(), Response.size() );
    return;
   }

  if( DecodedPath == "/api/times/sync" )
   {
    if( Method != "POST" )
     {
      SendError( ClientFd, 405, "Method Not Allowed", "Only POST is supported.", SendBody );
      return;
     }
    unsigned StatusCode = 500;
    TGNString Response;
    TimeStore.HandleSyncRequest( Body, StatusCode, Response );
    if( StatusCode == 200 )
     {
      RecordingDirty = true;
      ++TimesGeneration;
     }
    const char * StatusText = StatusCode == 200 ? "OK" : StatusCode == 400 ? "Bad Request" :
                              "Internal Server Error";
    SendHeader( ClientFd, StatusCode, StatusText, "application/json; charset=utf-8", Response.size() );
    if( SendBody ) SendAll( ClientFd, Response.data(), Response.size() );
    return;
   }

  if( DecodedPath == "/api/heatprofiles" )
   {
    if( Method != "GET" && Method != "HEAD" && Method != "PUT" )
     {
      SendError( ClientFd, 405, "Method Not Allowed", "GET, HEAD and PUT are supported.", SendBody );
      return;
     }
    unsigned StatusCode = 500;
    TGNString Response;
    HeatProfileStore.HandleCentralRequest( Method, Body, StatusCode, Response );
    if( Method == "PUT" && StatusCode == 200 ) RecordingDirty = true;
    if( Method == "PUT" && StatusCode == 200 )
     {
      json_object * Result = json_tokener_parse( Response.c_str() );
      json_object * Changed = nullptr;
      const bool ProfilesChanged = Result != nullptr &&
        json_object_object_get_ex( Result, "Changed", &Changed ) &&
        json_object_get_boolean( Changed );
      if( Result != nullptr ) json_object_put( Result );
      if( ProfilesChanged )
       {
        TimeStore.MarkAllPending();
        ++TimesGeneration;
       }
     }
    const char * StatusText = StatusCode == 200 ? "OK" : StatusCode == 400 ? "Bad Request" :
                              StatusCode == 405 ? "Method Not Allowed" : "Internal Server Error";
    SendHeader( ClientFd, StatusCode, StatusText, "application/json; charset=utf-8", Response.size() );
    if( SendBody ) SendAll( ClientFd, Response.data(), Response.size() );
    return;
   }

  if( Method != "GET" && Method != "HEAD" )
   {
    SendError( ClientFd, 405, "Method Not Allowed", "Only GET and HEAD are supported.", SendBody );
    return;
   }

  // The assignment journal temporarily contains access data, never serve it as a file.
  if( std::filesystem::path( DecodedPath ).filename().string().rfind( "DeviceAssignments.json", 0 ) == 0 )
   {
    SendError( ClientFd, 403, "Forbidden", "Use the public device registry API.", SendBody );
    return;
   }
  std::filesystem::path FileName = ResolvePath( DecodedPath );
  if ( FileName.empty() )
   {
    SendError( ClientFd, 403, "Forbidden", "Requested path is outside the dashboard directory.", SendBody );
    return;
   }
  if ( std::filesystem::is_directory( FileName ) )
    FileName /= "index.html";
  if ( !std::filesystem::is_regular_file( FileName ) )
   {
    SendError( ClientFd, 404, "Not Found", "File not found.", SendBody );
    return;
   }

  SendFile( ClientFd, FileName, SendBody );
 }

TReadRequestResult TDashboardServer::ReadRequest( int ClientFd, TGNString & Request, TGNString & Body ) const
 {
  char Buffer[4096];
  std::size_t HeaderEnd = TGNString::npos;
  while( HeaderEnd == TGNString::npos )
   {
    if( Request.size() >= MaxRequestHeaderSize )
      return TReadRequestResult::Invalid;
    ssize_t Count = recv( ClientFd, Buffer, sizeof( Buffer ), 0 );
    if( Count <= 0 )
      return TReadRequestResult::Incomplete;
    Request.append( Buffer, static_cast<std::size_t>( Count ) );
    HeaderEnd = Request.find( "\r\n\r\n" );
   }

  HeaderEnd += 4;
  Body.assign( Request.data() + HeaderEnd, Request.size() - HeaderEnd );
  Request.resize( HeaderEnd );

  std::size_t ContentLength = 0;
  std::istringstream HeaderStream( Request );
  TGNString Line;
  std::getline( HeaderStream, Line );
  while( std::getline( HeaderStream, Line ) )
   {
    if( !Line.empty() && Line.back() == '\r' )
      Line.pop_back();
    std::size_t Separator = Line.find( ':' );
    if( Separator == TGNString::npos )
      continue;
    TGNString Name = Line.substr( 0, Separator );
    std::transform( Name.begin(), Name.end(), Name.begin(),
                    []( unsigned char Value ) { return static_cast<char>( std::tolower( Value ) ); } );
    if( Name != "content-length" )
      continue;

    TGNString Value = Line.substr( Separator + 1 );
    std::size_t First = Value.find_first_not_of( " \t" );
    std::size_t Last = Value.find_last_not_of( " \t" );
    if( First == TGNString::npos )
      return TReadRequestResult::Invalid;
    Value = Value.substr( First, Last - First + 1 );
    try
     {
      std::size_t Used = 0;
      unsigned long long Length = std::stoull( Value, &Used );
      if( Used != Value.size() || Length > MaxRequestBodySize )
        return TReadRequestResult::Invalid;
      ContentLength = static_cast<std::size_t>( Length );
     }
    catch( std::exception const & )
     {
      return TReadRequestResult::Invalid;
     }
   }

  if( Body.size() > ContentLength )
    Body.resize( ContentLength );
  while( Body.size() < ContentLength )
   {
    std::size_t Remaining = ContentLength - Body.size();
    ssize_t Count = recv( ClientFd, Buffer, std::min( sizeof( Buffer ), Remaining ), 0 );
    if( Count <= 0 )
      return TReadRequestResult::Incomplete;
    Body.append( Buffer, static_cast<std::size_t>( Count ) );
   }
  return TReadRequestResult::Complete;
 }

void TDashboardServer::HandleLayoutsRequest( int ClientFd, TGNString const & Method,
                                             TGNString const & Body, bool SendBody ) const
 {
  std::filesystem::path FileName = RootDirectory / "data" / "Layouts.json";
  if( Method == "GET" || Method == "HEAD" )
   {
    if( std::filesystem::is_regular_file( FileName ) )
      SendFile( ClientFd, FileName, SendBody );
    else
      SendJson( ClientFd, "{\n  \"Ver\": 1,\n  \"Layouts\": []\n}\n", SendBody );
    return;
   }

  if( Method != "PUT" )
   {
    SendError( ClientFd, 405, "Method Not Allowed", "Only GET, HEAD and PUT are supported.", SendBody );
    return;
   }

  std::size_t First = Body.find_first_not_of( " \t\r\n" );
  std::size_t Last = Body.find_last_not_of( " \t\r\n" );
  if( First == TGNString::npos || Body[First] != '{' || Body[Last] != '}' )
   {
    SendError( ClientFd, 400, "Bad Request", "Layout data must be a JSON object.", SendBody );
    return;
   }

  std::error_code Error;
  std::filesystem::create_directories( FileName.parent_path(), Error );
  if( Error )
   {
    SendError( ClientFd, 500, "Internal Server Error", "Cannot create layout data directory.", SendBody );
    return;
   }

  std::filesystem::path TempFileName = FileName;
  TempFileName += ".tmp";
  {
    std::ofstream File( TempFileName, std::ios::binary | std::ios::trunc );
    if( !File )
     {
      SendError( ClientFd, 500, "Internal Server Error", "Cannot open layout data file.", SendBody );
      return;
     }
    File.write( Body.data(), static_cast<std::streamsize>( Body.size() ) );
    File.flush();
    if( !File )
     {
      File.close();
      std::filesystem::remove( TempFileName, Error );
      SendError( ClientFd, 500, "Internal Server Error", "Cannot write layout data file.", SendBody );
      return;
     }
  }

  std::filesystem::rename( TempFileName, FileName, Error );
  if( Error )
   {
    std::filesystem::remove( TempFileName, Error );
    SendError( ClientFd, 500, "Internal Server Error", "Cannot replace layout data file.", SendBody );
    return;
   }

  SendHeader( ClientFd, 204, "No Content", "application/json; charset=utf-8", 0 );
 }

void TDashboardServer::SendFile( int ClientFd, std::filesystem::path const & FileName,
                                 bool SendBody ) const
 {
  std::error_code Error;
  std::uintmax_t FileSize = std::filesystem::file_size( FileName, Error );
  if ( Error )
   {
    SendError( ClientFd, 500, "Internal Server Error", "Cannot read file size.", SendBody );
    return;
   }

  std::ifstream File( FileName, std::ios::binary );
  if ( !File )
   {
    SendError( ClientFd, 500, "Internal Server Error", "Cannot open file.", SendBody );
    return;
   }

  SendHeader( ClientFd, 200, "OK", GetContentType( FileName ), FileSize );
  if ( !SendBody )
    return;

  char Buffer[16384];
  while ( File )
   {
    File.read( Buffer, sizeof( Buffer ) );
    std::streamsize Count = File.gcount();
    if ( Count > 0 && !SendAll( ClientFd, Buffer, static_cast<std::size_t>( Count ) ) )
      return;
   }
 }

void TDashboardServer::SendJson( int ClientFd, TGNString const & Json, bool SendBody ) const
 {
  SendHeader( ClientFd, 200, "OK", "application/json; charset=utf-8", Json.size() );
  if( SendBody )
    SendAll( ClientFd, Json.data(), Json.size() );
 }

void TDashboardServer::SendError( int ClientFd, unsigned StatusCode, char const * StatusText,
                                  char const * Message, bool SendBody ) const
 {
  TGNString Body = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"><title>";
  Body += StatusText;
  Body += "</title></head><body><h1>";
  Body += StatusText;
  Body += "</h1><p>";
  Body += Message;
  Body += "</p></body></html>";
  SendHeader( ClientFd, StatusCode, StatusText, "text/html; charset=utf-8", Body.size() );
  if ( SendBody )
    SendAll( ClientFd, Body.data(), Body.size() );
 }

void TDashboardServer::SendHeader( int ClientFd, unsigned StatusCode, char const * StatusText,
                                   char const * ContentType, std::uintmax_t ContentLength ) const
 {
  std::ostringstream Header;
  Header << "HTTP/1.1 " << StatusCode << ' ' << StatusText << "\r\n"
         << "Content-Type: " << ContentType << "\r\n"
         << "Content-Length: " << ContentLength << "\r\n"
         << "Cache-Control: no-cache\r\n"
         << "X-Content-Type-Options: nosniff\r\n"
         << "Connection: close\r\n\r\n";
  TGNString HeaderText = Header.str();
  SendAll( ClientFd, HeaderText.data(), HeaderText.size() );
 }

bool TDashboardServer::SendAll( int ClientFd, char const * Data, std::size_t Size ) const
 {
  while ( Size > 0 )
   {
    ssize_t Count = send( ClientFd, Data, Size, MSG_NOSIGNAL );
    if ( Count <= 0 )
      return false;
    Data += Count;
    Size -= static_cast<std::size_t>( Count );
   }
  return true;
 }

std::filesystem::path TDashboardServer::ResolvePath( TGNString const & RequestPath ) const
 {
  TGNString RelativePath = RequestPath;
  while ( !RelativePath.empty() && RelativePath.front() == '/' )
    RelativePath.erase( RelativePath.begin() );

  std::error_code Error;
  std::filesystem::path Candidate = std::filesystem::weakly_canonical( RootDirectory / RelativePath, Error );
  if ( Error )
    return {};

  auto RootIterator = RootDirectory.begin();
  auto CandidateIterator = Candidate.begin();
  while ( RootIterator != RootDirectory.end() && CandidateIterator != Candidate.end() )
   {
    if ( *RootIterator != *CandidateIterator )
      return {};
    ++RootIterator;
    ++CandidateIterator;
   }
  if ( RootIterator != RootDirectory.end() )
    return {};
  return Candidate;
 }

TGNString TDashboardServer::DecodeUrl( TGNString const & Value )
 {
  TGNString Result;
  Result.reserve( Value.size() );
  for ( std::size_t Index = 0; Index < Value.size(); ++Index )
   {
    if ( Value[Index] == '%' && Index + 2 < Value.size() )
     {
      char Hex[3] = {Value[Index + 1], Value[Index + 2], '\0'};
      char * End = nullptr;
      long Decoded = std::strtol( Hex, &End, 16 );
      if ( End == Hex + 2 )
       {
        Result.push_back( static_cast<char>( Decoded ) );
        Index += 2;
        continue;
       }
     }
    Result.push_back( Value[Index] == '+' ? ' ' : Value[Index] );
   }
  return Result;
 }

char const * TDashboardServer::GetContentType( std::filesystem::path const & FileName )
 {
  TGNString Extension = FileName.extension().string();
  std::transform( Extension.begin(), Extension.end(), Extension.begin(),
                  []( unsigned char Value ) { return static_cast<char>( std::tolower( Value ) ); } );
  if ( Extension == ".html" || Extension == ".htm" )
    return "text/html; charset=utf-8";
  if ( Extension == ".css" )
    return "text/css; charset=utf-8";
  if ( Extension == ".js" || Extension == ".mjs" )
    return "text/javascript; charset=utf-8";
  if ( Extension == ".json" )
    return "application/json; charset=utf-8";
  if ( Extension == ".svg" )
    return "image/svg+xml";
  if ( Extension == ".png" )
    return "image/png";
  if ( Extension == ".jpg" || Extension == ".jpeg" )
    return "image/jpeg";
  if ( Extension == ".ico" )
    return "image/x-icon";
  return "application/octet-stream";
 }

TGNString TDashboardServer::GetClientAddress( sockaddr_in const & Address )
 {
  char Buffer[INET_ADDRSTRLEN]{};
  if ( inet_ntop( AF_INET, &Address.sin_addr, Buffer, sizeof( Buffer ) ) == nullptr )
    return "?";
  return Buffer;
 }
 } // namespace

int main( int Argc, char ** Argv )
 {
  unsigned Port = 8090;
  std::filesystem::path RootDirectory = ".";
  try
   {
    if ( Argc > 1 )
      Port = static_cast<unsigned>( std::stoul( Argv[1] ) );
    if ( Argc > 2 )
      RootDirectory = Argv[2];
   }
  catch ( std::exception const & Error )
   {
    std::cerr << "Invalid argument: " << Error.what() << '\n';
    return 1;
   }

  if ( Port == 0 || Port > 65535 )
   {
    std::cerr << "Port must be between 1 and 65535.\n";
    return 1;
   }
  if ( !std::filesystem::is_directory( RootDirectory ) )
   {
    std::cerr << "Dashboard directory not found: " << RootDirectory << '\n';
    return 1;
   }

  std::signal( SIGINT, HandleSignal );
  std::signal( SIGTERM, HandleSignal );

  try
   {
    TDashboardServer Server( Port, RootDirectory );
    return Server.Run() ? 0 : 1;
   }
  catch( std::exception const & Error )
   {
    std::cerr << "Dashboard startup failed: " << Error.what() << '\n';
    return 1;
   }
 }
