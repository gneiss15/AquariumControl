// HeatProfileStore.cpp, Version: 1.20
#define Do_THttpResult
#define Do_TJsonHeatConfig
#define Do_TJsonProfile
#define Do_TJsonProfiles
#define Do_TUdpLastError
#include "HeatProfileStore.h"
#include <GnJsonResponses.h>

#include <GnUdpStatus.h>
#include <GnHeaterControl.h>
#include <GnProfileModel.h>
#include <GnChargeProfileJson.h>
#include <GnHeatPiProfileJson.h>
#include <arpa/inet.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <initializer_list>
#include <json-c/json.h>
#include <netdb.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace
 {
constexpr char EmptySync[] = "{\n  \"Ver\": 1,\n  \"Rev\": \"\",\n  \"Sets\": []\n}\n";

TGNString JsonText( json_object * Object, bool Pretty = true )
 {
  if( Object == nullptr ) return {};
  const char * Text = json_object_to_json_string_ext( Object,
    Pretty ? JSON_C_TO_STRING_PRETTY : JSON_C_TO_STRING_PLAIN );
  return Text != nullptr ? TGNString( Text ) + ( Pretty ? "\n" : "" ) : TGNString();
 }

TGNString StringValue( json_object * Object, char const * Name )
 {
  json_object * Value = nullptr;
  if( Object == nullptr || !json_object_object_get_ex( Object, Name, &Value ) ||
      !json_object_is_type( Value, json_type_string ) ) return {};
  const char * Text = json_object_get_string( Value );
  return Text != nullptr ? TGNString( Text ) : TGNString();
 }

bool BoolValue( json_object * Object, char const * Name, bool DefaultValue = false )
 {
  json_object * Value = nullptr;
  if( Object == nullptr || !json_object_object_get_ex( Object, Name, &Value ) ) return DefaultValue;
  return json_object_get_boolean( Value );
 }

bool SetString( json_object * Object, char const * Name, TGNString const & Value )
 {
  return json_object_object_add( Object, Name, json_object_new_string( Value.c_str() ) ) == 0;
 }

TGNString NowTimeStamp(void)
 {
  using namespace std::chrono;
  const auto Now = system_clock::now();
  const auto Milliseconds = duration_cast<milliseconds>( Now.time_since_epoch() ) % 1000;
  const std::time_t NowTime = system_clock::to_time_t( Now );
  std::tm Utc{};
  gmtime_r( &NowTime, &Utc );
  char Buffer[32];
  std::strftime( Buffer, sizeof( Buffer ), "%Y-%m-%dT%H:%M:%S", &Utc );
  char Fraction[8];
  std::snprintf( Fraction, sizeof( Fraction ), ".%03lldZ",
                 static_cast<long long>( Milliseconds.count() ) );
  return TGNString( Buffer ) + Fraction;
 }

bool ContainsProfileId( json_object * Profiles, TGNString const & Id )
 {
  for( size_t Index = 0; Index < json_object_array_length( Profiles ); ++Index )
    if( StringValue( json_object_array_get_idx( Profiles, Index ), "ProfName" ) == Id ) return true;
  return false;
 }

bool RemovedProfileIsReferenced( json_object * ExistingProfiles, json_object * IncomingProfiles,
                                 std::filesystem::path const & TimesFile )
 {
  bool HasRemovedProfile = false;
  for( size_t Index = 0; Index < json_object_array_length( ExistingProfiles ); ++Index )
   {
    const TGNString Id = StringValue( json_object_array_get_idx( ExistingProfiles, Index ), "ProfName" );
    if( !ContainsProfileId( IncomingProfiles, Id ) )
     {
      HasRemovedProfile = true;
      break;
     }
   }
  if( !HasRemovedProfile ) return false;
  json_object * Times = json_object_from_file( TimesFile.c_str() );
  json_object * Channels = nullptr;
  if( Times == nullptr || !json_object_object_get_ex( Times, "Chs", &Channels ) ||
      !json_object_is_type( Channels, json_type_object ) )
   {
    if( Times != nullptr ) json_object_put( Times );
    return true;
   }
  bool Referenced = false;
  json_object_object_foreach( Channels, Device, State )
   {
    (void)Device;
    json_object * Rows = State;
    if( json_object_is_type( State, json_type_object ) )
      json_object_object_get_ex( State, "Rows", &Rows );
    if( !json_object_is_type( Rows, json_type_array ) ) continue;
    for( size_t RowIndex = 0; RowIndex < json_object_array_length( Rows ); ++RowIndex )
     {
      json_object * Row = json_object_array_get_idx( Rows, RowIndex );
      const TGNString Id = StringValue( Row, "Prof" );
      if( !Id.empty() && ContainsProfileId( ExistingProfiles, Id ) && !ContainsProfileId( IncomingProfiles, Id ) )
       {
        Referenced = true;
        break;
       }
     }
    if( Referenced ) break;
   }
  json_object_put( Times );
  return Referenced;
 }

bool ValidateProfiles( json_object * Root )
 {
  TJsonProfiles Contract;
  TGNString Error;
  if( !GnRead( TGnWireValue( Root ), Contract, Error ) )return false;
  std::vector<TGNString> Ids;
  if(!Contract.Profs.Known())return false;
  for( auto const & Profile : Contract.Profs )
   {
    THeaterProfile Values;
    if(!GnApplyHeatProfile(Profile,Values))return false;
    if( std::find( Ids.begin(), Ids.end(), Profile.ProfName ) != Ids.end() )return false;
    Ids.push_back( Profile.ProfName );
   }
  return true;
 }

json_object * CloneJson( json_object * Value )
 {
  if( Value == nullptr ) return nullptr;
  return json_tokener_parse( json_object_to_json_string_ext( Value, JSON_C_TO_STRING_PLAIN ) );
 }

TGNString ProfileSignature( json_object * Root )
 {
  json_object * Profiles = nullptr;
  if( !ValidateProfiles( Root ) || !json_object_object_get_ex( Root, "Profs", &Profiles ) ) return {};
  std::vector<json_object *> Sorted;
  for( size_t Index = 0; Index < json_object_array_length( Profiles ); ++Index )
    Sorted.push_back( json_object_array_get_idx( Profiles, Index ) );
  std::sort( Sorted.begin(), Sorted.end(), []( json_object * Left, json_object * Right )
   { return StringValue( Left, "ProfName" ) < StringValue( Right, "ProfName" ); } );

  json_object * Canonical = json_object_new_object();
  json_object * CanonicalProfiles = json_object_new_array();
  for( json_object * Profile : Sorted )
   {
    json_object * Entry = CloneJson( Profile );
    json_object * Measurement = nullptr;
    json_object_object_get_ex( Profile, "Meas", &Measurement );
    json_object_object_add( Entry, "Meas", Measurement ? CloneJson( Measurement ) : json_object_new_string( "Temperature" ) );
    json_object_array_add( CanonicalProfiles, Entry );
   }
  json_object_object_add( Canonical, "Profs", CanonicalProfiles );
  const TGNString Result = JsonText( Canonical, false );
  json_object_put( Canonical );
  return Result;
 }

json_object * FindSet( json_object * Sync, TGNString const & Device )
 {
  json_object * Sets = nullptr;
  if( Sync == nullptr || !json_object_object_get_ex( Sync, "Sets", &Sets ) ) return nullptr;
  for( size_t Index = 0; Index < json_object_array_length( Sets ); ++Index )
   {
    json_object * Set = json_object_array_get_idx( Sets, Index );
    if( StringValue( Set, "Dev" ) == Device ) return Set;
   }
  return nullptr;
 }

json_object * EnsureSet( json_object * Sync, TGNString const & Device, TGNString const & Format )
 {
  if( json_object * Existing = FindSet( Sync, Device ) )
   {
    SetString( Existing, "Format", Format );
    return Existing;
   }
  json_object * Sets = nullptr;
  json_object_object_get_ex( Sync, "Sets", &Sets );
  json_object * Set = json_object_new_object();
  SetString( Set, "Dev", Device );
  SetString( Set, "Format", Format );
  SetString( Set, "Rev", "" );
  json_object_object_add( Set, "Synced", json_object_new_boolean( false ) );
  json_object_array_add( Sets, Set );
  return Set;
 }

void SetAllPending( json_object * Sync, TGNString const & Revision )
 {
  SetString( Sync, "Rev", Revision );
  json_object * Sets = nullptr;
  json_object_object_get_ex( Sync, "Sets", &Sets );
  for( size_t Index = 0; Index < json_object_array_length( Sets ); ++Index )
   {
    json_object * Set = json_object_array_get_idx( Sets, Index );
    SetString( Set, "Rev", Revision );
    json_object_object_add( Set, "Synced", json_object_new_boolean( false ) );
   }
 }

bool ParseHttpUrl( TGNString Url, TGNString & Host, unsigned & Port )
 {
  constexpr char Prefix[] = "http://";
  if( Url.rfind( Prefix, 0 ) != 0 ) return false;
  Url.erase( 0, sizeof( Prefix ) - 1 );
  const size_t Slash = Url.find( '/' );
  if( Slash != TGNString::npos ) Url.erase( Slash );
  const size_t Colon = Url.rfind( ':' );
  Port = 80;
  if( Colon != TGNString::npos )
   {
    try { Port = static_cast<unsigned>( std::stoul( Url.substr( Colon + 1 ) ) ); }
    catch( ... ) { return false; }
    Url.erase( Colon );
   }
  Host = Url;
  return !Host.empty() && Port > 0 && Port <= 65535;
 }

bool SaveJson( std::filesystem::path const & FileName, json_object * Root )
 {
  std::error_code Error;
  std::filesystem::create_directories( FileName.parent_path(), Error );
  if( Error ) return false;
  std::filesystem::path Temp = FileName;
  Temp += ".tmp";
  {
   std::ofstream File( Temp, std::ios::binary | std::ios::trunc );
   if( !File ) return false;
   const TGNString Text = JsonText( Root );
   File.write( Text.data(), static_cast<std::streamsize>( Text.size() ) );
   File.flush();
   if( !File ) return false;
  }
  std::filesystem::rename( Temp, FileName, Error );
  if( Error ) { std::filesystem::remove( Temp, Error ); return false; }
  return true;
 }
 } // namespace

