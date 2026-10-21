// HeatProfileStore.cpp, Version: 1.11
#include "HeatProfileStore.h"

#include <GnUdpStatus.h>
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
constexpr char EmptySync[] = "{\n  \"Version\": 1,\n  \"Revision\": \"\",\n  \"Sets\": []\n}\n";

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

bool RequiredString( json_object * Row, char const * Name )
 {
  json_object * Value = nullptr;
  if( !json_object_object_get_ex( Row, Name, &Value ) || !json_object_is_type( Value, json_type_string ) )
    return false;
  const char * Text = json_object_get_string( Value );
  return Text != nullptr && *Text != '\0';
 }

bool RequiredNumber( json_object * Row, char const * Name, double & Result )
 {
  json_object * Value = nullptr;
  if( !json_object_object_get_ex( Row, Name, &Value ) ||
      ( !json_object_is_type( Value, json_type_double ) && !json_object_is_type( Value, json_type_int ) ) )
    return false;
  Result = json_object_get_double( Value );
  return std::isfinite( Result );
 }

bool ValidUtcTimeStamp( TGNString const & Value )
 {
  if( Value.size() != 24 || Value[ 4 ] != '-' || Value[ 7 ] != '-' || Value[ 10 ] != 'T' ||
      Value[ 13 ] != ':' || Value[ 16 ] != ':' || Value[ 19 ] != '.' || Value[ 23 ] != 'Z' ) return false;
  for( size_t Index : { 0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u, 11u, 12u, 14u, 15u, 17u, 18u, 20u, 21u, 22u } )
    if( Value[ Index ] < '0' || Value[ Index ] > '9' ) return false;
  std::tm Utc{};
  char const * End = strptime( Value.c_str(), "%Y-%m-%dT%H:%M:%S", &Utc );
  if( End == nullptr || End != Value.c_str() + 19 ) return false;
  const std::tm Parsed = Utc;
  Utc.tm_isdst = 0;
  const std::time_t Epoch = timegm( &Utc );
  std::tm Checked{};
  if( gmtime_r( &Epoch, &Checked ) == nullptr ) return false;
  return Checked.tm_year == Parsed.tm_year && Checked.tm_mon == Parsed.tm_mon && Checked.tm_mday == Parsed.tm_mday &&
    Checked.tm_hour == Parsed.tm_hour && Checked.tm_min == Parsed.tm_min && Checked.tm_sec == Parsed.tm_sec;
 }

bool ContainsProfileId( json_object * Profiles, TGNString const & Id )
 {
  for( size_t Index = 0; Index < json_object_array_length( Profiles ); ++Index )
    if( StringValue( json_object_array_get_idx( Profiles, Index ), "Id" ) == Id ) return true;
  return false;
 }

