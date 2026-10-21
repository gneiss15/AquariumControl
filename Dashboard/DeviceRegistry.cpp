// DeviceRegistry.cpp, Version: 1.13
#include "DeviceRegistry.h"
#include <arpa/inet.h>
#include <regex>
#include <stdexcept>
#include <fstream>
#include <chrono>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace
 {
TGNString Text( json_object * Row, char const * Key )
 {
  json_object * Value = nullptr;
  return Row && json_object_object_get_ex( Row, Key, &Value ) &&
    json_object_is_type( Value, json_type_string ) ? json_object_get_string( Value ) : "";
 }
bool NormalizeServiceMetadata( json_object * Document )
 {
  json_object * Rows = nullptr; bool Changed = false;
  if( !json_object_object_get_ex( Document, "Devices", &Rows ) || !json_object_is_type( Rows, json_type_array ) ) return false;
  auto Normalize = [&]( json_object * Row )
   {
    json_object * Service = nullptr;
    if( json_object_object_get_ex( Row, "ServiceId", &Service ) && json_object_get_int( Service ) == 1 &&
        ( Text( Row, "DeviceType" ) == "MyTimer" || Text( Row, "DeviceType" ) == "MyTimerWeb" ) )
     { json_object_object_add( Row, "DeviceType", json_object_new_string( "MultiTimer" ) ); Changed = true; }
    if( json_object_object_get_ex( Row, "ServiceId", &Service ) && json_object_get_int( Service ) == 2 &&
        Text( Row, "Function" ) != "Dashboard" )
     { json_object_object_add( Row, "Function", json_object_new_string( "Dashboard" ) ); Changed = true; }
   };
  for( size_t I = 0; I < json_object_array_length( Rows ); ++I )
   {
    auto Row = json_object_array_get_idx( Rows, I ); Normalize( Row ); json_object * Services = nullptr;
    if( json_object_object_get_ex( Row, "AdditionalServices", &Services ) && json_object_is_type( Services, json_type_array ) )
      for( size_t J = 0; J < json_object_array_length( Services ); ++J ) Normalize( json_object_array_get_idx( Services, J ) );
   }
  return Changed;
 }
 }