THeatProfileStore::THeatProfileStore( std::filesystem::path RootDirectoryValue, TDeviceRegistry & RegistryValue )
: Registry( RegistryValue )
, RootDirectory( std::filesystem::weakly_canonical( std::move( RootDirectoryValue ) ) )
, ProfilesFile( RootDirectory / "data" / "HeatProfiles.json" )
, SyncFile( RootDirectory / "data" / "HeatProfiles.sync.json" )
 {
  LoadEndpoints();
  NextRetry = std::chrono::steady_clock::now() + std::chrono::seconds( 5 );
 }

json_object * THeatProfileStore::LoadProfiles(void) const
 {
  json_object * Root = json_object_from_file( ProfilesFile.c_str() );
  if( !ValidateProfiles( Root ) )
   {
    if( Root != nullptr ) json_object_put( Root );
    return nullptr;
   }
  return Root;
 }

json_object * THeatProfileStore::LoadSync(void) const
 {
  json_object * Root = json_object_from_file( SyncFile.c_str() );
  json_object * Sets = nullptr;
  if( Root != nullptr && json_object_object_get_ex( Root, "Sets", &Sets ) &&
      json_object_is_type( Sets, json_type_array ) && json_object_array_length( Sets ) > 0 )
   {
    json_object * Profiles = json_object_from_file( ProfilesFile.c_str() );
    const TGNString FileRevision = StringValue( Profiles, "TimeStamp" );
    if( !FileRevision.empty() && StringValue( Root, "Rev" ) != FileRevision )
     {
      SetString( Root, "Rev", FileRevision );
      SaveSync( Root );
     }
    if( Profiles != nullptr ) json_object_put( Profiles );
    return Root;
   }
  if( Root == nullptr ) Root = json_tokener_parse( EmptySync );
  json_object * Profiles = json_object_from_file( ProfilesFile.c_str() );
  const TGNString Revision = StringValue( Profiles, "TimeStamp" ).empty()
    ? NowTimeStamp() : StringValue( Profiles, "TimeStamp" );
  if( Profiles != nullptr ) json_object_put( Profiles );
  SetString( Root, "Rev", Revision );
  json_object_object_get_ex( Root, "Sets", &Sets );
  if( Sets == nullptr )
   {
    Sets = json_object_new_array();
    json_object_object_add( Root, "Sets", Sets );
   }
  json_object * Times = json_object_from_file( ( RootDirectory / "data" / "Times.json" ).c_str() );
  json_object * TimeSets = nullptr;
  json_object * ChannelMaps = nullptr;
  if( Times != nullptr )
   {
    json_object_object_get_ex( Times, "Sets", &TimeSets );
    json_object_object_get_ex( Times, "Chs", &ChannelMaps );
   }
  for( size_t Index = 0; Index < json_object_array_length( TimeSets ); ++Index )
   {
    json_object * TimeSet = json_object_array_get_idx( TimeSets, Index );
    const TGNString Device = StringValue( TimeSet, "Dev" );
    const TGNString Type = StringValue( TimeSet, "Type" );
    const TGNString Format = StringValue( TimeSet, "Format" );
    json_object * ChannelRows = nullptr;
    if( Type != "SwitchTimes" || ( Format != "TmrSw" && Format != "MyTimer" ) ||
        Device.empty() || Endpoints.find( Device ) == Endpoints.end() || ChannelMaps == nullptr ||
        !json_object_object_get_ex( ChannelMaps, Device.c_str(), &ChannelRows ) )
      continue;
    json_object * Set = EnsureSet( Root, Device, Format );
    SetString( Set, "Rev", Revision );
    json_object_object_add( Set, "Synced", json_object_new_boolean( false ) );
   }
  if( Times != nullptr ) json_object_put( Times );
  SaveSync( Root );
  return Root;
 }

