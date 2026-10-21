// TimeStore.cpp, Version: 1.19
#include "TimeStore.h"

#include <GnUdpStatus.h>
#include <arpa/inet.h>
#include <cmath>
#include <ctime>
#include <fstream>
#include <json-c/json.h>
#include <netdb.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace
 {
constexpr char EmptyStore[] = "{\n  \"Version\": 1,\n  \"Sets\": [],\n  \"Times\": []\n}\n";
constexpr auto RetryInterval = std::chrono::seconds( 30 );

TGNString JsonText( json_object * Object, bool Pretty = true )
 {
  if( Object == nullptr )
    return {};
  const char * Text = json_object_to_json_string_ext( Object,
    Pretty ? JSON_C_TO_STRING_PRETTY : JSON_C_TO_STRING_PLAIN );
  return Text != nullptr ? TGNString( Text ) + ( Pretty ? "\n" : "" ) : TGNString();
 }

TGNString StringValue( json_object * Object, char const * Name )
 {
  json_object * Value = nullptr;
  if( Object == nullptr || !json_object_object_get_ex( Object, Name, &Value ) ||
      !json_object_is_type( Value, json_type_string ) )
    return {};
  const char * Text = json_object_get_string( Value );
  return Text != nullptr ? Text : "";
 }

void SetString( json_object * Object, char const * Name, TGNString const & Value )
 {
  json_object_object_add( Object, Name, json_object_new_string( Value.c_str() ) );
 }

json_object * CloneJson( json_object * Value );

bool BoolValue( json_object * Object, char const * Name, bool DefaultValue = false )
 {
  json_object * Value = nullptr;
  if( Object == nullptr || !json_object_object_get_ex( Object, Name, &Value ) )
    return DefaultValue;
  return json_object_get_boolean( Value );
 }

TGNString NowTimeStamp(void)
 {
  using namespace std::chrono;
  const auto Now = system_clock::now();
  const auto Milliseconds = duration_cast<milliseconds>( Now.time_since_epoch() ) % 1000;
  const std::time_t NowTime = system_clock::to_time_t( Now );
  std::tm Utc{};
  gmtime_r( &NowTime, &Utc );
  char DateTime[ 32 ];
  std::strftime( DateTime, sizeof( DateTime ), "%Y-%m-%dT%H:%M:%S", &Utc );
  char Fraction[ 8 ];
  std::snprintf( Fraction, sizeof( Fraction ), ".%03lldZ",
                 static_cast<long long>( Milliseconds.count() ) );
  return TGNString( DateTime ) + Fraction;
 }

json_object * LoadStore( std::filesystem::path const & FileName )
 {
  if( !std::filesystem::is_regular_file( FileName ) )
    return json_tokener_parse( EmptyStore );
  json_object * Root = json_object_from_file( FileName.c_str() );
  json_object * Sets = nullptr;
  json_object * Times = nullptr;
  if( Root == nullptr || !json_object_is_type( Root, json_type_object ) ||
      !json_object_object_get_ex( Root, "Sets", &Sets ) || !json_object_is_type( Sets, json_type_array ) ||
      !json_object_object_get_ex( Root, "Times", &Times ) || !json_object_is_type( Times, json_type_array ) )
   {
    if( Root != nullptr )
      json_object_put( Root );
    return nullptr;
   }
  json_object * Channels = nullptr;
  if( !json_object_object_get_ex( Root, "Channels", &Channels ) )
    json_object_object_add( Root, "Channels", json_object_new_object() );
  else if( !json_object_is_type( Channels, json_type_object ) )
   {
    json_object_put( Root );
    return nullptr;
   }
  std::vector<std::pair<TGNString,json_object *>> ChannelMigrations;
  json_object_object_foreach( Channels, ChannelDevice, ChannelValue )
   {
    if( json_object_is_type( ChannelValue, json_type_array ) )
      ChannelMigrations.emplace_back( ChannelDevice, CloneJson( ChannelValue ) );
    else if( !json_object_is_type( ChannelValue, json_type_object ) )
     {
      json_object_put( Root );
      return nullptr;
     }
    else
     {
      json_object * Rows = nullptr;
      if( !json_object_object_get_ex( ChannelValue, "Rows", &Rows ) || !json_object_is_type( Rows, json_type_array ) )
       {
        json_object_put( Root );
        return nullptr;
       }
      if( StringValue( ChannelValue, "TimeStamp" ).empty() )
        ChannelMigrations.emplace_back( ChannelDevice, CloneJson( Rows ) );
     }
   }
  if( !ChannelMigrations.empty() )
   {
    const TGNString Revision = NowTimeStamp();
    for( auto const & Migration : ChannelMigrations )
     {
      json_object * ChannelState = json_object_new_object();
      SetString( ChannelState, "TimeStamp", Revision );
      json_object_object_add( ChannelState, "Rows", Migration.second );
      json_object_object_add( Channels, Migration.first.c_str(), ChannelState );
     }
   }
  return Root;
 }

bool SaveStore( std::filesystem::path const & FileName, json_object * Root )
 {
  std::error_code Error;
  std::filesystem::create_directories( FileName.parent_path(), Error );
  if( Error )
    return false;
  std::filesystem::path Temp = FileName;
  Temp += ".tmp";
  {
    std::ofstream File( Temp, std::ios::binary | std::ios::trunc );
    if( !File )
      return false;
    const TGNString Text = JsonText( Root, true );
    File.write( Text.data(), static_cast<std::streamsize>( Text.size() ) );
    File.flush();
    if( !File )
      return false;
  }
  std::filesystem::rename( Temp, FileName, Error );
  if( Error )
   {
    std::filesystem::remove( Temp, Error );
    return false;
   }
  return true;
 }

json_object * FindSet( json_object * Root, TGNString const & Device, TGNString const & Type )
 {
  json_object * Sets = nullptr;
  if( !json_object_object_get_ex( Root, "Sets", &Sets ) )
    return nullptr;
  for( size_t Index = 0; Index < json_object_array_length( Sets ); ++Index )
   {
    json_object * Set = json_object_array_get_idx( Sets, Index );
    if( StringValue( Set, "Device" ) == Device && StringValue( Set, "Type" ) == Type )
      return Set;
   }
  return nullptr;
 }

json_object * EnsureSet( json_object * Root, TGNString const & Device, TGNString const & Type,
                         TGNString const & Format )
 {
  if( json_object * Set = FindSet( Root, Device, Type ) )
   {
    if( !Format.empty() )
      json_object_object_add( Set, "Format", json_object_new_string( Format.c_str() ) );
    return Set;
   }
  json_object * Sets = nullptr;
  json_object_object_get_ex( Root, "Sets", &Sets );
  json_object * Set = json_object_new_object();
  json_object_object_add( Set, "Device", json_object_new_string( Device.c_str() ) );
  json_object_object_add( Set, "Type", json_object_new_string( Type.c_str() ) );
  json_object_object_add( Set, "Format", json_object_new_string( Format.c_str() ) );
  json_object_object_add( Set, "TimeStamp", json_object_new_string( "" ) );
  json_object_object_add( Set, "Synced", json_object_new_boolean( false ) );
  json_object_array_add( Sets, Set );
  return Set;
 }

json_object * CloneJson( json_object * Value )
 {
  if( Value == nullptr )
    return nullptr;
  return json_tokener_parse( json_object_to_json_string_ext( Value, JSON_C_TO_STRING_PLAIN ) );
 }

bool ValidateProfileRows( json_object * Rows )
 {
  if( !json_object_is_type( Rows, json_type_array ) )
    return false;
  std::map<TGNString,bool> Ids;
  for( size_t Index = 0; Index < json_object_array_length( Rows ); ++Index )
   {
    json_object * Row = json_object_array_get_idx( Rows, Index );
    json_object * IdValue = nullptr;
    json_object * TempIdValue = nullptr;
    json_object * NoTempOn = nullptr;
    json_object * Measurement = nullptr;
    const bool HasMeasurement = json_object_is_type( Row, json_type_object ) &&
      json_object_object_get_ex( Row, "Measurement", &Measurement );
    if( ( HasMeasurement && ( !json_object_is_type( Measurement, json_type_string ) ||
          !GnUdpMeasurementFieldValid( json_object_get_string( Measurement ) ) ) ) ||
        !json_object_is_type( Row, json_type_object ) || json_object_object_length( Row ) != ( HasMeasurement ? 8 : 7 ) ||
        !json_object_object_get_ex( Row, "Id", &IdValue ) || !json_object_is_type( IdValue, json_type_string ) ||
        !json_object_object_get_ex( Row, "TempId", &TempIdValue ) || !json_object_is_type( TempIdValue, json_type_string ) ||
        !json_object_object_get_ex( Row, "NoTempOn", &NoTempOn ) || !json_object_is_type( NoTempOn, json_type_boolean ) )
      return false;
    const TGNString Id = json_object_get_string( IdValue );
    if( Id.empty() || TGNString( json_object_get_string( TempIdValue ) ).empty() || Ids[ Id ] )
      return false;
    Ids[ Id ] = true;
    double FloorOn = 0, FloorOff = 0, MainOn = 0, MainOff = 0;
    const char * Names[] = { "FloorOn", "FloorOff", "MainOn", "MainOff" };
    double * Values[] = { &FloorOn, &FloorOff, &MainOn, &MainOff };
    for( size_t ValueIndex = 0; ValueIndex < 4; ++ValueIndex )
     {
      json_object * Value = nullptr;
      if( !json_object_object_get_ex( Row, Names[ ValueIndex ], &Value ) ||
          ( !json_object_is_type( Value, json_type_double ) && !json_object_is_type( Value, json_type_int ) ) )
        return false;
      *Values[ ValueIndex ] = json_object_get_double( Value );
      if( !std::isfinite( *Values[ ValueIndex ] ) )
        return false;
     }
    if( FloorOn >= FloorOff || MainOn >= MainOff )
      return false;
   }
  return true;
 }

bool RowToEntry( json_object * Row, TGNString const & Device, TGNString const & Type,
                 TGNString const & Kind, json_object * & Entry )
 {
  if( !json_object_is_type( Row, json_type_array ) )
    return false;
  const size_t Count = json_object_array_length( Row );
  if( Count < 2 )
    return false;
  json_object * IdObject = json_object_array_get_idx( Row, Count - 1 );
  if( !json_object_is_type( IdObject, json_type_string ) )
    return false;
  const char * Id = json_object_get_string( IdObject );
  if( Id == nullptr || *Id == '\0' )
    return false;

  json_object * Data = json_object_new_array();
  for( size_t Index = 0; Index + 1 < Count; ++Index )
    json_object_array_add( Data, CloneJson( json_object_array_get_idx( Row, Index ) ) );

  Entry = json_object_new_object();
  json_object_object_add( Entry, "Id", json_object_new_string( Id ) );
  json_object_object_add( Entry, "Device", json_object_new_string( Device.c_str() ) );
  json_object_object_add( Entry, "Type", json_object_new_string( Type.c_str() ) );
  json_object_object_add( Entry, "Kind", json_object_new_string( Kind.c_str() ) );
  json_object_object_add( Entry, "Data", Data );
  return true;
 }

bool ExpandContent( json_object * Content, TGNString const & Device, TGNString const & Type,
                    TGNString const & Format, std::vector<json_object *> & Entries )
 {
  auto AddRows = [&]( json_object * Rows, TGNString const & Kind )
   {
    if( !json_object_is_type( Rows, json_type_array ) )
      return false;
    for( size_t Index = 0; Index < json_object_array_length( Rows ); ++Index )
     {
      json_object * Entry = nullptr;
      if( !RowToEntry( json_object_array_get_idx( Rows, Index ), Device, Type, Kind, Entry ) )
        return false;
      Entries.push_back( Entry );
     }
    return true;
   };

  if( Type == "FeedTimes" )
    return AddRows( Content, "FeedTime" );
  if( Type != "SwitchTimes" )
    return false;
  if( Format == "TmrSw" && json_object_is_type( Content, json_type_object ) )
   {
    json_object * SwitchTimes = nullptr;
    json_object * Intervals = nullptr;
    json_object * Channels = nullptr;
    json_object_object_get_ex( Content, "SwitchTimes", &SwitchTimes );
    json_object_object_get_ex( Content, "Intervals", &Intervals );
    json_object_object_get_ex( Content, "Channels", &Channels );
    if( SwitchTimes == nullptr || !json_object_is_type( SwitchTimes, json_type_array ) ||
        ( Intervals != nullptr && !json_object_is_type( Intervals, json_type_array ) ) ||
        ( Channels != nullptr && !json_object_is_type( Channels, json_type_object ) ) )
      return false;
    return AddRows( SwitchTimes, "SwitchTime" ) &&
      ( Intervals == nullptr || AddRows( Intervals, "Interval" ) );
   }
  if( Format == "MyTimer" )
   {
    json_object * SwitchTimes = nullptr;
    json_object * Intervals = nullptr;
    if( !json_object_is_type( Content, json_type_object ) ||
        !json_object_object_get_ex( Content, "SwitchTimes", &SwitchTimes ) ||
        !json_object_object_get_ex( Content, "Intervals", &Intervals ) )
      return false;
    return AddRows( SwitchTimes, "SwitchTime" ) && AddRows( Intervals, "Interval" );
   }
  return AddRows( Content, "SwitchTime" );
 }

void ReleaseEntries( std::vector<json_object *> & Entries )
 {
  for( json_object * Entry : Entries )
    json_object_put( Entry );
  Entries.clear();
 }

bool EntryMatches( json_object * Entry, TGNString const & Device, TGNString const & Type,
                   TGNString const & Kind )
 {
  return StringValue( Entry, "Device" ) == Device && StringValue( Entry, "Type" ) == Type &&
         StringValue( Entry, "Kind" ) == Kind;
 }

void MergeKind( json_object * Root, TGNString const & Device, TGNString const & Type,
                TGNString const & Kind, std::vector<json_object *> const & Incoming )
 {
  json_object * OldTimes = nullptr;
  json_object_object_get_ex( Root, "Times", &OldTimes );
  json_object * NewTimes = json_object_new_array();
  size_t IncomingIndex = 0;
  size_t LastTargetOutputIndex = static_cast<size_t>( -1 );

  for( size_t Index = 0; Index < json_object_array_length( OldTimes ); ++Index )
   {
    json_object * Existing = json_object_array_get_idx( OldTimes, Index );
    if( EntryMatches( Existing, Device, Type, Kind ) )
     {
      if( IncomingIndex < Incoming.size() )
       {
        json_object_array_add( NewTimes, CloneJson( Incoming[ IncomingIndex++ ] ) );
        LastTargetOutputIndex = json_object_array_length( NewTimes ) - 1;
       }
      continue;
     }
    json_object_array_add( NewTimes, CloneJson( Existing ) );
   }

  while( IncomingIndex < Incoming.size() )
   {
    json_object * Entry = CloneJson( Incoming[ IncomingIndex++ ] );
    if( LastTargetOutputIndex == static_cast<size_t>( -1 ) ||
        LastTargetOutputIndex + 1 >= json_object_array_length( NewTimes ) )
     {
      json_object_array_add( NewTimes, Entry );
      LastTargetOutputIndex = json_object_array_length( NewTimes ) - 1;
      continue;
     }

    json_object * Rebuilt = json_object_new_array();
    for( size_t Index = 0; Index < json_object_array_length( NewTimes ); ++Index )
     {
      json_object_array_add( Rebuilt, CloneJson( json_object_array_get_idx( NewTimes, Index ) ) );
      if( Index == LastTargetOutputIndex )
        json_object_array_add( Rebuilt, CloneJson( Entry ) );
     }
    json_object_put( Entry );
    json_object_put( NewTimes );
    NewTimes = Rebuilt;
    ++LastTargetOutputIndex;
   }

  json_object_object_add( Root, "Times", NewTimes );
 }

bool MergeContent( json_object * Root, TGNString const & Device, TGNString const & Type,
                   TGNString const & Format, json_object * Content )
 {
  std::vector<json_object *> Entries;
  if( !ExpandContent( Content, Device, Type, Format, Entries ) )
   {
    ReleaseEntries( Entries );
    return false;
   }

  bool IncludesIntervals = Format == "MyTimer";
  if( Format == "TmrSw" && json_object_is_type( Content, json_type_object ) )
   {
    json_object * Intervals = nullptr;
    IncludesIntervals = json_object_object_get_ex( Content, "Intervals", &Intervals ) &&
                        json_object_is_type( Intervals, json_type_array );
   }

  const char * Kinds[] = { "SwitchTime", "Interval", "FeedTime" };
  for( char const * Kind : Kinds )
   {
    if( TGNString( Kind ) == "Interval" && !IncludesIntervals )
      continue;
    std::vector<json_object *> KindEntries;
    for( json_object * Entry : Entries )
      if( StringValue( Entry, "Kind" ) == Kind )
        KindEntries.push_back( Entry );
    if( !KindEntries.empty() || Type == "SwitchTimes" || Type == "FeedTimes" )
      MergeKind( Root, Device, Type, Kind, KindEntries );
   }
  ReleaseEntries( Entries );
  return true;
 }

bool MergeProfileRows( json_object * ExistingRows, json_object * IncomingRows,
                       json_object * & MergedRows )
 {
  if( !ValidateProfileRows( ExistingRows ) || !ValidateProfileRows( IncomingRows ) )
    return false;
  MergedRows = CloneJson( ExistingRows );
  for( size_t IncomingIndex = 0; IncomingIndex < json_object_array_length( IncomingRows ); ++IncomingIndex )
   {
    json_object * IncomingProfile = json_object_array_get_idx( IncomingRows, IncomingIndex );
    const TGNString IncomingId = StringValue( IncomingProfile, "Id" );
    bool Replaced = false;
    for( size_t ExistingIndex = 0; ExistingIndex < json_object_array_length( MergedRows ); ++ExistingIndex )
     {
      json_object * ExistingProfile = json_object_array_get_idx( MergedRows, ExistingIndex );
      if( StringValue( ExistingProfile, "Id" ) != IncomingId ) continue;
      if( JsonText( ExistingProfile, false ) != JsonText( IncomingProfile, false ) )
        json_object_array_put_idx( MergedRows, ExistingIndex, CloneJson( IncomingProfile ) );
      Replaced = true;
      break;
     }
    if( !Replaced )
      json_object_array_add( MergedRows, CloneJson( IncomingProfile ) );
   }
  return true;
 }

bool SameProfiles( json_object * ExistingRows, json_object * IncomingRows )
 {
  if( IncomingRows == nullptr )
    return true;
  if( json_object_array_length( ExistingRows ) != json_object_array_length( IncomingRows ) )
    return false;
  if( json_object_array_length( IncomingRows ) == 0 )
    return json_object_array_length( ExistingRows ) == 0;
  json_object * MergedRows = nullptr;
  if( !MergeProfileRows( ExistingRows, IncomingRows, MergedRows ) )
    return false;
  const bool Same = JsonText( ExistingRows, false ) == JsonText( MergedRows, false );
  json_object_put( MergedRows );
  return Same;
 }

bool ProfilesChangedByIncoming( json_object * ExistingRows, json_object * IncomingRows )
 {
  if( IncomingRows == nullptr )
    return false;
  for( size_t IncomingIndex = 0; IncomingIndex < json_object_array_length( IncomingRows ); ++IncomingIndex )
   {
    json_object * IncomingProfile = json_object_array_get_idx( IncomingRows, IncomingIndex );
    const TGNString IncomingId = StringValue( IncomingProfile, "Id" );
    bool Found = false;
    for( size_t ExistingIndex = 0; ExistingIndex < json_object_array_length( ExistingRows ); ++ExistingIndex )
     {
      json_object * ExistingProfile = json_object_array_get_idx( ExistingRows, ExistingIndex );
      if( StringValue( ExistingProfile, "Id" ) != IncomingId ) continue;
      if( JsonText( ExistingProfile, false ) != JsonText( IncomingProfile, false ) )
        return true;
      Found = true;
      break;
     }
    if( !Found ) return true;
   }
  return false;
 }

bool SaveMergedProfiles( std::filesystem::path const & ProfilesFile, json_object * ExistingRows,
                         json_object * IncomingRows, TGNString const & Revision )
 {
  json_object * MergedRows = nullptr;
  if( !MergeProfileRows( ExistingRows, IncomingRows, MergedRows ) )
    return false;
  json_object * Profiles = json_object_new_object();
  json_object_object_add( Profiles, "Profiles", MergedRows );
  SetString( Profiles, "TimeStamp", Revision );
  const bool Saved = SaveStore( ProfilesFile, Profiles );
  json_object_put( Profiles );
  if( !Saved )
    return false;

  json_object * Sync = json_object_from_file( ( ProfilesFile.parent_path() / "HeatProfiles.sync.json" ).c_str() );
  if( Sync == nullptr )
    Sync = json_tokener_parse( "{\"Version\":1,\"Revision\":\"\",\"Sets\":[]}" );
  SetString( Sync, "Revision", Revision );
  json_object * SyncSets = nullptr;
  if( json_object_object_get_ex( Sync, "Sets", &SyncSets ) && json_object_is_type( SyncSets, json_type_array ) )
    for( size_t Index = 0; Index < json_object_array_length( SyncSets ); ++Index )
      json_object_object_add( json_object_array_get_idx( SyncSets, Index ), "Synced", json_object_new_boolean( false ) );
  const bool SyncSaved = SaveStore( ProfilesFile.parent_path() / "HeatProfiles.sync.json", Sync );
  json_object_put( Sync );
  return SyncSaved;
 }

void MarkSwitchSetsPending( json_object * Root, TGNString const & Revision )
 {
  json_object * Sets = nullptr;
  if( !json_object_object_get_ex( Root, "Sets", &Sets ) || !json_object_is_type( Sets, json_type_array ) )
    return;
  for( size_t Index = 0; Index < json_object_array_length( Sets ); ++Index )
   {
    json_object * Set = json_object_array_get_idx( Sets, Index );
    if( StringValue( Set, "Type" ) != "SwitchTimes" ) continue;
    SetString( Set, "TimeStamp", Revision );
    json_object_object_add( Set, "Synced", json_object_new_boolean( false ) );
   }
 }

json_object * ChannelRowsFor( json_object * Channels, TGNString const & Device )
 {
  json_object * DeviceChannels = nullptr;
  if( Channels == nullptr || !json_object_object_get_ex( Channels, Device.c_str(), &DeviceChannels ) )
    return nullptr;
  if( json_object_is_type( DeviceChannels, json_type_array ) )
    return DeviceChannels;
  json_object * Rows = nullptr;
  if( json_object_is_type( DeviceChannels, json_type_object ) &&
      json_object_object_get_ex( DeviceChannels, "Rows", &Rows ) && json_object_is_type( Rows, json_type_array ) )
    return Rows;
  return nullptr;
 }

TGNString ChannelTimeStampFor( json_object * Channels, TGNString const & Device )
 {
  json_object * DeviceChannels = nullptr;
  if( Channels == nullptr || !json_object_object_get_ex( Channels, Device.c_str(), &DeviceChannels ) ||
      !json_object_is_type( DeviceChannels, json_type_object ) )
    return {};
  return StringValue( DeviceChannels, "TimeStamp" );
 }

bool SameTimeContent( json_object * Central, json_object * Incoming )
 {
  json_object * CentralCopy = CloneJson( Central );
  json_object * IncomingCopy = CloneJson( Incoming );
  if( json_object_is_type( CentralCopy, json_type_object ) )
    json_object_object_del( CentralCopy, "Channels" );
  if( json_object_is_type( IncomingCopy, json_type_object ) )
    json_object_object_del( IncomingCopy, "Channels" );
  const bool Same = JsonText( CentralCopy, false ) == JsonText( IncomingCopy, false );
  if( CentralCopy != nullptr ) json_object_put( CentralCopy );
  if( IncomingCopy != nullptr ) json_object_put( IncomingCopy );
  return Same;
 }

bool SameChannelRows( json_object * ExistingRows, json_object * IncomingRows )
 {
  return ExistingRows != nullptr && IncomingRows != nullptr &&
    JsonText( ExistingRows, false ) == JsonText( IncomingRows, false );
 }

bool SameChannelHardware( json_object * ExistingRow, json_object * IncomingRow )
 {
  json_object * ExistingCopy = CloneJson( ExistingRow );
  json_object * IncomingCopy = CloneJson( IncomingRow );
  if( json_object_is_type( ExistingCopy, json_type_object ) )
   {
    json_object_object_del( ExistingCopy, "Role" );
    json_object_object_del( ExistingCopy, "Profile" );
   }
  if( json_object_is_type( IncomingCopy, json_type_object ) )
   {
    json_object_object_del( IncomingCopy, "Role" );
    json_object_object_del( IncomingCopy, "Profile" );
   }
  const bool Same = JsonText( ExistingCopy, false ) == JsonText( IncomingCopy, false );
  if( ExistingCopy != nullptr ) json_object_put( ExistingCopy );
  if( IncomingCopy != nullptr ) json_object_put( IncomingCopy );
  return Same;
 }

bool SameChannelTopology( json_object * ExistingChannels, json_object * IncomingChannels )
 {
  if( !json_object_is_type( ExistingChannels, json_type_object ) ||
      !json_object_is_type( IncomingChannels, json_type_object ) ||
      json_object_object_length( ExistingChannels ) != json_object_object_length( IncomingChannels ) ) return false;
  json_object_object_foreach( ExistingChannels, Device, ExistingState )
   {
    json_object * IncomingState = nullptr;
    if( !json_object_object_get_ex( IncomingChannels, Device, &IncomingState ) ) return false;
    json_object * ExistingRows = ChannelRowsFor( ExistingChannels, Device );
    json_object * IncomingRows = ChannelRowsFor( IncomingChannels, Device );
    if( !json_object_is_type( ExistingRows, json_type_array ) ||
        !json_object_is_type( IncomingRows, json_type_array ) ) return false;
    for( size_t Index = 0; Index < json_object_array_length( IncomingRows ); ++Index )
     {
      json_object * IncomingRow = json_object_array_get_idx( IncomingRows, Index );
      bool Found = false;
      for( size_t ExistingIndex = 0; ExistingIndex < json_object_array_length( ExistingRows ); ++ExistingIndex )
       {
        json_object * ExistingRow = json_object_array_get_idx( ExistingRows, ExistingIndex );
        if( StringValue( ExistingRow, "Id" ) != StringValue( IncomingRow, "Id" ) ) continue;
        if( !SameChannelHardware( ExistingRow, IncomingRow ) ) return false;
        Found = true;
        break;
       }
      if( !Found ) return false;
     }
    (void)ExistingState;
    (void)IncomingState;
   }
  return true;
 }

void StoreDeviceChannels( json_object * Channels, TGNString const & Device,
                          json_object * Rows, TGNString const & Revision )
 {
  json_object * DeviceChannels = json_object_new_object();
  SetString( DeviceChannels, "TimeStamp", Revision );
  json_object_object_add( DeviceChannels, "Rows", CloneJson( Rows ) );
  json_object_object_add( Channels, Device.c_str(), DeviceChannels );
 }

json_object * EntryToRow( json_object * Entry )
 {
  json_object * Data = nullptr;
  if( !json_object_object_get_ex( Entry, "Data", &Data ) || !json_object_is_type( Data, json_type_array ) )
    return nullptr;
  const TGNString Id = StringValue( Entry, "Id" );
  if( Id.empty() )
    return nullptr;
  json_object * Row = CloneJson( Data );
  json_object_array_add( Row, json_object_new_string( Id.c_str() ) );
  return Row;
 }

json_object * BuildContent( json_object * Root, TGNString const & Device, TGNString const & Type,
                            TGNString const & Format, std::filesystem::path const & ProfilesFile )
 {
  json_object * Times = nullptr;
  json_object_object_get_ex( Root, "Times", &Times );
  if( Type == "FeedTimes" )
   {
    json_object * Rows = json_object_new_array();
    const TGNString Kind = Type == "FeedTimes" ? "FeedTime" : "SwitchTime";
    for( size_t Index = 0; Index < json_object_array_length( Times ); ++Index )
     {
      json_object * Entry = json_object_array_get_idx( Times, Index );
      if( !EntryMatches( Entry, Device, Type, Kind ) )
        continue;
      if( json_object * Row = EntryToRow( Entry ) )
        json_object_array_add( Rows, Row );
     }
    return Rows;
   }

  json_object * Content = json_object_new_object();
  json_object * SwitchRows = json_object_new_array();
  json_object * IntervalRows = json_object_new_array();
  for( size_t Index = 0; Index < json_object_array_length( Times ); ++Index )
   {
    json_object * Entry = json_object_array_get_idx( Times, Index );
    if( StringValue( Entry, "Device" ) != Device || StringValue( Entry, "Type" ) != Type )
      continue;
    const TGNString Kind = StringValue( Entry, "Kind" );
    json_object * Row = EntryToRow( Entry );
    if( Row == nullptr )
      continue;
    if( Kind == "Interval" )
      json_object_array_add( IntervalRows, Row );
    else if( Kind == "SwitchTime" )
      json_object_array_add( SwitchRows, Row );
    else
      json_object_put( Row );
   }
  json_object_object_add( Content, "SwitchTimes", SwitchRows );
  if( Format == "MyTimer" || Format == "TmrSw" )
    json_object_object_add( Content, "Intervals", IntervalRows );
  json_object * Channels = nullptr;
  json_object * DeviceChannelState = nullptr;
  json_object_object_get_ex( Root, "Channels", &Channels );
  if( Channels != nullptr && json_object_object_get_ex( Channels, Device.c_str(), &DeviceChannelState ) )
   {
    json_object * DeviceChannels = ChannelRowsFor( Channels, Device );
    if( DeviceChannels == nullptr )
      return Content;
    json_object * ChannelConfig = json_object_new_object();
    json_object * Profiles = json_object_from_file( ProfilesFile.c_str() );
    if( Profiles != nullptr && json_object_is_type( Profiles, json_type_object ) )
     {
      json_object * ProfileRows = nullptr;
      if( json_object_object_get_ex( Profiles, "Profiles", &ProfileRows ) )
        json_object_object_add( ChannelConfig, "Profiles", CloneJson( ProfileRows ) );
      else
        json_object_object_add( ChannelConfig, "Profiles", json_object_new_array() );
     }
    else
      json_object_object_add( ChannelConfig, "Profiles", json_object_new_array() );
    json_object_object_add( ChannelConfig, "Channels", CloneJson( DeviceChannels ) );
    json_object_object_add( Content, "Channels", ChannelConfig );
    if( Profiles != nullptr ) json_object_put( Profiles );
   }
  return Content;
 }

json_object * BuildSetPayload( json_object * Root, json_object * Set,
                               std::filesystem::path const & ProfilesFile )
 {
  const TGNString Device = StringValue( Set, "Device" );
  const TGNString Type = StringValue( Set, "Type" );
  const TGNString Format = StringValue( Set, "Format" );
  json_object * Payload = json_object_new_object();
  json_object_object_add( Payload, "Device", json_object_new_string( Device.c_str() ) );
  json_object_object_add( Payload, "Type", json_object_new_string( Type.c_str() ) );
  json_object_object_add( Payload, "Format", json_object_new_string( Format.c_str() ) );
  json_object_object_add( Payload, "TimeStamp", json_object_new_string( StringValue( Set, "TimeStamp" ).c_str() ) );
  json_object_object_add( Payload, "Content", BuildContent( Root, Device, Type, Format, ProfilesFile ) );
  return Payload;
 }

TGNString ContentSignature( json_object * Root, TGNString const & Device, TGNString const & Type,
                            TGNString const & Format, std::filesystem::path const & ProfilesFile )
 {
  json_object * Content = BuildContent( Root, Device, Type, Format, ProfilesFile );
  const TGNString Signature = JsonText( Content, false );
  json_object_put( Content );
  return Signature;
 }

bool ValidateCentralTimes( json_object * Root )
 {
  json_object * Times = nullptr;
  if( Root == nullptr || !json_object_is_type( Root, json_type_object ) ||
      !json_object_object_get_ex( Root, "Times", &Times ) || !json_object_is_type( Times, json_type_array ) )
    return false;
  json_object * Channels = nullptr;
  if( json_object_object_get_ex( Root, "Channels", &Channels ) && !json_object_is_type( Channels, json_type_object ) )
    return false;
  for( size_t Index = 0; Index < json_object_array_length( Times ); ++Index )
   {
    json_object * Entry = json_object_array_get_idx( Times, Index );
    json_object * Data = nullptr;
    if( !json_object_is_type( Entry, json_type_object ) || StringValue( Entry, "Id" ).empty() ||
        StringValue( Entry, "Device" ).empty() || StringValue( Entry, "Type" ).empty() ||
        StringValue( Entry, "Kind" ).empty() ||
        !json_object_object_get_ex( Entry, "Data", &Data ) || !json_object_is_type( Data, json_type_array ) )
      return false;
   }
  return true;
 }

bool ValidateCentralChannels( json_object * Channels, json_object * ProfileRows )
 {
  if( !json_object_is_type( Channels, json_type_object ) || !ValidateProfileRows( ProfileRows ) ) return false;
  std::map<TGNString,bool> ProfileIds;
  for( size_t Index = 0; Index < json_object_array_length( ProfileRows ); ++Index )
    ProfileIds[ StringValue( json_object_array_get_idx( ProfileRows, Index ), "Id" ) ] = true;
  json_object_object_foreach( Channels, Device, State )
   {
    if( TGNString( Device ).empty() || !json_object_is_type( State, json_type_object ) ||
        StringValue( State, "TimeStamp" ).empty() ) return false;
    json_object * Rows = nullptr;
    if( !json_object_object_get_ex( State, "Rows", &Rows ) || !json_object_is_type( Rows, json_type_array ) ) return false;
    std::map<TGNString,bool> ChannelIds;
    for( size_t Index = 0; Index < json_object_array_length( Rows ); ++Index )
     {
      json_object * Row = json_object_array_get_idx( Rows, Index );
      if( !json_object_is_type( Row, json_type_object ) ) return false;
      const TGNString Id = StringValue( Row, "Id" );
      const TGNString Role = StringValue( Row, "Role" );
      const TGNString Profile = StringValue( Row, "Profile" );
      json_object * RoleValue = nullptr;
      const bool HasRole = json_object_object_get_ex( Row, "Role", &RoleValue );
      json_object * ProfileValue = nullptr;
      const bool HasProfile = json_object_object_get_ex( Row, "Profile", &ProfileValue );
      if( Id.empty() || ChannelIds[ Id ] ||
          ( HasRole && Role != "Timer" && Role != "HeaterMain" && Role != "HeaterFloor" ) ||
          ( !HasRole && HasProfile ) ) return false;
      ChannelIds[ Id ] = true;
      if( !HasRole || Role == "Timer" )
       {
        if( HasProfile ) return false;
       }
      else if( Profile.empty() || !ProfileIds[ Profile ] ) return false;
     }
   }
  return true;
 }

bool CopySetMetadataForManualSave( json_object * OldRoot, json_object * NewRoot,
                                  std::filesystem::path const & ProfilesFile )
 {
  json_object * OldChannels = nullptr;
  json_object * NewChannels = nullptr;
  if( json_object_object_get_ex( OldRoot, "Channels", &OldChannels ) &&
      !json_object_object_get_ex( NewRoot, "Channels", &NewChannels ) )
    json_object_object_add( NewRoot, "Channels", CloneJson( OldChannels ) );
  if( OldChannels != nullptr && NewChannels != nullptr && json_object_is_type( NewChannels, json_type_object ) )
   {
    std::vector<TGNString> ChangedDevices;
    json_object_object_foreach( NewChannels, Device, State )
     {
      json_object * OldRows = ChannelRowsFor( OldChannels, Device );
      json_object * NewRows = ChannelRowsFor( NewChannels, Device );
      if( NewRows != nullptr && !SameChannelRows( OldRows, NewRows ) ) ChangedDevices.emplace_back( Device );
     }
    for( TGNString const & Device : ChangedDevices )
     {
      json_object * State = nullptr;
      json_object_object_get_ex( NewChannels, Device.c_str(), &State );
      SetString( State, "TimeStamp", NowTimeStamp() );
     }
   }
  json_object * NewTimes = nullptr;
  json_object_object_get_ex( NewRoot, "Times", &NewTimes );
  json_object * NewSets = json_object_new_array();
  std::map<TGNString,bool> Seen;
  bool AnyChanged = false;

  auto AddSet = [&]( TGNString const & Device, TGNString const & Type )
   {
    const TGNString Key = Device + "\n" + Type;
    if( Seen[ Key ] )
      return;
    Seen[ Key ] = true;
    json_object * OldSet = FindSet( OldRoot, Device, Type );
    TGNString Format = OldSet != nullptr ? StringValue( OldSet, "Format" ) : "";
    if( Format.empty() )
      Format = Type == "FeedTimes" ? "Feeder" : "TmrSw";
    const TGNString OldSignature = OldSet != nullptr
      ? ContentSignature( OldRoot, Device, Type, Format, ProfilesFile ) : TGNString();
    const TGNString NewSignature = ContentSignature( NewRoot, Device, Type, Format, ProfilesFile );

    json_object * Set = json_object_new_object();
    json_object_object_add( Set, "Device", json_object_new_string( Device.c_str() ) );
    json_object_object_add( Set, "Type", json_object_new_string( Type.c_str() ) );
    json_object_object_add( Set, "Format", json_object_new_string( Format.c_str() ) );
    json_object_array_add( NewSets, Set );

    const bool Changed = OldSet == nullptr || OldSignature != NewSignature;
    AnyChanged = AnyChanged || Changed;
    const TGNString TimeStamp = Changed ? NowTimeStamp() : StringValue( OldSet, "TimeStamp" );
    const bool Synced = Changed ? false : BoolValue( OldSet, "Synced", false );
    json_object_object_add( Set, "TimeStamp", json_object_new_string( TimeStamp.c_str() ) );
    json_object_object_add( Set, "Synced", json_object_new_boolean( Synced ) );
   };

  for( size_t Index = 0; Index < json_object_array_length( NewTimes ); ++Index )
   {
    json_object * Entry = json_object_array_get_idx( NewTimes, Index );
    AddSet( StringValue( Entry, "Device" ), StringValue( Entry, "Type" ) );
   }

  json_object * OldSets = nullptr;
  json_object_object_get_ex( OldRoot, "Sets", &OldSets );
  for( size_t Index = 0; Index < json_object_array_length( OldSets ); ++Index )
   {
    json_object * OldSet = json_object_array_get_idx( OldSets, Index );
    AddSet( StringValue( OldSet, "Device" ), StringValue( OldSet, "Type" ) );
   }

  json_object_object_add( NewRoot, "Sets", NewSets );
  json_object_object_add( NewRoot, "Version", json_object_new_int( 1 ) );
  return AnyChanged;
 }

bool ParseHttpUrl( TGNString Url, TGNString & Host, unsigned & Port )
 {
  constexpr char Prefix[] = "http://";
  if( Url.rfind( Prefix, 0 ) != 0 )
    return false;
  Url.erase( 0, sizeof( Prefix ) - 1 );
  const size_t Slash = Url.find( '/' );
  if( Slash != TGNString::npos )
    Url.erase( Slash );
  const size_t Colon = Url.rfind( ':' );
  Port = 80;
  if( Colon != TGNString::npos )
   {
    try
     {
      Port = static_cast<unsigned>( std::stoul( Url.substr( Colon + 1 ) ) );
     }
    catch( ... )
     {
      return false;
     }
    Url.erase( Colon );
   }
  Host = Url;
  return !Host.empty() && Port > 0 && Port <= 65535;
 }
 } // namespace