TDeviceRegistry::TDeviceRegistry( std::filesystem::path const & Root )
: FRoot( Root )
 {
  const auto File = Root / "data" / "DeviceAssignments.json";
  if( !std::filesystem::exists( File ) ) throw std::runtime_error( "Device assignment table missing" );
  json_object * Document = json_object_from_file( File.c_str() );
  struct TRelease { json_object * P; ~TRelease(){ if( P ) json_object_put( P ); } } Release{ Document };
  json_object * Rows = nullptr;
  if( !Document || !json_object_object_get_ex( Document, "Devices", &Rows ) ||
      !json_object_is_type( Rows, json_type_array ) )
    throw std::runtime_error( "Invalid DeviceAssignments.json" );
  const TGNString State = Text( Document, "State" );
  json_object * Schema = nullptr;
  json_object_object_get_ex( Document, "Version", &Schema );
  const int SchemaVersion = Schema ? json_object_get_int( Schema ) : 0;
  if( !( ( SchemaVersion == 1 && State == "MAC" ) || ( SchemaVersion == 2 && State.empty() ) ) ) throw std::runtime_error( "MAC assignment table required" );
  NormalizeServiceMetadata( Document );
  FRevision = Text( Document, "Revision" );
  json_object * Pending = nullptr;
  FPending = json_object_object_get_ex( Document, "PendingOperation", &Pending ) && Pending != nullptr;
  if( FRevision.empty() ) throw std::runtime_error( "MAC registry revision missing" );
  // Hardware labels are static MAC metadata, independent of the live function journal.
  json_object * StoredConfiguration = nullptr;
  if( json_object_object_get_ex( Document, "EditorConfiguration", &StoredConfiguration ) && json_object_is_type( StoredConfiguration, json_type_object ) )
    FEditorConfiguration = json_object_to_json_string_ext( StoredConfiguration, JSON_C_TO_STRING_PLAIN );
  json_object * Configuration = FEditorConfiguration.empty() ? json_object_from_file( ( FRoot / "Devices.json" ).c_str() ) : json_tokener_parse( FEditorConfiguration.c_str() );
  TRelease ConfigurationRelease{ Configuration };
  json_object * HardwareNames = nullptr;
  if( Configuration ) json_object_object_get_ex( Configuration, "HardwareNames", &HardwareNames );
  const std::regex MacPattern( "([0-9A-F]{2}:){5}[0-9A-F]{2}" );
  auto Add = [&]( json_object * Row, TGNString const & Mac, TGNString const & IP )
   {
    json_object * Service = nullptr, * Port = nullptr;
    const TGNString FunctionId = Text( Row, "Function" );
    const TGNString Id = Text( Row, "DeviceId" );
    if( !json_object_object_get_ex( Row, "ServiceId", &Service ) ||
        !json_object_is_type( Service, json_type_int ) ||
        !json_object_object_get_ex( Row, "HttpPort", &Port ) ||
        !json_object_is_type( Port, json_type_int ) )
      throw std::runtime_error( "Registry service/port missing" );
    const int Number = json_object_get_int( Service ), PortNumber = json_object_get_int( Port );
    if( Number < 1 || Number > 4 || PortNumber < 1 || PortNumber > 65535 ||
        FunctionId.empty() || Id != Mac + "@" + std::to_string( Number ) || FDevices.count( FunctionId ) )
      throw std::runtime_error( "Invalid/duplicate registry identity" );
    FDevices.emplace( FunctionId, TDevice{ FunctionId, Id, IP, Text( Row, "DeviceType" ), Text( HardwareNames, Mac.c_str() ), unsigned( PortNumber ) } );
    for( auto const & Alias : { FunctionId, Id } )
     {
      auto It = FAliases.find( Alias );
      if( It != FAliases.end() && It->second != FunctionId )
        throw std::runtime_error( "Conflicting registry alias" );
      FAliases[Alias] = FunctionId;
     }
   };
  for( size_t Index = 0; Index < json_object_array_length( Rows ); ++Index )
   {
    json_object * Row = json_object_array_get_idx( Rows, Index );
    const TGNString Mac = Text( Row, "MAC" ), IP = Text( Row, "IP" );
    in_addr Address{};
    if( !std::regex_match( Mac, MacPattern ) || Mac == "00:00:00:00:00:00" ||
        inet_pton( AF_INET, IP.c_str(), &Address ) != 1 )
      throw std::runtime_error( "Invalid registry MAC/IP" );
    Add( Row, Mac, IP );
    json_object * Services = nullptr;
    if( json_object_object_get_ex( Row, "AdditionalServices", &Services ) )
     {
      if( !json_object_is_type( Services, json_type_array ) )
        throw std::runtime_error( "Invalid registry services" );
      for( size_t J = 0; J < json_object_array_length( Services ); ++J )
        Add( json_object_array_get_idx( Services, J ), Mac, IP );
     }
   }
  // External senders only publish UDP measurements; they are not managed devices.
  json_object * External = nullptr;
  if( json_object_object_get_ex( Document, "ExternalSources", &External ) )
   {
    if( !json_object_is_type( External, json_type_object ) )
      throw std::runtime_error( "Invalid external sources" );
    const std::regex SourceName( "[A-Za-z][A-Za-z0-9-]{0,62}" );
    json_object_object_foreach( External, Name, Value )
     {
      if( !json_object_is_type( Value, json_type_string ) )
        throw std::runtime_error( "Invalid external source identity" );
      const TGNString Id = json_object_get_string( Value );
      if( !std::regex_match( Name, SourceName ) || FDevices.count( Name ) ||
          Id.size() != 19 || Id.substr( 17 ) != "@3" ||
          !std::regex_match( Id.substr( 0, 17 ), MacPattern ) ||
          Id.substr( 0, 17 ) == "00:00:00:00:00:00" || FAliases.count( Id ) )
        throw std::runtime_error( "Invalid/conflicting external source" );
      FExternalSources.emplace( Name, Id );
     }
   }
 }

TGNString TDeviceRegistry::Function( TGNString const & Id ) const
 {
  auto It = FAliases.find( Id );
  return It == FAliases.end() ? Id : It->second;
 }
TGNString TDeviceRegistry::WireId( TGNString const & Id ) const
 {
  auto Device = FDevices.find( Function( Id ) );
  return Device == FDevices.end() ? TGNString{} : Device->second.Id;
 }
