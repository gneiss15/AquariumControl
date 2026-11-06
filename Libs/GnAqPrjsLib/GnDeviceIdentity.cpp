// GnDeviceIdentity.cpp, Version: 1.09
#define Do_TJsonIdentity
#define Do_TJsonIdentityEnvelope
#define Do_TJsonSources
#include <GnBasics.h>
#include <GnJsonSchema.h>
#include <GnDeviceIdentity.h>
#include <AddressResolver.h>
#include <cctype>
#include <cstdio>

#if defined( GNPlatformESP )
#include <ArduinoJson.h>
#include <LittleFS.h>
#if defined( GNPlatformESP32 )
#include <WiFi.h>
#include <HTTPClient.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif
#else
#include <json-c/json.h>
#include <fstream>
#include <sstream>
#include <iterator>
#include <climits>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#endif

namespace
 {
bool ValidId( TGNString const & Id, unsigned Service )
 {
  if( Id.length() != 19 || Id[17] != '@' || Id[18] != static_cast<char>( '0' + Service ) ) return false;
  bool Nonzero = false;
  for( size_t I = 0; I < 17; ++I )
   {
    if( I % 3 == 2 ) { if( Id[I] != ':' ) return false; }
    else
     {
      if( !( ( Id[I] >= '0' && Id[I] <= '9' ) || ( Id[I] >= 'A' && Id[I] <= 'F' ) ) ) return false;
      Nonzero = Nonzero || Id[I] != '0';
     }
   }
  return Nonzero;
 }

TGNString ReadFile( TGNString const & Path )
 {
#if defined( GNPlatformESP )
  File FileHandle = LittleFS.open( Path.c_str(), "r" );
  if( !FileHandle || FileHandle.size() > 8192 ) return {};
  TGNString Text;
  if( !GnReserveStringExact( Text, FileHandle.size() ) ) return {};
  while( FileHandle.available() ) Text += static_cast<char>( FileHandle.read() );
  return Text;
#else
  std::ifstream File( Path );
  return File ? TGNString( std::istreambuf_iterator<char>( File ), std::istreambuf_iterator<char>() ) : TGNString{};
#endif
 }

bool SaveFile( TGNString const & Path, TGNString const & Text )
 {
  const TGNString Temporary = Path + ".new";
#if defined( GNPlatformESP )
  File FileHandle = LittleFS.open( Temporary.c_str(), "w" );
  if( !FileHandle ) return false;
  const bool Ok = FileHandle.write( reinterpret_cast<uint8_t const *>( Text.data() ), Text.length() ) == Text.length();
  FileHandle.flush();
  FileHandle.close();
  return Ok && LittleFS.rename( Temporary.c_str(), Path.c_str() );
#else
  const int Descriptor = ::open( Temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600 );
  if( Descriptor < 0 ) return false;
  size_t Offset = 0;
  while( Offset < Text.length() )
   {
    const ssize_t Written = ::write( Descriptor, Text.data() + Offset, Text.length() - Offset );
    if( Written <= 0 ) { ::close( Descriptor ); return false; }
    Offset += static_cast<size_t>( Written );
   }
  const bool Ok = fsync( Descriptor ) == 0;
  const bool Closed = ::close( Descriptor ) == 0;
  return Ok && Closed && std::rename( Temporary.c_str(), Path.c_str() ) == 0;
#endif
 }

TGNString LocalMac(void)
 {
#if defined( GNPlatformESP )
  // STA MAC, never the AP MAC; WiFi is initialized before identity setup.
  return WiFi.macAddress().c_str();
#else
  // Select the default-route interface with the lowest metric, not an arbitrary
  // Ethernet/WLAN interface. The registry verifies this identity before syncing.
  std::ifstream Routes( "/proc/net/route" );
  TGNString Line, Interface;
  unsigned BestMetric = UINT_MAX;
  std::getline( Routes, Line );
  while( std::getline( Routes, Line ) )
   {
    std::istringstream Row( Line );
    TGNString Name, Destination, Gateway, Flags;
    unsigned Ref, Use, Metric;
    if( Row >> Name >> Destination >> Gateway >> Flags >> Ref >> Use >> Metric &&
        Destination == "00000000" && ( std::stoul( Flags, nullptr, 16 ) & 1 ) && Metric < BestMetric )
     { Interface = Name; BestMetric = Metric; }
   }
  if( Interface.empty() ) return {};
  std::ifstream Address( "/sys/class/net/" + Interface + "/address" );
  TGNString Mac;
  std::getline( Address, Mac );
  for( char & C : Mac ) C = static_cast<char>( std::toupper( static_cast<unsigned char>( C ) ) );
  return Mac;
#endif
 }

TGNString StateJson( TGNString const & Id, std::map<TGNString,TGNString> const & Sources,
                      bool Report, unsigned Service, bool Prepared, TGNString const & Function,
                      TGNString const & Revision, TGNString const & AppliedRevision, bool LocalSources, uint64_t BootCounter )
 {
  TJsonIdentity State;
  State.Ver = 1;
  State.ServiceId = Id;
  State.LocalSrcs = LocalSources;
  State.BootCnt = BootCounter;
  State.Func = Function;
  State.Rev = Revision;
  State.AppliedRev = AppliedRevision;
  State.Srcs = Sources;
  if( Report )State.Prepared = Prepared;
  (void)Service;
  return GnWriteJson( State );
 }
 } // namespace