TTimeStore::TTimeStore( std::filesystem::path RootDirectoryValue, TDeviceRegistry & RegistryValue )
: Registry( RegistryValue )
, RootDirectory( std::filesystem::weakly_canonical( std::move( RootDirectoryValue ) ) )
, FileName( RootDirectory / "data" / "Times.json" )
, ProfilesFile( RootDirectory / "data" / "HeatProfiles.json" )
 {
  LoadEndpoints();
  NextRetry = std::chrono::steady_clock::now() + std::chrono::seconds( 5 );
 }

void TTimeStore::LoadEndpoints(void)
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
      const TGNString Name = StringValue( Device, "Name" );
      const TGNString BaseUrl = StringValue( Device, "BaseUrl" );
      TGNString Host;
      unsigned Port = 80;
      if( !ParseHttpUrl( BaseUrl, Host, Port ) )
        continue;
      TEndpoint Endpoint{ Host, Port };
      if( !Name.empty() ) Endpoints[ Name ] = Endpoint;
      Endpoints[ Host ] = Endpoint;
      if( Host == "gn-pi" ) Endpoints[ "GN-Pi" ] = Endpoint;
     }
   }
  json_object_put( Root );
  for( auto const & Item : Registry.Devices() )
    Endpoints[Item.first] = TEndpoint{ Item.second.IP, Item.second.Port };
 }

