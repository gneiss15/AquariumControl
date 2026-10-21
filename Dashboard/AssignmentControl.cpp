// AssignmentControl.cpp, Version: 1.07
#include "AssignmentControl.h"
#include "EndpointAssignments.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <charconv>

namespace
 {
TGNString Text( json_object * Object, char const * Key )
 {
  json_object * Value = nullptr;
  return Object && json_object_object_get_ex( Object, Key, &Value ) && json_object_is_type( Value, json_type_string )
    ? json_object_get_string( Value ) : "";
 }
bool Flag( json_object * Object, char const * Key )
 {
  json_object * Value = nullptr;
  return Object && json_object_object_get_ex( Object, Key, &Value ) && json_object_is_type( Value, json_type_boolean ) && json_object_get_boolean( Value );
 }
// Configured capability, independent of whether the current reading is valid.
int SensorCapability( json_object * Status )
 {
  json_object * Measurements = nullptr;
  if( !Status || !json_object_object_get_ex( Status, "Measurements", &Measurements ) ||
      !json_object_is_type( Measurements, json_type_array ) ) return -1;
  for( size_t I = 0; I < json_object_array_length( Measurements ); ++I )
    if( Text( json_object_array_get_idx( Measurements, I ), "Id" ) == "Temperature" ) return 2;
  return 0;
 }
json_object * GetDevice( TDeviceRegistry::TDevice const & Device, char const * Path )
 {
  int Socket = socket( AF_INET, SOCK_STREAM, 0 );
  if( Socket < 0 ) return nullptr;
  struct TClose { int F; ~TClose(){ close( F ); } } Close{ Socket };
  const int Flags = fcntl( Socket, F_GETFL, 0 );
  if( Flags < 0 || fcntl( Socket, F_SETFL, Flags | O_NONBLOCK ) < 0 ) return nullptr;
  sockaddr_in Address{}; Address.sin_family = AF_INET; Address.sin_port = htons( Device.Port );
  if( inet_pton( AF_INET, Device.IP.c_str(), &Address.sin_addr ) != 1 ) return nullptr;
  if( connect( Socket, reinterpret_cast<sockaddr *>( &Address ), sizeof( Address ) ) != 0 )
   {
    pollfd Poll{ Socket, POLLOUT, 0 };
    int Error = 0; socklen_t Size = sizeof( Error );
    if( poll( &Poll, 1, 250 ) <= 0 || getsockopt( Socket, SOL_SOCKET, SO_ERROR, &Error, &Size ) != 0 || Error ) return nullptr;
   }
  if( fcntl( Socket, F_SETFL, Flags ) < 0 ) return nullptr;
  timeval Timeout{ 0, 500000 };
  setsockopt( Socket, SOL_SOCKET, SO_RCVTIMEO, &Timeout, sizeof( Timeout ) );
  setsockopt( Socket, SOL_SOCKET, SO_SNDTIMEO, &Timeout, sizeof( Timeout ) );
  const TGNString Request = TGNString( "GET " ) + Path + " HTTP/1.1\r\nHost: " + Device.IP +
    "\r\nConnection: close\r\n\r\n";
  size_t Offset = 0;
  while( Offset < Request.size() )
   {
    const ssize_t Sent = send( Socket, Request.data() + Offset, Request.size() - Offset, MSG_NOSIGNAL );
    if( Sent <= 0 ) return nullptr;
    Offset += static_cast<size_t>( Sent );
   }
  TGNString Raw; char Buffer[4096];
  size_t BodyStart = TGNString::npos, BodyLength = TGNString::npos;
  while( Raw.size() < 65536 )
   {
    const ssize_t Count = recv( Socket, Buffer, sizeof( Buffer ), 0 );
    if( Count < 0 ) return nullptr;
    if( Count == 0 ) break;
    Raw.append( Buffer, static_cast<size_t>( Count ) );
    if( BodyStart == TGNString::npos )
     {
      const auto HeaderEnd = Raw.find( "\r\n\r\n" );
      if( HeaderEnd != TGNString::npos )
       {
        BodyStart = HeaderEnd + 4;
        TGNString Headers = Raw.substr( 0, HeaderEnd + 2 );
        std::transform( Headers.begin(), Headers.end(), Headers.begin(),
          []( unsigned char C ){ return static_cast<char>( std::tolower( C ) ); } );
        const TGNString Key = "\r\ncontent-length:";
        const auto Position = Headers.find( Key );
        if( Position != TGNString::npos )
         {
          size_t First = Position + Key.size(), Last = Headers.find( "\r\n", First );
          while( First < Last && ( Headers[First] == ' ' || Headers[First] == '\t' ) ) ++First;
          while( Last > First && ( Headers[Last - 1] == ' ' || Headers[Last - 1] == '\t' ) ) --Last;
          const auto Parsed = std::from_chars( Headers.data() + First, Headers.data() + Last, BodyLength );
          if( Parsed.ec != std::errc{} || Parsed.ptr != Headers.data() + Last ||
              BodyLength > 65536 - BodyStart ) return nullptr;
         }
       }
     }
    // ESP8266 may keep the connection open despite our Connection: close request.
    // A complete Content-Length body must not wait for EOF and become "offline".
    if( BodyLength != TGNString::npos && Raw.size() - BodyStart >= BodyLength )
     {
      Raw.resize( BodyStart + BodyLength );
      break;
     }
   }
  if( BodyLength != TGNString::npos && Raw.size() - BodyStart != BodyLength ) return nullptr;
  const auto Start = Raw.find( ' ' ), End = Raw.find( "\r\n\r\n" );
  if( Start == TGNString::npos || Raw.substr( Start + 1, 3 ) != "200" || End == TGNString::npos ) return nullptr;
  return json_tokener_parse( Raw.substr( End + 4 ).c_str() );
 }
 } // namespace