void TGnDeviceIdentity::Setup( TGNString const & InitialFunction, unsigned ServiceId, TGNString const & StateFile )
 {
  FInitialFunction = InitialFunction;
  FServiceId = ServiceId;
  FStateFile = StateFile;
  FSources.clear();
  FPrepared = false;
  FFunctionChanged = FLocalSources = false;
  FBootCounter = 0;
  FFunction = InitialFunction;
  FRevision.clear();
  FAppliedRevision.clear();
  char Suffix[4];
  std::snprintf( Suffix, sizeof( Suffix ), "@%u", ServiceId );
  FDeviceId = LocalMac();
  FDeviceId += Suffix;
  if( !ValidId( FDeviceId, ServiceId ) ) { FDeviceId.clear(); return; }
  ApplyState( ReadFile( FStateFile ), false );
  ++FBootCounter;
  // A persisted boot generation lets the Dashboard confirm a real restart.
  if( !SaveFile( FStateFile, StateJson( FDeviceId, FSources, false, FServiceId, FPrepared,
       Function(), FRevision, FAppliedRevision, FLocalSources, FBootCounter ) + "\n" ) ) FPrepared = false;
 }

bool TGnDeviceIdentity::ApplyState( TGNString const & Text, bool Persist )
 {
  if( FDeviceId.empty() || Text.empty() || Text.length() > 8192 ) return false;
  std::map<TGNString,TGNString> Sources;
  TGNString Function, Revision, AppliedRevision;
  bool LocalSources = false;
  uint64_t BootCounter = FBootCounter;
  TJsonIdentity State;
  if( !GnReadJsonValue( Text, State ) || !State.ServiceId.Known() || State.ServiceId != FDeviceId )return false;
  Sources = State.Srcs;
  Function = State.Func.Present ? State.Func.Value : FInitialFunction;
  Revision = State.Rev.Value;
  AppliedRevision = State.AppliedRev.Value;
  if( !Persist ) { LocalSources = State.LocalSrcs.Value;BootCounter = State.BootCnt.Value; }
  if( Function.empty() || Function.length() > 80 ||
      ( Persist && Revision.empty() ) || ( !FRevision.empty() && Revision < FRevision ) || Sources.size() > 32 ) return false;
  if( !FRevision.empty() && Revision == FRevision &&
      ( Function != FFunction || ( Sources != FSources && !FLocalSources ) ) ) return false;
  if( Persist ) AppliedRevision = FAppliedRevision;
  const bool FunctionChanged = Function != FFunction;

  for( auto const & Source : Sources )
    if( Source.first.empty() || Source.first.length() > 80 || !ValidId( Source.second, 3 ) ) return false;
  // Store first; a failed write never acknowledges or changes the live mapping.
  if( Persist && ( !FPrepared || Sources != FSources || FLocalSources != LocalSources || FunctionChanged || Revision != FRevision ) &&
      !SaveFile( FStateFile, StateJson( FDeviceId, Sources, false, FServiceId, true,
        Function, Revision, AppliedRevision, LocalSources, BootCounter ) + "\n" ) ) return false;
  FSources = std::move( Sources );
  FLocalSources = LocalSources;
  FBootCounter = BootCounter;
  FFunction = Function;
  FRevision = Revision;
  FAppliedRevision = AppliedRevision;
  FFunctionChanged = FFunctionChanged || ( Persist && FunctionChanged );
  FPrepared = true;
  return true;
 }