bool TTimeStore::HandleCentralRequest( TGNString const & Method, TGNString const & Body,
                                       unsigned & StatusCode, TGNString & Response )
 {
  if( Method == "GET" || Method == "HEAD" )
   {
    json_object * Root = LoadStore( FileName );
    if( Root == nullptr )
     {
      StatusCode = 500;
      Response = "{\"Ok\":false,\"Error\":\"Invalid central Times.json\"}";
      return true;
     }
    StatusCode = 200;
    Response = JsonText( Root, true );
    json_object_put( Root );
    return true;
   }

  if( Method != "PUT" )
   {
    StatusCode = 405;
    Response = "{\"Ok\":false,\"Error\":\"Method not allowed\"}";
    return true;
   }

  json_object * Incoming = json_tokener_parse( Body.c_str() );
  if( !ValidateCentralTimes( Incoming ) )
   {
    if( Incoming != nullptr ) json_object_put( Incoming );
    StatusCode = 400;
    Response = "{\"Ok\":false,\"Error\":\"Invalid time configuration\"}";
    return true;
   }
  json_object * IncomingChannels = nullptr;
  if( json_object_object_get_ex( Incoming, "Channels", &IncomingChannels ) )
   {
    json_object * Profiles = json_object_from_file( ProfilesFile.c_str() );
    json_object * ProfileRows = nullptr;
    const bool ValidChannels = Profiles != nullptr &&
      json_object_object_get_ex( Profiles, "Profiles", &ProfileRows ) &&
      ValidateCentralChannels( IncomingChannels, ProfileRows );
    if( Profiles != nullptr ) json_object_put( Profiles );
    if( !ValidChannels )
     {
      json_object_put( Incoming );
      StatusCode = 400;
      Response = "{\"Ok\":false,\"Error\":\"Invalid channel assignments or unknown heater profile\"}";
      return true;
     }
   }
  json_object * OldRoot = LoadStore( FileName );
  if( OldRoot == nullptr )
   {
    json_object_put( Incoming );
    StatusCode = 500;
    Response = "{\"Ok\":false,\"Error\":\"Invalid existing Times.json\"}";
    return true;
   }
  if( IncomingChannels != nullptr )
   {
    json_object * OldChannels = nullptr;
    json_object_object_get_ex( OldRoot, "Channels", &OldChannels );
    if( !SameChannelTopology( OldChannels, IncomingChannels ) )
     {
      json_object_put( OldRoot );
      json_object_put( Incoming );
      StatusCode = 400;
      Response = "{\"Ok\":false,\"Error\":\"Channel editing cannot add, remove or reorder channel rows\"}";
      return true;
     }
   }

  const bool Changed = CopySetMetadataForManualSave( OldRoot, Incoming, ProfilesFile );
  const bool Saved = SaveStore( FileName, Incoming );
  json_object_put( OldRoot );
  json_object_put( Incoming );
  if( !Saved )
   {
    StatusCode = 500;
    Response = "{\"Ok\":false,\"Error\":\"Cannot save Times.json\"}";
    return true;
   }
  TryPushUnsynced();
  StatusCode = 200;
  Response = Changed ? "{\"Ok\":true,\"Changed\":true}" : "{\"Ok\":true,\"Changed\":false}";
  return true;
 }