TGNString TDeviceRegistry::HostnameFor( TGNString const & DeviceId, TGNString const & NewFunction ) const
 {
  auto Device = FDevices.find( Function( DeviceId ) );
  if( Device == FDevices.end() ) return {};
  TGNString Hostname = NewFunction;
  json_object * Configuration = FEditorConfiguration.empty() ? json_object_from_file( ( FRoot / "Devices.json" ).c_str() ) : json_tokener_parse( FEditorConfiguration.c_str() );
  json_object * Rows = nullptr;
  if( Configuration && json_object_object_get_ex( Configuration, "Devices", &Rows ) && json_object_is_type( Rows, json_type_array ) )
    for( size_t I = 0; I < json_object_array_length( Rows ); ++I )
     {
      auto Row = json_object_array_get_idx( Rows, I );
      json_object * Hardware = nullptr;
      if( Text( Row, "Name" ) == NewFunction && json_object_object_get_ex( Row, "UseHardwareName", &Hardware ) &&
          json_object_is_type( Hardware, json_type_boolean ) && json_object_get_boolean( Hardware ) )
        Hostname = Device->second.HardwareName;
     }
  if( Configuration ) json_object_put( Configuration );
  // Do not emit an invalid network name, or a reserve name without known hardware.
  if( Hostname.size() > 63 || !std::regex_match( Hostname, std::regex( "[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?" ) ) ) return {};
  return Hostname;
 }

bool TDeviceRegistry::ValidateIdentity( TGNString const & FunctionId, json_object * Envelope ) const
 {
  json_object * Identity = nullptr, * Version = nullptr;
  if( !json_object_object_get_ex( Envelope, "Identity", &Identity ) ) return false;
  auto It = FDevices.find( Function( FunctionId ) );
  return It != FDevices.end() && json_object_is_type( Identity, json_type_object ) &&
    json_object_object_get_ex( Identity, "Version", &Version ) && json_object_is_type( Version, json_type_int ) &&
    ( json_object_get_int( Version ) >= 1 && json_object_get_int( Version ) <= 3 ) && Text( Identity, "DeviceId" ) == It->second.Id &&
    Text( Identity, "Mode" ) == "MAC";
 }

void TDeviceRegistry::AddPreparation( TGNString const & FunctionId, json_object * Envelope ) const
 {
  auto It = FDevices.find( Function( FunctionId ) );
  if( It == FDevices.end() ) return;
  json_object * Identity = json_object_new_object(), * Sources = json_object_new_object();
  json_object_object_add( Identity, "Version", json_object_new_int( 1 ) );
  json_object_object_add( Identity, "DeviceId", json_object_new_string( It->second.Id.c_str() ) );
  json_object_object_add( Identity, "Mode", json_object_new_string( "MAC" ) );
  json_object_object_add( Identity, "Function", json_object_new_string( It->second.Function.c_str() ) );
  json_object_object_add( Identity, "Revision", json_object_new_string( FRevision.c_str() ) );
  // Small complete temperature-source table for both heater consumer classes.
  // Missing hardware (FeederAq3) is excluded by the registry loader.
  if( It->second.Id.back() == '1' || It->second.Id.back() == '4' )
    for( auto const & Item : FDevices )
      if( Item.second.Id.back() == '3' )
        json_object_object_add( Sources, Item.first.c_str(), json_object_new_string( Item.second.Id.c_str() ) );
  if( It->second.Id.back() == '1' || It->second.Id.back() == '4' )
   {
    auto Journal = json_object_from_file( ( FRoot / "data/DeviceAssignments.json" ).c_str() ); json_object * Endpoints = nullptr;
    if( Journal && json_object_object_get_ex( Journal, "Endpoints", &Endpoints ) )
      for( size_t I = 0; I < json_object_array_length( Endpoints ); ++I )
       {
        auto Row = json_object_array_get_idx( Endpoints, I ); json_object * Capability = nullptr;
        if( json_object_object_get_ex( Row, "Capabilities", &Capability ) && json_object_get_int( Capability ) == 2 )
          json_object_object_add( Sources, Text( Row, "Function" ).c_str(), json_object_new_string( Text( Row, "DeviceId" ).c_str() ) );
       }
    if( Journal ) json_object_put( Journal );
   }
  if( It->second.Id.back() == '1' || It->second.Id.back() == '4' )
    for( auto const & Source : FExternalSources )
      json_object_object_add( Sources, Source.first.c_str(), json_object_new_string( Source.second.c_str() ) );
  json_object_object_add( Identity, "Sources", Sources );
  json_object_object_add( Envelope, "Identity", Identity );
  // Transfer data belongs only to the two swapped devices, not the other consumers.
  if( FPending )
   {
    json_object * Document = json_object_from_file( ( FRoot / "data" / "DeviceAssignments.json" ).c_str() );
    json_object * Pending = nullptr, * Transfer = nullptr, * Target = nullptr, * Config = nullptr;
    if( Document && json_object_object_get_ex( Document, "PendingOperation", &Pending ) &&
        json_object_object_get_ex( Pending, "Transfer", &Transfer ) &&
        json_object_object_get_ex( Transfer, It->second.Id.c_str(), &Target ) &&
        json_object_object_get_ex( Target, "FunctionConfig", &Config ) )
      json_object_object_add( Envelope, "FunctionConfig", json_object_get( Config ) );
    if( Document ) json_object_put( Document );
   }
 }