bool RemovedProfileIsReferenced( json_object * ExistingProfiles, json_object * IncomingProfiles,
                                 std::filesystem::path const & TimesFile )
 {
  bool HasRemovedProfile = false;
  for( size_t Index = 0; Index < json_object_array_length( ExistingProfiles ); ++Index )
   {
    const TGNString Id = StringValue( json_object_array_get_idx( ExistingProfiles, Index ), "Id" );
    if( !ContainsProfileId( IncomingProfiles, Id ) )
     {
      HasRemovedProfile = true;
      break;
     }
   }
  if( !HasRemovedProfile ) return false;
  json_object * Times = json_object_from_file( TimesFile.c_str() );
  json_object * Channels = nullptr;
  if( Times == nullptr || !json_object_object_get_ex( Times, "Channels", &Channels ) ||
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
      const TGNString Id = StringValue( Row, "Profile" );
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
  json_object * Profiles = nullptr;
  if( Root == nullptr || !json_object_is_type( Root, json_type_object ) ||
      !json_object_object_get_ex( Root, "Profiles", &Profiles ) ||
      !json_object_is_type( Profiles, json_type_array ) ) return false;
  const int RootKeys = json_object_object_length( Root );
  json_object * TimeStamp = nullptr;
  if( RootKeys > 1 && ( RootKeys != 2 ||
      !json_object_object_get_ex( Root, "TimeStamp", &TimeStamp ) ||
      !json_object_is_type( TimeStamp, json_type_string ) ||
      !ValidUtcTimeStamp( StringValue( Root, "TimeStamp" ) ) ) )
    return false;
  std::vector<TGNString> Ids;
  for( size_t Index = 0; Index < json_object_array_length( Profiles ); ++Index )
   {
    json_object * Row = json_object_array_get_idx( Profiles, Index );
    json_object * Measurement = nullptr;
    const bool HasMeasurement = json_object_is_type( Row, json_type_object ) &&
      json_object_object_get_ex( Row, "Measurement", &Measurement );
    if( ( HasMeasurement && ( !json_object_is_type( Measurement, json_type_string ) ||
          !GnUdpMeasurementFieldValid( json_object_get_string( Measurement ) ) ) ) ||
        !json_object_is_type( Row, json_type_object ) || json_object_object_length( Row ) != ( HasMeasurement ? 8 : 7 ) ||
        !RequiredString( Row, "Id" ) || !RequiredString( Row, "TempId" ) ) return false;
    double FloorOn = 0, FloorOff = 0, MainOn = 0, MainOff = 0;
    json_object * NoTempOn = nullptr;
    if( !RequiredNumber( Row, "FloorOn", FloorOn ) || !RequiredNumber( Row, "FloorOff", FloorOff ) ||
        !RequiredNumber( Row, "MainOn", MainOn ) || !RequiredNumber( Row, "MainOff", MainOff ) ||
        FloorOn >= FloorOff || MainOn >= MainOff ||
        !json_object_object_get_ex( Row, "NoTempOn", &NoTempOn ) ||
        !json_object_is_type( NoTempOn, json_type_boolean ) ) return false;
    const TGNString Id = StringValue( Row, "Id" );
    if( std::find( Ids.begin(), Ids.end(), Id ) != Ids.end() ) return false;
    Ids.push_back( Id );
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
  if( !ValidateProfiles( Root ) || !json_object_object_get_ex( Root, "Profiles", &Profiles ) ) return {};
  std::vector<json_object *> Sorted;
  for( size_t Index = 0; Index < json_object_array_length( Profiles ); ++Index )
    Sorted.push_back( json_object_array_get_idx( Profiles, Index ) );
  std::sort( Sorted.begin(), Sorted.end(), []( json_object * Left, json_object * Right )
   { return StringValue( Left, "Id" ) < StringValue( Right, "Id" ); } );

  json_object * Canonical = json_object_new_object();
  json_object * CanonicalProfiles = json_object_new_array();
  for( json_object * Profile : Sorted )
   {
    json_object * Entry = json_object_new_object();
    const char * Keys[] = { "Id", "TempId", "FloorOn", "FloorOff", "MainOn", "MainOff", "NoTempOn" };
    for( char const * Key : Keys )
     {
      json_object * Value = nullptr;
      json_object_object_get_ex( Profile, Key, &Value );
      json_object_object_add( Entry, Key, CloneJson( Value ) );
     }
    json_object * Measurement = nullptr;
    json_object_object_get_ex( Profile, "Measurement", &Measurement );
    json_object_object_add( Entry, "Measurement", Measurement ? CloneJson( Measurement ) : json_object_new_string( "Temperature" ) );
    json_object_array_add( CanonicalProfiles, Entry );
   }
  json_object_object_add( Canonical, "Profiles", CanonicalProfiles );
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
    if( StringValue( Set, "Device" ) == Device ) return Set;
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
  SetString( Set, "Device", Device );
  SetString( Set, "Format", Format );
  SetString( Set, "Revision", "" );
  json_object_object_add( Set, "Synced", json_object_new_boolean( false ) );
  json_object_array_add( Sets, Set );
  return Set;
 }

void SetAllPending( json_object * Sync, TGNString const & Revision )
 {
  SetString( Sync, "Revision", Revision );
  json_object * Sets = nullptr;
  json_object_object_get_ex( Sync, "Sets", &Sets );
  for( size_t Index = 0; Index < json_object_array_length( Sets ); ++Index )
   {
    json_object * Set = json_object_array_get_idx( Sets, Index );
    SetString( Set, "Revision", Revision );
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
    if( !FileRevision.empty() && StringValue( Root, "Revision" ) != FileRevision )
     {
      SetString( Root, "Revision", FileRevision );
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
  SetString( Root, "Revision", Revision );
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
    json_object_object_get_ex( Times, "Channels", &ChannelMaps );
   }
  for( size_t Index = 0; Index < json_object_array_length( TimeSets ); ++Index )
   {
    json_object * TimeSet = json_object_array_get_idx( TimeSets, Index );
    const TGNString Device = StringValue( TimeSet, "Device" );
    const TGNString Type = StringValue( TimeSet, "Type" );
    const TGNString Format = StringValue( TimeSet, "Format" );
    json_object * ChannelRows = nullptr;
    if( Type != "SwitchTimes" || ( Format != "TmrSw" && Format != "MyTimer" ) ||
        Device.empty() || Endpoints.find( Device ) == Endpoints.end() || ChannelMaps == nullptr ||
        !json_object_object_get_ex( ChannelMaps, Device.c_str(), &ChannelRows ) )
      continue;
    json_object * Set = EnsureSet( Root, Device, Format );
    SetString( Set, "Revision", Revision );
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
      Response = "{\"Ok\":false,\"Error\":\"Invalid central HeatProfiles.json\"}";
      return true;
    }
    json_object_object_add( Profiles, "Revision", json_object_new_string( StringValue( Sync, "Revision" ).c_str() ) );
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
      SetString( Participant, "Device", StringValue( TimeSet, "Device" ) );
      SetString( Participant, "Format", Format );
      SetString( Participant, "Revision", StringValue( TimeSet, "TimeStamp" ) );
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
    Response = "{\"Ok\":false,\"Error\":\"Method not allowed\"}";
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
    Response = "{\"Ok\":false,\"Error\":\"Invalid heat profiles\"}";
    return true;
   }
  json_object * ExistingRows = nullptr;
  json_object * IncomingRows = nullptr;
  json_object_object_get_ex( Existing, "Profiles", &ExistingRows );
  json_object_object_get_ex( Incoming, "Profiles", &IncomingRows );
  if( RemovedProfileIsReferenced( ExistingRows, IncomingRows, RootDirectory / "data" / "Times.json" ) )
   {
    json_object_put( Incoming ); json_object_put( Existing ); json_object_put( Sync );
    StatusCode = 409;
    Response = "{\"Ok\":false,\"Error\":\"A profile still referenced by a channel cannot be deleted\"}";
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
      Response = "{\"Ok\":false,\"Error\":\"Cannot save HeatProfiles.json\"}";
      return true;
     }
    SetAllPending( Sync, Revision );
    if( !SaveSync( Sync ) )
     {
      json_object_put( Incoming ); json_object_put( Existing ); json_object_put( Sync );
      StatusCode = 500;
      Response = "{\"Ok\":false,\"Error\":\"Cannot save heat profile sync state\"}";
      return true;
     }
   }
  json_object_put( Incoming ); json_object_put( Existing ); json_object_put( Sync );
  StatusCode = 200;
  Response = Changed ? "{\"Ok\":true,\"Changed\":true}" : "{\"Ok\":true,\"Changed\":false}";
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
  const TGNString Device = StringValue( Set, "Device" );
  auto Endpoint = Endpoints.find( Device );
  if( Endpoint == Endpoints.end() ) return false;
  json_object * Payload = json_object_new_object();
  SetString( Payload, "Device", Registry.WireId( Device ) );
  Registry.AddPreparation( Device, Payload );
  SetString( Payload, "Type", "SwitchTimes" );
  SetString( Payload, "Format", StringValue( Set, "Format" ) );
  SetString( Payload, "TimeStamp", StringValue( Sync, "Revision" ) );
  json_object * Times = json_object_from_file( ( RootDirectory / "data" / "Times.json" ).c_str() );
  json_object * DeviceChannels = nullptr;
  json_object * Channels = nullptr;
  if( Times != nullptr ) json_object_object_get_ex( Times, "Channels", &Channels );
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
  json_object_object_get_ex( Profiles, "Profiles", &ProfileRows );
  json_object_object_add( ChannelConfig, "Profiles", CloneJson( ProfileRows ) );
  json_object_object_add( ChannelConfig, "Channels", CloneJson( ChannelRows ) );
  json_object_object_add( Content, "Channels", ChannelConfig );
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
  SetString( Set, "Revision", StringValue( Sync, "Revision" ) );
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