TAssignmentControl::TAssignmentControl( TDeviceRegistry & Registry, TTimeStore & Times )
: FRegistry( Registry ), FTimes( Times )
 {
  if( FRegistry.Pending() && !FTimes.PrepareAssignment() )
    throw std::runtime_error( "Cannot recover pending assignment distribution" );
  auto Endpoints = EndpointView( FRegistry ); TGNString Error;
  const bool Initialized = FRegistry.InitializeEndpoints( Endpoints, Error );
  json_object_put( Endpoints );
  if( !Initialized ) throw std::runtime_error( Error );
 }

json_object * TAssignmentControl::Inspect(void)
 {
  json_object * View = FRegistry.View(), * Rows = nullptr;
  json_object_object_get_ex( View, "Devices", &Rows );
  bool AllReady = true, AllApplied = true;
  for( size_t I = 0; I < json_object_array_length( Rows ); ++I )
   {
    json_object * Row = json_object_array_get_idx( Rows, I );
    auto const & Device = FRegistry.Devices().at( Text( Row, "Function" ) );
    if( Device.Id.back() == '2' ) continue; // Dashboard service is not a remote participant.
    json_object * Info = GetDevice( Device, "/api/v1/device" ), * Identity = nullptr, * Version = nullptr;
    json_object * Expected = json_object_new_object(), * ExpectedIdentity = nullptr, * ExpectedSources = nullptr, * Sources = nullptr;
    FRegistry.AddPreparation( Device.Function, Expected );
    json_object_object_get_ex( Expected, "Identity", &ExpectedIdentity );
    json_object_object_get_ex( ExpectedIdentity, "Sources", &ExpectedSources );
    const bool Identified = Info && json_object_object_get_ex( Info, "Identity", &Identity ) &&
      json_object_object_get_ex( Identity, "Version", &Version ) && json_object_is_type( Version, json_type_int ) &&
      json_object_get_int( Version ) >= 3 && Text( Identity, "DeviceId" ) == Device.Id && Flag( Identity, "Prepared" );
    const bool SourcesMatch = Identified && json_object_object_get_ex( Identity, "Sources", &Sources ) &&
      json_object_equal( Sources, ExpectedSources );
    const bool Ready = SourcesMatch && Text( Identity, "Function" ) == Device.Function &&
      Text( Identity, "Mode" ) == "MAC";
    const bool Applied = Ready && Text( Identity, "Mode" ) == "MAC" &&
      Text( Identity, "AppliedRevision" ) == FRegistry.Revision() &&
      FRegistry.RestartConfirmed( Device.Function, Identity );
    if( Device.Id.back() == '3' )
     {
      auto Status = Info ? GetDevice( Device, "/api/v1/status" ) : nullptr;
      const int Capability = SensorCapability( Status );
      json_object_object_add( Row, "SensorCapabilityId", json_object_new_int( Capability ) );
      json_object_object_add( Row, "CapabilityId", json_object_new_int( Capability ) );
      if( Status ) json_object_put( Status );
     }
    json_object * EndpointVersion = nullptr;
    if( Info ) json_object_object_get_ex( Info, "EndpointAssignments", &EndpointVersion );
    json_object_object_add( Row, "EndpointAssignments", json_object_new_int( EndpointVersion ? json_object_get_int( EndpointVersion ) : 0 ) );
    json_object_object_add( Row, "Online", json_object_new_boolean( Info != nullptr ) );
    json_object_object_add( Row, "Ready", json_object_new_boolean( Ready ) );
    json_object_object_add( Row, "Applied", json_object_new_boolean( Applied ) );
    json_object_object_add( Row, "FirmwareVersion", json_object_new_string( Text( Info, "FirmwareVersion" ).c_str() ) );
    if( Identity ) json_object_object_add( Row, "Identity", json_object_get( Identity ) );
    AllReady = AllReady && Ready;
    AllApplied = AllApplied && Applied;
    json_object_put( Expected );
    if( Info ) json_object_put( Info );
   }
  if( FRegistry.Pending() && AllApplied )
   {
    TGNString Error;
    if( FRegistry.Complete( Error ) ) json_object_object_add( View, "Pending", json_object_new_boolean( false ) );
    else json_object_object_add( View, "Error", json_object_new_string( Error.c_str() ) );
   }
  try
   {
    auto Endpoints = EndpointView( FRegistry );
    for( size_t I = 0; I < json_object_array_length( Endpoints ); ++I )
     {
      auto Endpoint = json_object_array_get_idx( Endpoints, I );
      for( size_t J = 0; J < json_object_array_length( Rows ); ++J )
       {
        auto Device = json_object_array_get_idx( Rows, J ); json_object * Capability = nullptr;
        if( Text( Endpoint, "DeviceId" ) != Text( Device, "DeviceId" ) ||
            !json_object_object_get_ex( Device, "SensorCapabilityId", &Capability ) ) continue;
        const int Value = json_object_get_int( Capability );
        json_object_object_add( Endpoint, "Available", json_object_new_boolean( Value == 2 ) );
        if( Value == 0 ) json_object_object_add( Endpoint, "Capabilities", json_object_new_int( 0 ) );
       }
     }
    json_object_object_add( View, "Endpoints", Endpoints );
   }
  catch( std::exception const & Error ) { json_object_object_add( View, "EndpointError", json_object_new_string( Error.what() ) ); }
  json_object_object_add( View, "AllReady", json_object_new_boolean( AllReady && !FRegistry.Devices().empty() ) );
  json_object_object_add( View, "AllApplied", json_object_new_boolean( AllApplied ) );
  return View;
 }