void TDeviceRegistry::ApplyAddresses( json_object * Configuration ) const
 {
  if( !FEditorConfiguration.empty() )
   {
    json_object * Stored = json_tokener_parse( FEditorConfiguration.c_str() );
    if( Stored )
     {
      json_object_object_foreach( Stored, Key, Value )
        json_object_object_add( Configuration, Key, json_object_get( Value ) );
      json_object_put( Stored );
     }
   }
  for( char const * List : { "Devices", "SyncOnlyDevices" } )
   {
    json_object * Rows = nullptr;
    if( !json_object_object_get_ex( Configuration, List, &Rows ) || !json_object_is_type( Rows, json_type_array ) ) continue;
    for( size_t I = 0; I < json_object_array_length( Rows ); ++I )
     {
      json_object * Row = json_object_array_get_idx( Rows, I );
      TGNString Name = Text( Row, "Name" );
      if( Name == "MultiTimer Aq1 & 2" ) Name = "GN-Pi";
      auto It = FDevices.find( Function( Name ) );
      if( It == FDevices.end() ) continue;
      auto const & Device = It->second;
      const auto Url = "http://" + Device.IP + ( Device.Port == 80 ? "" : ":" + std::to_string( Device.Port ) );
      json_object_object_add( Row, "BaseUrl", json_object_new_string( Url.c_str() ) );
      json_object_object_add( Row, "Function", json_object_new_string( Device.Function.c_str() ) );
      json_object_object_add( Row, "DeviceId", json_object_new_string( Device.Id.c_str() ) );
    json_object_object_add( Row, "DeviceType", json_object_new_string( Device.DeviceType.c_str() ) );
    json_object_object_add( Row, "HardwareName", json_object_new_string( Device.HardwareName.c_str() ) );
      auto Journal = json_object_from_file( ( FRoot / "data/DeviceAssignments.json" ).c_str() ); json_object * Endpoints = nullptr;
      if( Journal && json_object_object_get_ex( Journal, "Endpoints", &Endpoints ) && ( Device.Id.back() == '1' || Device.Id.back() == '4' ) )
       {
        std::map<TGNString,json_object *> Groups;
        auto HardwareLabels = json_object_new_object();
        for( size_t E = 0; E < json_object_array_length( Endpoints ); ++E )
         {
          auto Endpoint = json_object_array_get_idx( Endpoints, E );
          if( Text( Endpoint, "DeviceId" ) != Device.Id ) continue;
          json_object * FunctionGroups = nullptr; json_object_object_get_ex( Endpoint, "Groups", &FunctionGroups );
          for( size_t G = 0; G < json_object_array_length( FunctionGroups ); ++G )
           {
            const TGNString Group = json_object_get_string( json_object_array_get_idx( FunctionGroups, G ) );
            if( !Groups.count( Group ) ) Groups[Group] = json_object_new_array();
            json_object_array_add( Groups[Group], json_object_new_string( Text( Endpoint, "Function" ).c_str() ) );
           }
          json_object * HardwareLabel = nullptr;
          json_object_object_get_ex( Endpoint, "UseHardwareName", &HardwareLabel );
          if( json_object_get_boolean( HardwareLabel ) ) json_object_object_add( HardwareLabels, Text( Endpoint, "Function" ).c_str(), json_object_new_string( Text( Endpoint, "HardwareName" ).c_str() ) );
          if( Device.Id.back() == '4' )
           {
            json_object_object_add( Row, "EndpointFunction", json_object_new_string( Text( Endpoint, "Function" ).c_str() ) );
            const bool Reserve = json_object_get_boolean( HardwareLabel );
            json_object_object_add( Row, "UseHardwareName", json_object_new_boolean( Reserve ) );
            json_object_object_add( Row, "PowerLabel", json_object_new_string( "Power" ) );
           }
         }
        json_object_object_add( Row, "EndpointHardwareLabels", HardwareLabels );
        if( !Groups.empty() )
         {
          auto Views = json_object_new_array();
          for( auto const & Group : Groups )
           {
            auto View = json_object_new_object(); json_object_object_add( View, "Group", json_object_new_string( Group.first.c_str() ) );
            json_object_object_add( View, "Channels", Group.second ); json_object_array_add( Views, View );
           }
          json_object_object_add( Row, "GroupViews", Views );
         }
       }
      if( Device.Id.back() == '3' && json_object_is_type( Endpoints, json_type_array ) )
       {
        for( size_t E = 0; E < json_object_array_length( Endpoints ); ++E )
         {
          auto Endpoint = json_object_array_get_idx( Endpoints, E );
          if( Text( Endpoint, "DeviceId" ) != Device.Id ) continue;
          json_object * SensorGroups = nullptr; json_object_object_get_ex( Endpoint, "Groups", &SensorGroups );
          const auto FeedingGroup = Text( Row, "Group" ); bool SharedGroup = false;
          for( size_t G = 0; G < json_object_array_length( SensorGroups ); ++G )
            SharedGroup = SharedGroup || FeedingGroup == json_object_get_string( json_object_array_get_idx( SensorGroups, G ) );
          auto Views = json_object_new_array(), FeedingView = json_object_new_object(), Measurements = json_object_new_array();
          json_object_object_add( FeedingView, "Group", json_object_new_string( FeedingGroup.c_str() ) );
          if( SharedGroup ) json_object_array_add( Measurements, json_object_new_string( "Temperature" ) );
          json_object_object_add( FeedingView, "Measurements", Measurements ); json_object_array_add( Views, FeedingView );
          for( size_t G = 0; G < json_object_array_length( SensorGroups ); ++G )
           {
            const TGNString Group = json_object_get_string( json_object_array_get_idx( SensorGroups, G ) );
            if( Group == FeedingGroup ) continue;
            auto View = json_object_new_object(), SensorMeasurements = json_object_new_array();
            json_object_object_add( View, "Group", json_object_new_string( Group.c_str() ) );
            json_object_object_add( View, "StatusFunction", json_object_new_string( Text( Endpoint, "Function" ).c_str() ) );
            json_object_object_add( View, "TemperatureOnly", json_object_new_boolean( true ) );
            json_object_array_add( SensorMeasurements, json_object_new_string( "Temperature" ) );
            json_object_object_add( View, "Measurements", SensorMeasurements );
            json_object_object_add( View, "Channels", json_object_new_array() ); json_object_array_add( Views, View );
           }
          json_object_object_add( Row, "GroupViews", Views );
         }
       }
      if( Journal ) json_object_put( Journal );
     }
   }
 }