bool TTimeStore::HandleSyncRequest( TGNString const & Body,
                                    unsigned & StatusCode, TGNString & Response )
 {
  json_object * Incoming = json_tokener_parse( Body.c_str() );
  json_object * Content = nullptr;
  const TGNString WireDevice = StringValue( Incoming, "Device" );
  const TGNString Device = Registry.Function( WireDevice );
  const TGNString Type = StringValue( Incoming, "Type" );
  const TGNString Format = StringValue( Incoming, "Format" );
  const TGNString TimeStamp = StringValue( Incoming, "TimeStamp" );
  if( Incoming == nullptr || Device.empty() || Type.empty() || Format.empty() || TimeStamp.empty() ||
      !json_object_object_get_ex( Incoming, "Content", &Content ) )
   {
    if( Incoming != nullptr ) json_object_put( Incoming );
    StatusCode = 400;
    Response = "{\"Ok\":false,\"Error\":\"Invalid sync payload\"}";
    return true;
   }

  if( !Registry.ValidateIdentity( Device, Incoming ) )
   {
    json_object_put( Incoming );
    StatusCode = 409;
    Response = "{\"Ok\":false,\"Error\":\"IdentityMismatch\"}";
    return true;
   }
  if( !Registry.AcceptWireId( WireDevice ) )
   {
    json_object_put( Incoming );
    StatusCode = 409; Response = "{\"Ok\":false,\"Error\":\"StableDeviceIdRequired\"}"; return true;
   }
  const bool AssignmentRequired = Registry.NeedsAssignment( Device, Incoming );

  json_object * Root = LoadStore( FileName );
  if( Root == nullptr )
   {
    json_object_put( Incoming );
    StatusCode = 500;
    Response = "{\"Ok\":false,\"Error\":\"Invalid central Times.json\"}";
    return true;
   }
  json_object * Set = FindSet( Root, Device, Type );
  const TGNString CentralTimeStamp = Set != nullptr ? StringValue( Set, "TimeStamp" ) : "";
  std::vector<json_object *> CheckEntries;
  if( !ExpandContent( Content, Device, Type, Format, CheckEntries ) )
   {
    ReleaseEntries( CheckEntries );
    json_object_put( Root );
    json_object_put( Incoming );
    StatusCode = 400;
    Response = "{\"Ok\":false,\"Error\":\"Invalid content or missing UUID\"}";
    return true;
   }
  ReleaseEntries( CheckEntries );

  json_object * IncomingProfileRows = nullptr;
  if( Type == "SwitchTimes" && json_object_is_type( Content, json_type_object ) )
   {
    json_object * IncomingChannels = nullptr;
    json_object_object_get_ex( Content, "Channels", &IncomingChannels );
    json_object * ChannelRows = nullptr;
    if( IncomingChannels != nullptr &&
        ( !json_object_is_type( IncomingChannels, json_type_object ) ||
          !json_object_object_get_ex( IncomingChannels, "Channels", &ChannelRows ) ||
          !json_object_is_type( ChannelRows, json_type_array ) ) )
     {
      StatusCode = 400;
      Response = "{\"Ok\":false,\"Error\":\"Invalid device Channels payload\"}";
      json_object_put( Root );
      json_object_put( Incoming );
      return true;
     }
    if( IncomingChannels != nullptr )
     {
      json_object_object_get_ex( IncomingChannels, "Profiles", &IncomingProfileRows );
      if( IncomingProfileRows != nullptr && !ValidateProfileRows( IncomingProfileRows ) )
       {
        StatusCode = 400;
        Response = "{\"Ok\":false,\"Error\":\"Invalid device HeatProfiles payload\"}";
        json_object_put( Root );
        json_object_put( Incoming );
        return true;
       }
     }
   }

  json_object * Profiles = json_object_from_file( ProfilesFile.c_str() );
  json_object * CentralProfileRows = nullptr;
  if( Profiles == nullptr || !json_object_object_get_ex( Profiles, "Profiles", &CentralProfileRows ) ||
      !ValidateProfileRows( CentralProfileRows ) )
   {
    if( Profiles != nullptr ) json_object_put( Profiles );
    StatusCode = 500;
    Response = "{\"Ok\":false,\"Error\":\"Invalid central HeatProfiles.json\"}";
    json_object_put( Root );
    json_object_put( Incoming );
    return true;
   }

  const bool ProfileDifference = Type == "SwitchTimes" &&
    !SameProfiles( CentralProfileRows, IncomingProfileRows );
  const bool IncomingProfileChange = Type == "SwitchTimes" &&
    ProfilesChangedByIncoming( CentralProfileRows, IncomingProfileRows );
  const TGNString ProfileRevision = StringValue( Profiles, "TimeStamp" );
  const bool AcceptProfiles = !AssignmentRequired && ( IncomingProfileChange &&
    ( ProfileRevision.empty() || TimeStamp > ProfileRevision ) );

  json_object * RootChannels = nullptr;
  json_object_object_get_ex( Root, "Channels", &RootChannels );
  json_object * IncomingChannelRows = nullptr;
  if( Type == "SwitchTimes" && json_object_is_type( Content, json_type_object ) )
   {
    json_object * IncomingChannels = nullptr;
    json_object_object_get_ex( Content, "Channels", &IncomingChannels );
    if( IncomingChannels != nullptr )
      json_object_object_get_ex( IncomingChannels, "Channels", &IncomingChannelRows );
   }
  json_object * CentralChannelRows = ChannelRowsFor( RootChannels, Device );
  const TGNString ChannelRevision = ChannelTimeStampFor( RootChannels, Device );
  const bool ChannelDifference = IncomingChannelRows != nullptr &&
    !SameChannelRows( CentralChannelRows, IncomingChannelRows );
  const bool AcceptChannels = !AssignmentRequired && ( IncomingChannelRows != nullptr &&
    ( CentralChannelRows == nullptr || ChannelRevision.empty() || TimeStamp > ChannelRevision ) );

  const bool AcceptTimes = !AssignmentRequired && ( Set == nullptr || TimeStamp > CentralTimeStamp );
  const bool DeviceRevisionBehind =
    ( !CentralTimeStamp.empty() && TimeStamp < CentralTimeStamp ) ||
    ( !ChannelRevision.empty() && TimeStamp < ChannelRevision ) ||
    ( !ProfileRevision.empty() && TimeStamp < ProfileRevision );
  bool TimeDifference = true;
  if( Set != nullptr )
   {
    json_object * CentralContent = BuildContent( Root, Device, Type, Format, ProfilesFile );
    TimeDifference = !SameTimeContent( CentralContent, Content );
    json_object_put( CentralContent );
   }

  if( AcceptTimes )
   {
    if( !MergeContent( Root, Device, Type, Format, Content ) )
     {
      if( Profiles != nullptr ) json_object_put( Profiles );
      StatusCode = 400;
      Response = "{\"Ok\":false,\"Error\":\"Cannot merge device content\"}";
      json_object_put( Root );
      json_object_put( Incoming );
      return true;
     }
    Set = EnsureSet( Root, Device, Type, Format );
    SetString( Set, "TimeStamp", TimeStamp );
    json_object_object_add( Set, "Synced", json_object_new_boolean( true ) );
   }
  if( AcceptChannels )
    StoreDeviceChannels( RootChannels, Device, IncomingChannelRows, TimeStamp );

  const TGNString Revision = NowTimeStamp();
  if( AcceptProfiles )
   {
    if( !SaveMergedProfiles( ProfilesFile, CentralProfileRows, IncomingProfileRows, Revision ) )
     {
      json_object_put( Profiles );
      StatusCode = 500;
      Response = "{\"Ok\":false,\"Error\":\"Cannot save central HeatProfiles.json\"}";
      json_object_put( Root );
      json_object_put( Incoming );
      return true;
     }
    MarkSwitchSetsPending( Root, Revision );
   }
  else if( ProfileDifference )
   {
    // The profile copy is newer at the Dashboard. Preserve the valid time set,
    // then return it together with the authoritative global profiles.
    Set = EnsureSet( Root, Device, Type, Format );
    SetString( Set, "TimeStamp", Revision );
    json_object_object_add( Set, "Synced", json_object_new_boolean( false ) );
   }
  else if( ( ChannelDifference && !AcceptChannels ) ||
           ( !AcceptTimes && TimeDifference ) || DeviceRevisionBehind )
   {
    Set = EnsureSet( Root, Device, Type, Format );
    SetString( Set, "TimeStamp", Revision );
    json_object_object_add( Set, "Synced", json_object_new_boolean( false ) );
   }

  if( !SaveStore( FileName, Root ) )
   {
    if( Profiles != nullptr ) json_object_put( Profiles );
    StatusCode = 500;
    Response = "{\"Ok\":false,\"Error\":\"Cannot save device content\"}";
    json_object_put( Root );
    json_object_put( Incoming );
    return true;
   }
  if( Profiles != nullptr ) json_object_put( Profiles );

  if( AcceptProfiles )
    TryPushUnsynced();

  if( ( ProfileDifference && !AcceptProfiles ) ||
      ( ChannelDifference && !AcceptChannels ) || ( !AcceptTimes && TimeDifference ) ||
      DeviceRevisionBehind || AssignmentRequired )
   {
    json_object * Payload = BuildSetPayload( Root, Set, ProfilesFile );
    SetString( Payload, "Device", WireDevice );
    Registry.AddPreparation( Device, Payload );
    json_object_object_add( Payload, "Action", json_object_new_string( "UseCentral" ) );
    StatusCode = 200;
    Response = JsonText( Payload, false );
    json_object_put( Payload );
    json_object_put( Root );
    json_object_put( Incoming );
    return true;
   }

  if( Set != nullptr && !AcceptProfiles && !ProfileDifference && !ChannelDifference &&
      !TimeDifference && !DeviceRevisionBehind )
   {
    json_object_object_add( Set, "Synced", json_object_new_boolean( true ) );
    SaveStore( FileName, Root );
   }

  StatusCode = 200;
  json_object * Reply = json_object_new_object();
  json_object_object_add( Reply, "Ok", json_object_new_boolean( true ) );
  SetString( Reply, "Action", "Synced" );
  SetString( Reply, "TimeStamp", TimeStamp );
  Registry.AddPreparation( Device, Reply );
  Response = JsonText( Reply, false );
  json_object_put( Reply );
  json_object_put( Root );
  json_object_put( Incoming );
  return true;
 }