bool THeatProfileStore::SaveProfiles( json_object * Profiles ) const
 {
  return ValidateProfiles( Profiles ) && SaveJson( ProfilesFile, Profiles );
 }

bool THeatProfileStore::SaveSync( json_object * Sync ) const
 {
  return SaveJson( SyncFile, Sync );
 }

void THeatProfileStore::LoadEndpoints(void)
 {
  Endpoints.clear();
  json_object * Root = json_object_from_file( ( RootDirectory / "Devices.json" ).c_str() );
  if( Root == nullptr )
   {
    return;
   }
  Registry.ApplyAddresses( Root );
  const char * Lists[] = { "Devices", "SyncOnlyDevices" };
  for( char const * List : Lists )
   {
    json_object * Devices = nullptr;
    if( !json_object_object_get_ex( Root, List, &Devices ) || !json_object_is_type( Devices, json_type_array ) )
      continue;
    for( size_t Index = 0; Index < json_object_array_length( Devices ); ++Index )
     {
      json_object * Device = json_object_array_get_idx( Devices, Index );
      TGNString Host;
      unsigned Port = 80;
      if( !ParseHttpUrl( StringValue( Device, "BaseUrl" ), Host, Port ) ) continue;
      const TEndpoint Endpoint{ Host, Port };
      const TGNString Name = StringValue( Device, "Name" );
      if( !Name.empty() ) Endpoints[ Name ] = Endpoint;
      Endpoints[ Host ] = Endpoint;
      if( Host == "gn-pi" ) Endpoints[ "GN-Pi" ] = Endpoint;
     }
   }
  json_object_put( Root );
  for( auto const & Item : Registry.Devices() )
    Endpoints[Item.first] = TEndpoint{ Item.second.IP, Item.second.Port };
 }