bool TDeviceRegistry::AcceptWireId( TGNString const & Id ) const
 {
  for( auto const & Item : FDevices ) if( Item.second.Id == Id ) return true;
  return false;
 }

bool TDeviceRegistry::NeedsAssignment( TGNString const & FunctionId, json_object * Envelope ) const
 {
  if( FPending ) return true;
  json_object * Identity = nullptr;
  auto It = FDevices.find( Function( FunctionId ) );
  if( It == FDevices.end() ) return true;
  return !json_object_object_get_ex( Envelope, "Identity", &Identity ) ||
    Text( Identity, "AppliedRevision" ) != FRevision || Text( Identity, "Function" ) != It->second.Function ||
    Text( Identity, "Mode" ) != "MAC" ||
    ( json_object_object_get_ex( Identity, "LocalSources", &Identity ) && json_object_get_boolean( Identity ) );
 }

bool TDeviceRegistry::Save( json_object * Document, TGNString & Error )
 {
  const auto Target = FRoot / "data" / "DeviceAssignments.json", Temporary = FRoot / "data" / "DeviceAssignments.json.new";
  json_object * DeviceRows = nullptr;
  if( json_object_object_get_ex( Document, "Devices", &DeviceRows ) )
    for( size_t I = 0; I < json_object_array_length( DeviceRows ); ++I )
      json_object_object_del( json_object_array_get_idx( DeviceRows, I ), "LegacyDeviceId" );
  NormalizeServiceMetadata( Document );
  json_object_object_del( Document, "State" );
  json_object_object_add( Document, "Version", json_object_new_int( 2 ) );
  const TGNString Text = TGNString( json_object_to_json_string_ext( Document, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE ) ) + "\n";
  const int File = open( Temporary.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600 );
  if( File < 0 ) { Error = "Cannot write assignment journal"; return false; }
  if( fchmod( File, 0600 ) != 0 ) { close( File ); Error = "Cannot protect assignment journal"; return false; }
  size_t Offset = 0;
  while( Offset < Text.length() )
   {
    const ssize_t Written = write( File, Text.data() + Offset, Text.length() - Offset );
    if( Written <= 0 ) { close( File ); Error = "Cannot write assignment journal"; return false; }
    Offset += static_cast<size_t>( Written );
   }
  const bool Synced = fsync( File ) == 0;
  const bool Closed = close( File ) == 0;
  if( !Synced || !Closed || rename( Temporary.c_str(), Target.c_str() ) != 0 )
   { Error = "Cannot commit assignment journal"; return false; }
  const int Directory = open( ( FRoot / "data" ).c_str(), O_RDONLY | O_DIRECTORY );
  const bool DirectorySynced = Directory >= 0 && fsync( Directory ) == 0;
  if( Directory >= 0 ) close( Directory );
  // Rebuild aliases/endpoints only after the authoritative atomic replacement.
  *this = TDeviceRegistry( FRoot );
  if( !DirectorySynced ) Error = "Assignment committed; directory sync failed";
  return DirectorySynced;
 }