void TAssignmentControl::Handle( TGNString const & Method, TGNString const & Body, unsigned & Code, TGNString & Response )
 {
  if( Method == "GET" || Method == "HEAD" )
   {
    json_object * View = Inspect();
    Response = json_object_to_json_string_ext( View, JSON_C_TO_STRING_PLAIN );
    json_object_put( View ); Code = 200; return;
   }
  auto Fail = [&]( unsigned Status, char const * Error )
   {
    Code = Status;
    json_object * Result = json_object_new_object();
    json_object_object_add( Result, "Ok", json_object_new_boolean( false ) );
    json_object_object_add( Result, "Error", json_object_new_string( Error ) );
    Response = json_object_to_json_string_ext( Result, JSON_C_TO_STRING_PLAIN ); json_object_put( Result );
   };
  if( Method != "POST" ) { Fail( 405, "GET and POST supported" ); return; }
  json_object * Request = json_tokener_parse( Body.c_str() );
  struct TRelease { json_object * P; ~TRelease(){ if( P ) json_object_put( P ); } } Release{ Request };
  if( !Request || !json_object_is_type( Request, json_type_object ) ) { Fail( 400, "InvalidJson" ); return; }
  if( Text( Request, "ExpectedRevision" ) != FRegistry.Revision() ) { Fail( 409, "StaleAssignmentView" ); return; }
  const auto Operation = Text( Request, "Operation" );
  if( Operation == "Resume" )
   {
    if( !FRegistry.Pending() ) { Fail( 409, "NoPendingOperation" ); return; }
    if( !FTimes.PrepareAssignment() ) { Fail( 500, "CannotPrepareDistribution" ); return; }
    Code = 202; Response = "{\"Ok\":true,\"Pending\":true}"; return;
   }
  if( Operation == "EditDevices" )
   {
    json_object * Rows = nullptr;
    if( !json_object_object_get_ex( Request, "Devices", &Rows ) ) { Fail( 400, "DeviceListMissing" ); return; }
    if( !json_object_is_type( Rows, json_type_array ) ) { Fail( 400, "Invalid device list" ); return; }
    json_object * Transfer = json_object_new_object();
    struct TEditorRelease { json_object * P; ~TEditorRelease(){ json_object_put( P ); } } TransferRelease{ Transfer };
    for( size_t I = 0; I < json_object_array_length( Rows ); ++I )
     {
      auto Row = json_object_array_get_idx( Rows, I );
      json_object * Service = nullptr, * Port = nullptr;
      json_object_object_get_ex( Row, "ServiceId", &Service ); json_object_object_get_ex( Row, "HttpPort", &Port );
      if( !Flag( Row, "Active" ) || json_object_get_int( Service ) <= 2 ) continue;
      const auto Id = Text( Row, "MAC" ) + "@" + std::to_string( json_object_get_int( Service ) );
      const auto FunctionId = Text( Row, "Function" );
      auto Old = FRegistry.Devices().find( FRegistry.Function( Id ) );
      const auto Hostname = Flag( Row, "UseHardwareName" ) ? Text( Row, "HardwareName" ) : FunctionId;
      const bool Changed = Old == FRegistry.Devices().end() || Old->second.IP != Text( Row, "IP" ) ||
        Old->second.Port != unsigned( json_object_get_int( Port ) ) || FRegistry.HostnameFor( Id, FunctionId ) != Hostname;
      if( !Changed ) continue;
      TDeviceRegistry::TDevice Candidate{ FunctionId, Id, Text( Row, "IP" ), Text( Row, "DeviceType" ), Text( Row, "HardwareName" ), unsigned( json_object_get_int( Port ) ) };
      json_object * Info = GetDevice( Candidate, "/api/v1/device" ), * Identity = nullptr, * Version = nullptr, * Boot = nullptr;
      const bool Valid = Info && Text( Info, "DeviceType" ) == Candidate.DeviceType &&
        json_object_object_get_ex( Info, "Identity", &Identity ) && Text( Identity, "DeviceId" ) == Id &&
        json_object_object_get_ex( Identity, "Version", &Version ) && json_object_get_int( Version ) >= 3 &&
        json_object_object_get_ex( Identity, "BootCounter", &Boot ) && json_object_get_int64( Boot ) > 0;
      if( !Valid ) { if( Info ) json_object_put( Info ); Fail( 409, "New or changed device is offline or has wrong identity/type" ); return; }
      auto Config = GetDevice( Candidate, "/Config.json" );
      if( !Config ) { json_object_put( Info ); Fail( 409, "Cannot read new device configuration" ); return; }
      if( Text( Config, "Hostname" ) != Hostname || Text( Identity, "Function" ) != FunctionId )
       {
        auto Entry = json_object_new_object(), Fields = json_object_new_object();
        #define GN_FUNCTION_CONFIG_FIELD( Name ) \
          { json_object * Value = nullptr; \
            if( json_object_object_get_ex( Config, #Name, &Value ) && json_object_is_type( Value, json_type_string ) ) json_object_object_add( Fields, #Name, json_object_get( Value ) ); }
        #include <GnFunctionConfig.inc.h>
        if( json_object_object_length( Fields ) != 4 )
         { json_object_put( Entry ); json_object_put( Fields ); json_object_put( Config ); json_object_put( Info ); Fail( 409, "Invalid new device configuration" ); return; }
        json_object_object_add( Fields, "Hostname", json_object_new_string( Hostname.c_str() ) );
        json_object_object_add( Entry, "FunctionConfig", Fields ); json_object_object_add( Entry, "BootCounter", json_object_get( Boot ) );
        json_object_object_add( Transfer, Id.c_str(), Entry );
       }
      json_object_put( Config ); json_object_put( Info );
     }
    TGNString Error;
    if( !FRegistry.EditDevices( Rows, Transfer, Error ) ) { Fail( 409, Error.c_str() ); return; }
    if( !FRegistry.Pending() ) { Code = 200; Response = "{\"Ok\":true,\"Pending\":false}"; return; }
    if( !FTimes.PrepareAssignment() ) { Fail( 500, "JournalCommittedDistributionPending" ); return; }
    Code = 202; Response = "{\"Ok\":true,\"Pending\":true}"; return;
   }
  if( Operation == "SwapEndpoints" )
   {
    if( !Flag( Request, "ConsumersDetached" ) ) { Fail( 400, "DisconnectBothConsumersFirst" ); return; }
    auto View = Inspect(); const bool Ready = Flag( View, "AllApplied" ) && !FRegistry.Pending(); json_object_put( View );
    if( !Ready ) { Fail( 409, "ParticipantsNotReady" ); return; }
    try
     {
      auto Endpoints = EndpointView( FRegistry ); TRelease EndpointRelease{ Endpoints };
      for( auto Key : { "First", "Second" } )
       {
        json_object * Selected = nullptr;
        for( size_t I = 0; I < json_object_array_length( Endpoints ); ++I )
         { auto R = json_object_array_get_idx( Endpoints, I ); if( Text( R, "EndpointId" ) == Text( Request, Key ) ) Selected = R; }
        if( !Selected ) { Fail( 400, "UnknownEndpoint" ); return; }
        auto It = FRegistry.Devices().find( Text( Selected, "Device" ) );
        if( It->second.Id.back() == '3' )
         {
          auto Status = GetDevice( It->second, "/api/v1/status" );
          const int Capability = SensorCapability( Status ); if( Status ) json_object_put( Status );
          if( Capability != 2 ) { Fail( 409, "TemperatureSensorDisabledOrUnknown" ); return; }
         }
        auto Info = GetDevice( It->second, "/api/v1/device" ); TRelease InfoRelease{ Info };
        json_object * Version = nullptr, * Capability = nullptr;
        json_object_object_get_ex( Selected, "Capabilities", &Capability );
        if( !Info || ( json_object_get_int( Capability ) == 1 &&
            ( !json_object_object_get_ex( Info, "EndpointAssignments", &Version ) || json_object_get_int( Version ) != 1 ) ) )
         { Fail( 409, "EndpointFirmwareUpdateRequired" ); return; }
       }
      TGNString Error;
      if( !SwapEndpoints( FRegistry, Text( Request, "First" ), Text( Request, "Second" ), Error ) ) { Fail( 409, Error.c_str() ); return; }
      if( !FTimes.PrepareAssignment() ) { Fail( 500, "JournalCommittedDistributionPending" ); return; }
      Code = 202; Response = "{\"Ok\":true,\"Pending\":true}"; return;
     }
    catch( std::exception const & Error ) { Fail( 409, Error.what() ); return; }
   }
  if( Operation != "Swap" ) { Fail( 400, "InvalidOperation" ); return; }
  json_object * View = Inspect();
  const bool Ready = Flag( View, "AllReady" ) && !FRegistry.Pending() &&
    Flag( View, "AllApplied" );
  json_object_put( View );
  if( !Ready ) { Fail( 409, "ParticipantsNotReady" ); return; }
  const TGNString First = Text( Request, "First" ), Second = Text( Request, "Second" );
  if( Operation == "Swap" )
   {
    if( !Flag( Request, "ConsumersDetached" ) ) { Fail( 400, "DisconnectBothConsumersFirst" ); return; }
    auto A = FRegistry.Devices().find( First ), B = FRegistry.Devices().find( Second );
    if( A == FRegistry.Devices().end() || B == FRegistry.Devices().end() || A == B ||
        ( A->second.Id.back() != '3' && A->second.Id.back() != '4' ) || A->second.Id.back() != B->second.Id.back() )
      { Fail( 400, "IncompatibleServices" ); return; }
    if( A->second.Id.back() == '3' )
     {
      int PreviousCapability = -1;
      for( auto const * Device : { &A->second, &B->second } )
       {
        json_object * Status = GetDevice( *Device, "/api/v1/status" ), * Channels = nullptr, * State = nullptr;
        const bool Idle = Status && json_object_object_get_ex( Status, "Channels", &Channels ) &&
          json_object_array_length( Channels ) == 1 &&
          json_object_object_get_ex( json_object_array_get_idx( Channels, 0 ), "State", &State ) &&
          json_object_is_type( State, json_type_boolean ) && !json_object_get_boolean( State );
        const int Capability = SensorCapability( Status );
        if( Status ) json_object_put( Status );
        if( !Idle ) { Fail( 409, "FeederBusyOrUnknown" ); return; }
        if( Capability < 0 ) { Fail( 409, "FeederCapabilitiesUnknown" ); return; }
        if( PreviousCapability >= 0 && Capability != PreviousCapability )
         { Fail( 409, "FeederCapabilitiesMismatch" ); return; }
        PreviousCapability = Capability;
       }
     }
   }
  // Read the existing device Config.json path, keeping passwords out of status/logs.
  json_object * Transfer = json_object_new_object();
  struct TTransferRelease { json_object * P; ~TTransferRelease(){ json_object_put( P ); } } TransferRelease{ Transfer };
  for( auto const & Pair : { std::make_pair( First, Second ), std::make_pair( Second, First ) } )
   {
    auto const & Source = FRegistry.Devices().at( Pair.first );
    auto const & Target = FRegistry.Devices().at( Pair.second );
    json_object * Config = GetDevice( Source, "/Config.json" ), * Info = GetDevice( Target, "/api/v1/device" );
    json_object * Identity = nullptr, * Boot = nullptr;
    bool Valid = Config && Info && json_object_object_get_ex( Info, "Identity", &Identity ) &&
      Text( Identity, "DeviceId" ) == Target.Id && json_object_object_get_ex( Identity, "BootCounter", &Boot ) &&
      json_object_is_type( Boot, json_type_int ) && json_object_get_int64( Boot ) > 0;
    json_object * Fields = json_object_new_object();
    #define GN_FUNCTION_CONFIG_FIELD( Name ) \
      { json_object * Value = nullptr; \
        if( Config && json_object_object_get_ex( Config, #Name, &Value ) && json_object_is_type( Value, json_type_string ) ) \
          json_object_object_add( Fields, #Name, json_object_get( Value ) ); \
        else Valid = false; }
    #include <GnFunctionConfig.inc.h>
    // Concrete functions name the network host; reserve slots use target hardware.
    const auto Hostname = FRegistry.HostnameFor( Target.Id, Source.Function );
    if( Hostname.empty() ) Valid = false;
    else json_object_object_add( Fields, "Hostname", json_object_new_string( Hostname.c_str() ) );
    if( Valid )
     {
      json_object * Row = json_object_new_object();
      json_object_object_add( Row, "FunctionConfig", Fields );
      json_object_object_add( Row, "BootCounter", json_object_get( Boot ) );
      json_object_object_add( Transfer, Target.Id.c_str(), Row );
     }
    else json_object_put( Fields );
    if( Config ) json_object_put( Config );
    if( Info ) json_object_put( Info );
    if( !Valid ) { Fail( 409, "FunctionConfigOrBootStateUnavailable" ); return; }
   }
  TGNString Error;
  if( !FRegistry.Change( First, Second, Transfer, Error ) )
   { Fail( 409, Error.c_str() ); return; }
  if( !FTimes.PrepareAssignment() ) { Fail( 500, "JournalCommittedDistributionPending" ); return; }
  Code = 202; Response = "{\"Ok\":true,\"Pending\":true}";
 }

void TAssignmentControl::Tick(void)
 {
  if( !FRegistry.Pending() || std::chrono::steady_clock::now() < FNextInspect ) return;
  FNextInspect = std::chrono::steady_clock::now() + std::chrono::seconds( 5 );
  json_object_put( Inspect() );
 }