bool TTimeStore::HttpPost( TEndpoint const & Endpoint, TGNString const & Path,
                           TGNString const & Body, unsigned & StatusCode, TGNString & Response ) const
 {
  addrinfo Hints{};
  Hints.ai_family = AF_UNSPEC;
  Hints.ai_socktype = SOCK_STREAM;
  addrinfo * Addresses = nullptr;
  const TGNString PortText = std::to_string( Endpoint.Port );
  if( getaddrinfo( Endpoint.Host.c_str(), PortText.c_str(), &Hints, &Addresses ) != 0 )
    return false;

  int Socket = -1;
  for( addrinfo * Address = Addresses; Address != nullptr; Address = Address->ai_next )
   {
    Socket = socket( Address->ai_family, Address->ai_socktype, Address->ai_protocol );
    if( Socket < 0 ) continue;
    timeval Timeout{};
    Timeout.tv_sec = 3;
    setsockopt( Socket, SOL_SOCKET, SO_RCVTIMEO, &Timeout, sizeof( Timeout ) );
    setsockopt( Socket, SOL_SOCKET, SO_SNDTIMEO, &Timeout, sizeof( Timeout ) );
    if( connect( Socket, Address->ai_addr, Address->ai_addrlen ) == 0 ) break;
    close( Socket );
    Socket = -1;
   }
  freeaddrinfo( Addresses );
  if( Socket < 0 )
    return false;

  std::ostringstream Header;
  Header << "POST " << Path << " HTTP/1.1\r\nHost: " << Endpoint.Host << ':' << Endpoint.Port
         << "\r\nContent-Type: application/json\r\nContent-Length: " << Body.size()
         << "\r\nConnection: close\r\n\r\n";
  const TGNString Request = Header.str() + Body;
  size_t Offset = 0;
  while( Offset < Request.size() )
   {
    const ssize_t Sent = send( Socket, Request.data() + Offset, Request.size() - Offset, MSG_NOSIGNAL );
    if( Sent <= 0 )
     {
      close( Socket );
      return false;
     }
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

bool TTimeStore::TryPushSet( TGNString const & Device, TGNString const & Type )
 {
  LoadEndpoints();
  auto EndpointIt = Endpoints.find( Device );
  if( EndpointIt == Endpoints.end() )
    return false;
  json_object * Root = LoadStore( FileName );
  if( Root == nullptr ) return false;
  json_object * Set = FindSet( Root, Device, Type );
  if( Set == nullptr )
   {
    json_object_put( Root );
    return false;
   }
  json_object * Payload = BuildSetPayload( Root, Set, ProfilesFile );
  SetString( Payload, "Device", Registry.WireId( Device ) );
  Registry.AddPreparation( Device, Payload );
  const TGNString Body = JsonText( Payload, false );
  json_object_put( Payload );

  unsigned Status = 0;
  TGNString Response;
  const bool Sent = HttpPost( EndpointIt->second, "/api/v1/times", Body, Status, Response );
  if( Sent && Status >= 200 && Status < 300 )
   {
    json_object * Reply = json_tokener_parse( Response.c_str() );
    if( Reply != nullptr && BoolValue( Reply, "Ok", false ) )
     {
      json_object_object_add( Set, "Synced", json_object_new_boolean( true ) );
      SaveStore( FileName, Root );
      json_object_put( Reply );
      json_object_put( Root );
      return true;
     }
    if( Reply != nullptr ) json_object_put( Reply );
   }
  json_object_put( Root );
  return false;
 }

void TTimeStore::TryPushUnsynced(void)
 {
  json_object * Root = LoadStore( FileName );
  if( Root == nullptr ) return;
  json_object * Sets = nullptr;
  json_object_object_get_ex( Root, "Sets", &Sets );
  std::vector<std::pair<TGNString,TGNString>> Pending;
  for( size_t Index = 0; Index < json_object_array_length( Sets ); ++Index )
   {
    json_object * Set = json_object_array_get_idx( Sets, Index );
    if( !BoolValue( Set, "Synced", false ) )
      Pending.emplace_back( StringValue( Set, "Device" ), StringValue( Set, "Type" ) );
   }
  json_object_put( Root );
  for( auto const & Item : Pending )
    TryPushSet( Item.first, Item.second );
 }

void TTimeStore::Tick(void)
 {
  const auto Now = std::chrono::steady_clock::now();
  if( Now < NextRetry ) return;
  NextRetry = Now + ( Registry.Pending() ? std::chrono::seconds( 3 ) : RetryInterval );
  TryPushUnsynced();
 }

void TTimeStore::MarkAllPending(void)
 {
  json_object * Root = LoadStore( FileName );
  if( Root == nullptr ) return;
  const TGNString Revision = NowTimeStamp();
  json_object * Sets = nullptr;
  json_object_object_get_ex( Root, "Sets", &Sets );
  for( size_t Index = 0; Index < json_object_array_length( Sets ); ++Index )
   {
    json_object * Set = json_object_array_get_idx( Sets, Index );
    if( StringValue( Set, "Type" ) != "SwitchTimes" ) continue;
    json_object_object_add( Set, "TimeStamp", json_object_new_string( Revision.c_str() ) );
    json_object_object_add( Set, "Synced", json_object_new_boolean( false ) );
   }
  json_object * ProfileSync = json_object_from_file( ( ProfilesFile.parent_path() / "HeatProfiles.sync.json" ).c_str() );
  if( ProfileSync == nullptr )
    ProfileSync = json_tokener_parse( "{\"Version\":1,\"Revision\":\"\",\"Sets\":[]}" );
  SetString( ProfileSync, "Revision", Revision );
  SaveStore( ProfilesFile.parent_path() / "HeatProfiles.sync.json", ProfileSync );
  json_object_put( ProfileSync );
  if( SaveStore( FileName, Root ) )
    TryPushUnsynced();
  json_object_put( Root );
 }

bool TTimeStore::PrepareAssignment(void)
 {
  json_object * Root = LoadStore( FileName );
  if( !Root ) return false;
  json_object * Sets = nullptr, * Channels = nullptr;
  json_object_object_get_ex( Root, "Sets", &Sets );
  json_object_object_get_ex( Root, "Channels", &Channels );
  json_object * Journal = json_object_from_file( ( RootDirectory / "data/DeviceAssignments.json" ).c_str() ), * Pending = nullptr;
  if( Journal ) json_object_object_get_ex( Journal, "PendingOperation", &Pending );
  if( StringValue( Pending, "Kind" ) == "EndpointSwap" )
   {
    json_object * Target = nullptr;
    if( !json_object_object_get_ex( Pending, "TimesTarget", &Target ) || !json_object_is_type( Target, json_type_object ) )
     { if( Journal ) json_object_put( Journal ); json_object_put( Root ); return false; }
    auto Committed = CloneJson( Target ); json_object_put( Root ); Root = Committed;
    json_object_object_get_ex( Root, "Sets", &Sets ); json_object_object_get_ex( Root, "Channels", &Channels );
   }
  if( StringValue( Pending, "Kind" ) == "EditDevices" )
   {
    auto CurrentSets = json_object_new_array();
    for( size_t I = 0; I < json_object_array_length( Sets ); ++I )
     {
      auto Set = json_object_array_get_idx( Sets, I );
      if( Registry.Devices().count( StringValue( Set, "Device" ) ) ) json_object_array_add( CurrentSets, json_object_get( Set ) );
     }
    json_object_object_add( Root, "Sets", CurrentSets ); Sets = CurrentSets;
    for( auto const & Item : Registry.Devices() )
     {
      const auto Service = Item.second.Id.back(); if( Service == '2' ) continue;
      EnsureSet( Root, Item.first, Service == '3' ? "FeedTimes" : "SwitchTimes", Service == '3' ? "Feeder" : Service == '1' ? "MyTimer" : "TmrSw" );
      if( Service != '3' && ChannelRowsFor( Channels, Item.first ) == nullptr )
       {
        if( Channels == nullptr ) { Channels = json_object_new_object(); json_object_object_add( Root, "Channels", Channels ); }
        auto State = json_object_new_object(); SetString( State, "TimeStamp", Registry.Revision() );
        json_object_object_add( State, "Rows", json_object_new_array() ); json_object_object_add( Channels, Item.first.c_str(), State );
       }
     }
    if( Channels )
     {
      std::vector<TGNString> Retired;
      json_object_object_foreach( Channels, Key, Value )
       { (void)Value; if( !Registry.Devices().count( Key ) ) Retired.emplace_back( Key ); }
      for( auto const & Key : Retired ) json_object_object_del( Channels, Key.c_str() );
     }
   }
  if( Journal ) json_object_put( Journal );
  for( size_t I = 0; I < json_object_array_length( Sets ); ++I )
   {
    json_object * Set = json_object_array_get_idx( Sets, I );
    SetString( Set, "TimeStamp", Registry.Revision() );
    json_object_object_add( Set, "Synced", json_object_new_boolean( false ) );
   }
  if( Channels )
   {
    json_object_object_foreach( Channels, Key, Value )
     {
      (void)Key;
      if( json_object_is_type( Value, json_type_object ) ) SetString( Value, "TimeStamp", Registry.Revision() );
     }
   }
  const bool Ok = SaveStore( FileName, Root );
  if( Ok ) NextRetry = {};
  json_object_put( Root );
  return Ok;
 }