bool TDeviceRegistry::Change( TGNString const & First, TGNString const & Second, json_object * Transfer, TGNString & Error )
 {
  if( FPending ) { Error = "Previous operation still pending"; return false; }
  const bool Swap = true;
  auto A = FDevices.find( First ), B = FDevices.find( Second );
  if( Swap && ( A == FDevices.end() || B == FDevices.end() || A == B ||
       ( A->second.Id.back() != '3' && A->second.Id.back() != '4' ) || A->second.Id.back() != B->second.Id.back() ) )
   { Error = "Only Feeder/Feeder or TimerSw/TimerSw swaps are allowed in MAC mode"; return false; }
  if( !Transfer || !json_object_is_type( Transfer, json_type_object ) ) { Error = "Function config transfer missing"; return false; }
  json_object * Document = json_object_from_file( ( FRoot / "data" / "DeviceAssignments.json" ).c_str() );
  if( !Document ) { Error = "Assignment table missing"; return false; }
  json_object * Endpoints = nullptr;
  if( json_object_object_get_ex( Document, "Endpoints", &Endpoints ) && json_object_is_type( Endpoints, json_type_array ) )
   {
    if( A->second.Id.back() == '4' )
     { json_object_put( Document ); Error = "Use the hardware endpoint swap for switches"; return false; }
    json_object * FirstSensor = nullptr, * SecondSensor = nullptr;
    for( size_t I = 0; I < json_object_array_length( Endpoints ); ++I )
     {
      auto Row = json_object_array_get_idx( Endpoints, I );
      if( Text( Row, "DeviceId" ) == A->second.Id ) FirstSensor = Row;
      if( Text( Row, "DeviceId" ) == B->second.Id ) SecondSensor = Row;
     }
    if( FirstSensor && SecondSensor )
     {
      const auto FirstFunction = Text( FirstSensor, "Function" ), SecondFunction = Text( SecondSensor, "Function" );
      json_object * FirstGroups = nullptr, * SecondGroups = nullptr;
      json_object_object_get_ex( FirstSensor, "Groups", &FirstGroups ); json_object_object_get_ex( SecondSensor, "Groups", &SecondGroups );
      if( FirstGroups ) json_object_get( FirstGroups );
      if( SecondGroups ) json_object_get( SecondGroups );
      json_object_object_add( FirstSensor, "Function", json_object_new_string( SecondFunction.c_str() ) );
      json_object_object_add( SecondSensor, "Function", json_object_new_string( FirstFunction.c_str() ) );
      json_object_object_add( FirstSensor, "Device", json_object_new_string( Second.c_str() ) );
      json_object_object_add( SecondSensor, "Device", json_object_new_string( First.c_str() ) );
      if( SecondGroups ) json_object_object_add( FirstSensor, "Groups", SecondGroups );
      if( FirstGroups ) json_object_object_add( SecondSensor, "Groups", FirstGroups );
     }
   }
  if( Swap )
   {
    json_object * Rows = nullptr;
    json_object_object_get_ex( Document, "Devices", &Rows );
    TGNString FirstGroup, SecondGroup;
    for( size_t I = 0; I < json_object_array_length( Rows ); ++I )
     {
      auto Row = json_object_array_get_idx( Rows, I );
      if( Text( Row, "Function" ) == First ) FirstGroup = Text( Row, "Group" );
      if( Text( Row, "Function" ) == Second ) SecondGroup = Text( Row, "Group" );
     }
    for( size_t I = 0; I < json_object_array_length( Rows ); ++I )
     {
      json_object * Row = json_object_array_get_idx( Rows, I );
      const auto FunctionId = Text( Row, "Function" );
      if( FunctionId == First || FunctionId == Second )
       {
        json_object_object_add( Row, "Function", json_object_new_string( FunctionId == First ? Second.c_str() : First.c_str() ) );
        const auto Group = FunctionId == First ? SecondGroup : FirstGroup;
        if( !Group.empty() ) json_object_object_add( Row, "Group", json_object_new_string( Group.c_str() ) );
       }
     }
   }
  const auto Now = std::chrono::system_clock::now();
  const time_t Seconds = std::chrono::system_clock::to_time_t( Now );
  tm Utc{}; gmtime_r( &Seconds, &Utc );
  char Stamp[40];
  strftime( Stamp, sizeof( Stamp ), "%Y-%m-%dT%H:%M:%S", &Utc );
  char Fraction[10];
  snprintf( Fraction, sizeof( Fraction ), ".%03dZ", int( std::chrono::duration_cast<std::chrono::milliseconds>( Now.time_since_epoch() ).count() % 1000 ) );
  const TGNString Revision = TGNString( Stamp ) + Fraction;
  if( !FRevision.empty() && Revision <= FRevision )
   { json_object_put( Document ); Error = "Wait for a newer server timestamp"; return false; }
  json_object_object_add( Document, "State", json_object_new_string( "MAC" ) );
  json_object_object_add( Document, "Revision", json_object_new_string( Revision.c_str() ) );
  json_object * Pending = json_object_new_object();
  json_object_object_add( Pending, "Kind", json_object_new_string( "Swap" ) );
  json_object_object_add( Pending, "First", json_object_new_string( First.c_str() ) );
  json_object_object_add( Pending, "Second", json_object_new_string( Second.c_str() ) );
  json_object_object_add( Pending, "Transfer", json_object_get( Transfer ) );
  json_object_object_add( Document, "PendingOperation", Pending );
  const bool Ok = Save( Document, Error );
  json_object_put( Document );
  return Ok;
 }