bool THeatProfileStore::HandleCentralRequest( TGNString const & Method, TGNString const & Body,
                                              unsigned & StatusCode, TGNString & Response )
 {
  if( Method == "GET" || Method == "HEAD" )
   {
    json_object * Profiles = LoadProfiles();
    json_object * Sync = LoadSync();
    if( Profiles == nullptr || Sync == nullptr )
     {
      if( Profiles != nullptr ) json_object_put( Profiles );
      if( Sync != nullptr ) json_object_put( Sync );
      StatusCode = 500;
      Response = GnJsonResult(false,"Invalid central HeatProfiles.json");
      return true;
    }
    json_object_object_add( Profiles, "Rev", json_object_new_string( StringValue( Sync, "Rev" ).c_str() ) );
    json_object * ParticipantSets = json_object_new_array();
    json_object * Times = json_object_from_file( ( RootDirectory / "data" / "Times.json" ).c_str() );
    json_object * TimeSets = nullptr;
    if( Times != nullptr ) json_object_object_get_ex( Times, "Sets", &TimeSets );
    for( size_t Index = 0; Index < json_object_array_length( TimeSets ); ++Index )
     {
      json_object * TimeSet = json_object_array_get_idx( TimeSets, Index );
      const TGNString Type = StringValue( TimeSet, "Type" );
      const TGNString Format = StringValue( TimeSet, "Format" );
      if( Type != "SwitchTimes" || ( Format != "TmrSw" && Format != "MyTimer" ) ) continue;
      json_object * Participant = json_object_new_object();
      SetString( Participant, "Dev", StringValue( TimeSet, "Dev" ) );
      SetString( Participant, "Format", Format );
      SetString( Participant, "Rev", StringValue( TimeSet, "TimeStamp" ) );
      json_object_object_add( Participant, "Synced", json_object_new_boolean( BoolValue( TimeSet, "Synced", false ) ) );
      json_object_array_add( ParticipantSets, Participant );
     }
    if( Times != nullptr ) json_object_put( Times );
    json_object_object_add( Profiles, "Sets", ParticipantSets );
    Response = JsonText( Profiles );
    json_object_put( Profiles );
    json_object_put( Sync );
    StatusCode = 200;
    return true;
   }
  if( Method != "PUT" )
   {
    StatusCode = 405;
    Response = GnJsonResult(false,"Method not allowed");
    return true;
   }
  json_object * Incoming = json_tokener_parse( Body.c_str() );
  json_object * Existing = LoadProfiles();
  json_object * Sync = LoadSync();
  if( !ValidateProfiles( Incoming ) || Existing == nullptr || Sync == nullptr )
   {
    if( Incoming != nullptr ) json_object_put( Incoming );
    if( Existing != nullptr ) json_object_put( Existing );
    if( Sync != nullptr ) json_object_put( Sync );
    StatusCode = 400;
    Response = GnJsonResult(false,"Invalid heat profiles");
    return true;
   }
  const TGNString Expected = StringValue( Incoming, "TimeStamp" );
  if( !Expected.empty() && Expected != StringValue( Existing, "TimeStamp" ) )
   {
    json_object_put( Incoming ); json_object_put( Existing ); json_object_put( Sync );
    StatusCode = 409;
    Response = GnJsonResult(false,"Profiles changed; reload before saving");
    return true;
   }
  // A form editing Profiles must not discard unrelated root metadata.
  json_object_object_foreach( Existing, Key, Value )
   {
    json_object * Present = nullptr;
    if( TGNString( Key ) != "Profiles" && TGNString( Key ) != "TimeStamp" &&
        !json_object_object_get_ex( Incoming, Key, &Present ) )
      json_object_object_add( Incoming, Key, CloneJson( Value ) );
   }
  json_object * ExistingRows = nullptr;
  json_object * IncomingRows = nullptr;
  json_object_object_get_ex( Existing, "Profs", &ExistingRows );
  json_object_object_get_ex( Incoming, "Profs", &IncomingRows );
  if( RemovedProfileIsReferenced( ExistingRows, IncomingRows, RootDirectory / "data" / "Times.json" ) )
   {
    json_object_put( Incoming ); json_object_put( Existing ); json_object_put( Sync );
    StatusCode = 409;
    Response = GnJsonResult(false,"A profile still referenced by a channel cannot be deleted");
    return true;
   }
  const bool Changed = ProfileSignature( Incoming ) != ProfileSignature( Existing );
  if( Changed )
   {
    const TGNString Revision = NowTimeStamp();
    SetString( Incoming, "TimeStamp", Revision );
    if( !SaveProfiles( Incoming ) )
     {
      json_object_put( Incoming ); json_object_put( Existing ); json_object_put( Sync );
      StatusCode = 500;
      Response = GnJsonResult(false,"Cannot save HeatProfiles.json");
      return true;
     }
    SetAllPending( Sync, Revision );
    if( !SaveSync( Sync ) )
     {
      json_object_put( Incoming ); json_object_put( Existing ); json_object_put( Sync );
      StatusCode = 500;
      Response = GnJsonResult(false,"Cannot save heat profile sync state");
      return true;
     }
   }
  json_object_put( Incoming ); json_object_put( Existing ); json_object_put( Sync );
  StatusCode = 200;
  Response = GnJsonChangedResult(Changed);
  return true;
 }