bool TGnDeviceIdentity::ApplyEnvelope( TGNString const & Envelope )
 {
  TJsonIdentityEnvelope EnvelopeData;
  if( !GnReadJsonValue( Envelope, EnvelopeData ) )return false;
  return ApplyState( GnWriteJson( EnvelopeData.Identity ), true );
 }

TGNString TGnDeviceIdentity::ReportJson(void) const
 {
  return StateJson( FDeviceId, FSources, true, FServiceId, FPrepared,
    Function(), FRevision, FAppliedRevision, FLocalSources, FBootCounter );
 }

TGNString TGnDeviceIdentity::ResolveTemperatureSource( TGNString const & Function ) const
 {
  TChannelAddress Address;
  if(TChannelAddress::Parse(Function,Address))
   {
    if(Address.HasService)return ServiceAddress(Address);
    // A MAC-only reference must resolve uniquely against known measurement services.
    std::vector<TPhysicalEndpoint> Known;
    for(auto const & Source:FSources)Known.push_back({Source.second+"/0",2});
    TGNString Resolved;
    if(!ResolveChannelAddress(Function,Known,2,Resolved) || !TChannelAddress::Parse(Resolved,Address))return {};
    return ServiceAddress(Address);
   }
  // Local source queries use function names; received profile addresses do not.
  auto Source=FSources.find(Function);
  const TGNString Preferred=Source==FSources.end()?TGNString{}:Source->second;
  return FSourceResolver && !FLocalSources ? FSourceResolver(Function,Preferred,FRevision) : Preferred;
 }

bool TGnDeviceIdentity::SetSources( TGNString const & Json, bool DashboardAvailable )
 {
  if( FDeviceId.empty() || DashboardAvailable || ( FServiceId != 1 && FServiceId != 4 ) || Json.size() > 8192 ) return false;
  std::map<TGNString,TGNString> Sources;
  TJsonSources SourceData;
  if( !GnReadJsonValue( Json, SourceData ) )return false;
  Sources = std::move( SourceData.Value );
  if( Sources.size() > 32 ) return false;
  for( auto const & Source : Sources )
    if( Source.first.empty() || Source.first.size() > 80 || !ValidId( Source.second, 3 ) ) return false;
  if( Sources == FSources ) return true;
  if( !SaveFile( FStateFile, StateJson( FDeviceId, Sources, false, FServiceId, FPrepared,
      Function(), FRevision, FAppliedRevision, true, FBootCounter ) + "\n" ) ) return false;
  FSources = std::move( Sources );
  FLocalSources = true;
  return true;
 }