bool TDeviceRegistry::InitializeEndpoints( json_object * Endpoints, TGNString & Error )
 {
  auto Document = json_object_from_file( ( FRoot / "data/DeviceAssignments.json" ).c_str() );
  if( !Document ) { Error = "Cannot read assignment journal"; return false; }
  json_object * Stored = nullptr, * Version = nullptr;
  json_object_object_get_ex( Document, "Version", &Version );
  const bool MetadataChanged = NormalizeServiceMetadata( Document );
  if( !MetadataChanged && json_object_get_int( Version ) == 2 && json_object_object_get_ex( Document, "Endpoints", &Stored ) &&
      json_object_equal( Stored, Endpoints ) ) { json_object_put( Document ); return true; }
  // Metadata migration preserves the revision and any pending distribution journal.
  json_object_object_add( Document, "Endpoints", json_object_get( Endpoints ) );
  const bool Result = Save( Document, Error );
  json_object_put( Document ); return Result;
 }

bool TDeviceRegistry::ChangeEndpoints( json_object * Endpoints, json_object * Times, TGNString & Error )
 {
  if( FPending ) { Error = "Previous operation still pending"; return false; }
  auto Document = json_object_from_file( ( FRoot / "data/DeviceAssignments.json" ).c_str() );
  if( !Document || Text( Document, "Revision" ) != FRevision )
   { if( Document ) json_object_put( Document ); Error = "Stale assignment journal"; return false; }
  const auto Now = std::chrono::system_clock::now();
  const auto Seconds = std::chrono::system_clock::to_time_t( Now ); tm Utc{}; gmtime_r( &Seconds, &Utc );
  char Stamp[40], Fraction[12]; strftime( Stamp, sizeof( Stamp ), "%Y-%m-%dT%H:%M:%S", &Utc );
  snprintf( Fraction, sizeof( Fraction ), ".%03dZ", int( std::chrono::duration_cast<std::chrono::milliseconds>( Now.time_since_epoch() ).count() % 1000 ) );
  const TGNString Revision = TGNString( Stamp ) + Fraction;
  if( Revision <= FRevision ) { json_object_put( Document ); Error = "Wait for newer timestamp"; return false; }
  json_object_object_add( Document, "Revision", json_object_new_string( Revision.c_str() ) );
  json_object_object_add( Document, "Endpoints", json_object_get( Endpoints ) );
  auto Pending = json_object_new_object();
  json_object_object_add( Pending, "Kind", json_object_new_string( "EndpointSwap" ) );
  json_object_object_add( Pending, "Transfer", json_object_new_object() );
  json_object_object_add( Pending, "TimesTarget", json_object_get( Times ) );
  json_object_object_add( Document, "PendingOperation", Pending );
  const bool Ok = Save( Document, Error ); json_object_put( Document ); return Ok;
 }