bool THeatProfileStore::HttpPost( TEndpoint const & Endpoint, TGNString const & Path,
                                  TGNString const & Body, unsigned & StatusCode,
                                  TGNString & Response ) const
 {
  addrinfo Hints{};
  Hints.ai_family = AF_UNSPEC;
  Hints.ai_socktype = SOCK_STREAM;
  addrinfo * Addresses = nullptr;
  const TGNString PortText = std::to_string( Endpoint.Port );
  if( getaddrinfo( Endpoint.Host.c_str(), PortText.c_str(), &Hints, &Addresses ) != 0 ) return false;
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
  Header << "POST " << Path << " HTTP/1.1\r\nHost: " << Endpoint.Host << ':' << Endpoint.Port
         << "\r\nContent-Type: application/json\r\nContent-Length: " << Body.size()
         << "\r\nConnection: close\r\n\r\n";
  const TGNString Request = Header.str() + Body;
  size_t Offset = 0;
  while( Offset < Request.size() )
   {
    const ssize_t Sent = send( Socket, Request.data() + Offset, Request.size() - Offset, MSG_NOSIGNAL );
    if( Sent <= 0 ) { close( Socket ); return false; }
    Offset += static_cast<size_t>( Sent );
   }
  TGNString Raw;
  char Buffer[4096];
  while( true )
   {
    const ssize_t Count = recv( Socket, Buffer, sizeof( Buffer ), 0 );
    if( Count <= 0 ) break;
    Raw.append( Buffer, static_cast<size_t>( Count ) );
   }
  close( Socket );
  const size_t FirstSpace = Raw.find( ' ' );
  if( FirstSpace == TGNString::npos ) return false;
  try { StatusCode = static_cast<unsigned>( std::stoul( Raw.substr( FirstSpace + 1, 3 ) ) ); }
  catch( ... ) { return false; }
  const size_t HeaderEnd = Raw.find( "\r\n\r\n" );
  Response = HeaderEnd == TGNString::npos ? "" : Raw.substr( HeaderEnd + 4 );
  return true;
 }