bool TGnDeviceIdentity::AssignmentPending( TGNString const & Envelope ) const
 {
  TGNString Revision;
#if defined( GNPlatformESP )
  JsonDocument Document;
  if( deserializeJson( Document, Envelope.c_str() ) ) return false;
  Revision = Document["Identity"]["Rev"] | "";
#else
  json_object * Root = json_tokener_parse( Envelope.c_str() ), * Identity = nullptr, * Value = nullptr;
  if( Root && json_object_object_get_ex( Root, "Identity", &Identity ) &&
      json_object_object_get_ex( Identity, "Rev", &Value ) && json_object_is_type( Value, json_type_string ) )
    Revision = json_object_get_string( Value );
  if( Root ) json_object_put( Root );
#endif
  return !Revision.empty() && Revision > FAppliedRevision && Revision >= FRevision;
 }

bool TGnDeviceIdentity::CommitAssignment(void)
 {
  if( FAppliedRevision != FRevision )
   {
    if( !SaveFile( FStateFile, StateJson( FDeviceId, FSources, false, FServiceId, FPrepared,
         Function(), FRevision, FRevision, FLocalSources, FBootCounter ) + "\n" ) ) return false;
    FAppliedRevision = FRevision;
   }
  FFunctionChanged = false;
  return true;
 }

TGnDeviceIdentity GnDeviceIdentity;

bool GnDashboardAvailable(void)
 {
#if defined( GNPlatformESP )
  WiFiClient Client;
  HTTPClient Http;
  Http.setTimeout( 750 );
#if defined( GNPlatformESP32 )
  Http.setConnectTimeout( 500 );
#endif
  if( !Http.begin( Client, "http://192.168.99.51:8090/api/version" ) ) return false;
  const int Status = Http.GET();
  Http.end();
  return Status == 200;
#else
  const int Socket = socket( AF_INET, SOCK_STREAM, 0 );
  if( Socket < 0 ) return false;
  struct TClose { int F; ~TClose(){ close( F ); } } Close{ Socket };
  const int Flags = fcntl( Socket, F_GETFL, 0 );
  if( Flags < 0 || fcntl( Socket, F_SETFL, Flags | O_NONBLOCK ) < 0 ) return false;
  sockaddr_in Address{}; Address.sin_family = AF_INET; Address.sin_port = htons( 8090 );
  inet_pton( AF_INET, "127.0.0.1", &Address.sin_addr );
  if( connect( Socket, reinterpret_cast<sockaddr *>( &Address ), sizeof( Address ) ) != 0 )
   {
    pollfd Poll{ Socket, POLLOUT, 0 };
    int Error = 0; socklen_t Size = sizeof( Error );
    if( poll( &Poll, 1, 500 ) <= 0 || getsockopt( Socket, SOL_SOCKET, SO_ERROR, &Error, &Size ) != 0 || Error ) return false;
   }
  if( fcntl( Socket, F_SETFL, Flags ) < 0 ) return false;
  timeval Timeout{ 0, 750000 };
  setsockopt( Socket, SOL_SOCKET, SO_RCVTIMEO, &Timeout, sizeof( Timeout ) );
  setsockopt( Socket, SOL_SOCKET, SO_SNDTIMEO, &Timeout, sizeof( Timeout ) );
  const char Request[] = "GET /api/version HTTP/1.1\r\nHost: 127.0.0.1:8090\r\nConnection: close\r\n\r\n";
  if( send( Socket, Request, sizeof( Request ) - 1, MSG_NOSIGNAL ) != static_cast<ssize_t>( sizeof( Request ) - 1 ) ) return false;
  TGNString Header; char Buffer[128];
  while( Header.find( "\r\n" ) == TGNString::npos && Header.size() < 512 )
   {
    const ssize_t Count = recv( Socket, Buffer, sizeof( Buffer ), 0 );
    if( Count <= 0 ) return false;
    Header.append( Buffer, static_cast<size_t>( Count ) );
   }
  return Header.rfind( "HTTP/1.1 200 ", 0 ) == 0 || Header.rfind( "HTTP/1.0 200 ", 0 ) == 0;
#endif
 }