bool TDeviceRegistry::Complete( TGNString & Error )
 {
  if( !FPending ) return true;
  json_object * Document = json_object_from_file( ( FRoot / "data" / "DeviceAssignments.json" ).c_str() );
  if( !Document ) { Error = "Assignment table missing"; return false; }
  json_object_object_del( Document, "PendingOperation" );
  const bool Ok = Save( Document, Error );
  json_object_put( Document );
  return Ok;
 }

json_object * TDeviceRegistry::View(void) const
 {
  json_object * Root = json_object_new_object(), * Rows = json_object_new_array();
  json_object_object_add( Root, "Mode", json_object_new_string( "MAC" ) );
  json_object_object_add( Root, "Revision", json_object_new_string( FRevision.c_str() ) );
  json_object_object_add( Root, "Pending", json_object_new_boolean( FPending ) );
  for( auto const & Item : FDevices )
   {
    auto const & Device = Item.second;
    json_object * Row = json_object_new_object();
    json_object_object_add( Row, "Function", json_object_new_string( Device.Function.c_str() ) );
    json_object_object_add( Row, "DeviceId", json_object_new_string( Device.Id.c_str() ) );
    json_object_object_add( Row, "DeviceType", json_object_new_string( Device.DeviceType.c_str() ) );
    json_object_object_add( Row, "HardwareName", json_object_new_string( Device.HardwareName.c_str() ) );
    json_object_object_add( Row, "IP", json_object_new_string( Device.IP.c_str() ) );
    json_object_object_add( Row, "HttpPort", json_object_new_int( Device.Port ) );
    json_object_object_add( Row, "ServiceId", json_object_new_int( Device.Id.back() - '0' ) );
    json_object_object_add( Row, "CapabilityId", json_object_new_int( Device.Id.back() == '2' ? 0 : Device.Id.back() == '3' ? 2 : 1 ) );
    json_object_array_add( Rows, Row );
   }
  json_object_object_add( Root, "Devices", Rows );
  auto External = json_object_new_object();
  for( auto const & Source : FExternalSources )
    json_object_object_add( External, Source.first.c_str(), json_object_new_string( Source.second.c_str() ) );
  json_object_object_add( Root, "ExternalSources", External );
  auto Document = json_object_from_file( ( FRoot / "data/DeviceAssignments.json" ).c_str() ); json_object * Endpoints = nullptr;
  if( Document && json_object_object_get_ex( Document, "Endpoints", &Endpoints ) ) json_object_object_add( Root, "Endpoints", json_object_get( Endpoints ) );
  if( Document ) json_object_put( Document );
  return Root;
 }

bool TDeviceRegistry::RestartConfirmed( TGNString const & FunctionId, json_object * Identity ) const
 {
  if( !FPending ) return true;
  auto It = FDevices.find( FunctionId );
  if( It == FDevices.end() ) return false;
  json_object * Document = json_object_from_file( ( FRoot / "data" / "DeviceAssignments.json" ).c_str() );
  json_object * Pending = nullptr, * Transfer = nullptr, * Target = nullptr, * Previous = nullptr, * Current = nullptr;
  bool Ok = Document && json_object_object_get_ex( Document, "PendingOperation", &Pending ) &&
    json_object_object_get_ex( Pending, "Transfer", &Transfer );
  if( Ok && json_object_object_get_ex( Transfer, It->second.Id.c_str(), &Target ) )
    Ok = json_object_object_get_ex( Target, "BootCounter", &Previous ) && json_object_is_type( Previous, json_type_int ) &&
      json_object_object_get_ex( Identity, "BootCounter", &Current ) && json_object_is_type( Current, json_type_int ) &&
      json_object_get_int64( Current ) > json_object_get_int64( Previous );
  if( Document ) json_object_put( Document );
  return Ok;
 }