bool THeatProfileStore::TryPushSet( json_object * Sync, json_object * Set, json_object * Profiles )
 {
  const TGNString Device = StringValue( Set, "Dev" );
  auto Endpoint = Endpoints.find( Device );
  if( Endpoint == Endpoints.end() ) return false;
  json_object * Payload = json_object_new_object();
  SetString( Payload, "Dev", Registry.WireId( Device ) );
  Registry.AddPreparation( Device, Payload );
  SetString( Payload, "Type", "SwitchTimes" );
  SetString( Payload, "Format", StringValue( Set, "Format" ) );
  SetString( Payload, "TimeStamp", StringValue( Sync, "Rev" ) );
  json_object * Times = json_object_from_file( ( RootDirectory / "data" / "Times.json" ).c_str() );
  json_object * DeviceChannels = nullptr;
  json_object * Channels = nullptr;
  if( Times != nullptr ) json_object_object_get_ex( Times, "Chs", &Channels );
  if( Channels == nullptr || !json_object_object_get_ex( Channels, Device.c_str(), &DeviceChannels ) )
   {
    if( Times != nullptr ) json_object_put( Times );
    json_object_put( Payload );
    return false;
   }
  json_object * ChannelRows = DeviceChannels;
  if( json_object_is_type( DeviceChannels, json_type_object ) )
    json_object_object_get_ex( DeviceChannels, "Rows", &ChannelRows );
  if( !json_object_is_type( ChannelRows, json_type_array ) )
   {
    if( Times != nullptr ) json_object_put( Times );
    json_object_put( Payload );
    return false;
   }
  json_object * Content = json_object_new_object();
  json_object * ChannelConfig = json_object_new_object();
  json_object * ProfileRows = nullptr;
  json_object_object_get_ex( Profiles, "Profs", &ProfileRows );
  json_object_object_add( ChannelConfig, "Profs", CloneJson( ProfileRows ) );
  json_object_object_add( ChannelConfig, "Chs", CloneJson( ChannelRows ) );
  json_object_object_add( Content, "Chs", ChannelConfig );
  json_object_object_add( Payload, "Content", Content );
  json_object_put( Times );
  const TGNString Body = JsonText( Payload, false );
  json_object_put( Payload );
  unsigned Status = 0;
  TGNString ReplyText;
  if( !HttpPost( Endpoint->second, "/api/v1/times", Body, Status, ReplyText ) ||
      Status < 200 || Status >= 300 ) return false;
  json_object * Reply = json_tokener_parse( ReplyText.c_str() );
  json_object * Ok = nullptr;
  const bool Accepted = Reply != nullptr && json_object_object_get_ex( Reply, "Ok", &Ok ) &&
                        json_object_get_boolean( Ok );
  if( Reply != nullptr ) json_object_put( Reply );
  if( !Accepted ) return false;
  SetString( Set, "Rev", StringValue( Sync, "Rev" ) );
  json_object_object_add( Set, "Synced", json_object_new_boolean( true ) );
  SaveSync( Sync );
  return true;
 }

bool THeatProfileStore::TryPushUnsynced(void)
 {
  json_object * Profiles = LoadProfiles();
  json_object * Sync = LoadSync();
  json_object * Sets = nullptr;
  if( Profiles == nullptr || Sync == nullptr || !json_object_object_get_ex( Sync, "Sets", &Sets ) )
   {
    if( Profiles != nullptr ) json_object_put( Profiles );
    if( Sync != nullptr ) json_object_put( Sync );
    return false;
   }
  bool AllSynced = true;
  for( size_t Index = 0; Index < json_object_array_length( Sets ); ++Index )
   {
    json_object * Set = json_object_array_get_idx( Sets, Index );
    json_object * Synced = nullptr;
    if( json_object_object_get_ex( Set, "Synced", &Synced ) && json_object_get_boolean( Synced ) ) continue;
    AllSynced = false;
    TryPushSet( Sync, Set, Profiles );
   }
  json_object_put( Profiles );
  json_object_put( Sync );
  return AllSynced;
 }

void THeatProfileStore::Tick(void)
 {
 }
