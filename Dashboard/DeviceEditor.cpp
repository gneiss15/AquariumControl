// DeviceEditor.cpp, Version: 1.02
#include "DeviceRegistry.h"
#include <arpa/inet.h>
#include <set>
#include <regex>
#include <chrono>
#include <ctime>

namespace
 {
struct TJson { json_object * P; ~TJson(){ if( P ) json_object_put( P ); } };
json_object * Member( json_object * Root, char const * Key )
 { json_object * Value = nullptr; if( Root ) json_object_object_get_ex( Root, Key, &Value ); return Value; }
TGNString Text( json_object * Root, char const * Key )
 { auto Value = Member( Root, Key ); return Value && json_object_is_type( Value, json_type_string ) ? json_object_get_string( Value ) : ""; }
void String( json_object * Root, char const * Key, TGNString const & Value )
 { json_object_object_add( Root, Key, json_object_new_string( Value.c_str() ) ); }
json_object * Clone( json_object * Root )
 { return Root ? json_tokener_parse( json_object_to_json_string_ext( Root, JSON_C_TO_STRING_PLAIN ) ) : json_object_new_object(); }
TGNString ConfigFunction( json_object * Row )
 { const auto Name = Text( Row, "Name" ); return Name == "MultiTimer Aq1 & 2" ? "GN-Pi" : Name; }
json_object * Find( json_object * Rows, char const * Key, TGNString const & Value )
 {
  if( !Rows || !json_object_is_type( Rows, json_type_array ) ) return nullptr;
  for( size_t I = 0; I < json_object_array_length( Rows ); ++I )
   { auto Row = json_object_array_get_idx( Rows, I ); if( Text( Row, Key ) == Value ) return Row; }
  return nullptr;
 }
json_object * Display( json_object * Configuration, TGNString const & Function )
 {
  auto Rows = Member( Configuration, "Devices" );
  for( size_t I = 0; Rows && I < json_object_array_length( Rows ); ++I )
   { auto Row = json_object_array_get_idx( Rows, I ); if( ConfigFunction( Row ) == Function ) return Row; }
  return nullptr;
 }
TGNString RevisionNow(void)
 {
  const auto Now = std::chrono::system_clock::now();
  const time_t Seconds = std::chrono::system_clock::to_time_t( Now );
  tm Utc{}; gmtime_r( &Seconds, &Utc ); char Stamp[40], Fraction[10];
  strftime( Stamp, sizeof( Stamp ), "%Y-%m-%dT%H:%M:%S", &Utc );
  snprintf( Fraction, sizeof( Fraction ), ".%03dZ", int( std::chrono::duration_cast<std::chrono::milliseconds>( Now.time_since_epoch() ).count() % 1000 ) );
  return TGNString( Stamp ) + Fraction;
 }
 }

json_object * TDeviceRegistry::EditorView(void) const
 {
  TJson Configuration{ FEditorConfiguration.empty() ? json_object_from_file( ( FRoot / "Devices.json" ).c_str() ) : json_tokener_parse( FEditorConfiguration.c_str() ) };
  TJson Document{ json_object_from_file( ( FRoot / "data/DeviceAssignments.json" ).c_str() ) };
  json_object * Root = json_object_new_object(), * Rows = json_object_new_array();
  json_object_object_add( Root, "Version", json_object_new_int( 1 ) ); String( Root, "Revision", FRevision );
  json_object_object_add( Root, "Devices", Rows );
  for( auto const & Item : FDevices )
   {
    auto const & Device = Item.second;
    auto View = Display( Configuration.P, Device.Function );
    auto Row = json_object_new_object();
    String( Row, "Function", Device.Function ); String( Row, "MAC", Device.Id.substr( 0, 17 ) ); String( Row, "IP", Device.IP );
    json_object_object_add( Row, "HttpPort", json_object_new_int( Device.Port ) );
    json_object_object_add( Row, "ServiceId", json_object_new_int( Device.Id.back() - '0' ) );
    String( Row, "DeviceType", Device.DeviceType ); String( Row, "HardwareName", Device.HardwareName );
    auto Group = Text( View, "Group" ); String( Row, "Group", Group.empty() ? "System" : Group );
    json_object_object_add( Row, "UseHardwareName", json_object_new_boolean( json_object_get_boolean( Member( View, "UseHardwareName" ) ) ) );
    json_object_object_add( Row, "Active", json_object_new_boolean( true ) ); json_object_array_add( Rows, Row );
   }
  auto Inactive = Member( Document.P, "Unresolved" );
  for( size_t I = 0; Inactive && I < json_object_array_length( Inactive ); ++I )
   {
    auto Old = json_object_array_get_idx( Inactive, I ), Row = json_object_new_object();
    String( Row, "Function", Text( Old, "Function" ) ); String( Row, "MAC", Text( Old, "MAC" ) ); String( Row, "IP", Text( Old, "IP" ) );
    json_object_object_add( Row, "HttpPort", json_object_new_int( 80 ) );
    json_object_object_add( Row, "ServiceId", json_object_new_int( json_object_get_int( Member( Old, "ServiceId" ) ) ) );
    auto Type = Text( Old, "DeviceType" ); if( Type.empty() ) Type = Text( Old, "PlannedProject" ); String( Row, "DeviceType", Type );
    auto Hardware = Text( Old, "HardwareName" ); if( Hardware.empty() ) Hardware = Text( Member( Configuration.P, "HardwareNames" ), Text( Old, "MAC" ).c_str() );
    auto View = Display( Configuration.P, Text( Old, "Function" ) );
    auto Group = Text( Old, "Group" ); if( Group.empty() ) Group = Text( View, "Group" );
    String( Row, "HardwareName", Hardware ); String( Row, "Group", Group.empty() ? "System" : Group );
    json_object_object_add( Row, "UseHardwareName", json_object_new_boolean( json_object_get_boolean( Member( Old, "UseHardwareName" ) ) || json_object_get_boolean( Member( View, "UseHardwareName" ) ) ) );
    json_object_object_add( Row, "Active", json_object_new_boolean( false ) ); json_object_array_add( Rows, Row );
   }
  return Root;
 }

bool TDeviceRegistry::EditDevices( json_object * Rows, json_object * Transfer, TGNString & Error )
 {
  auto Fail = [&]( TGNString const & Message ){ Error = Message; return false; };
  if( FPending ) return Fail( "Assignment distribution pending" );
  if( !Rows || !json_object_is_type( Rows, json_type_array ) || json_object_array_length( Rows ) == 0 || json_object_array_length( Rows ) > 64 ) return Fail( "Invalid device list" );
  TJson Previous{ EditorView() }, Document{ json_object_from_file( ( FRoot / "data/DeviceAssignments.json" ).c_str() ) };
  TJson Configuration{ FEditorConfiguration.empty() ? json_object_from_file( ( FRoot / "Devices.json" ).c_str() ) : json_tokener_parse( FEditorConfiguration.c_str() ) };
  if( !Document.P || !Configuration.P ) return Fail( "Cannot load device configuration" );
  if( Text( Document.P, "Revision" ) != FRevision ) return Fail( "Assignment file changed; reload first" );
  if( json_object_equal( Rows, Member( Previous.P, "Devices" ) ) ) return true;
  const std::regex MacPattern( "([0-9A-F]{2}:){5}[0-9A-F]{2}" ), NamePattern( "[A-Za-z]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?" );
  std::set<TGNString> Functions, Ids, Addresses, HardwareNames, PhysicalMacs;
  std::map<TGNString,json_object *> ById;
  std::map<TGNString,int> MacServices;
  for( size_t I = 0; I < json_object_array_length( Rows ); ++I )
   {
    auto Row = json_object_array_get_idx( Rows, I );
    if( !json_object_is_type( Row, json_type_object ) || json_object_object_length( Row ) != 10 ) return Fail( "Each device needs the ten displayed fields" );
    for( auto Key : { "Function", "MAC", "IP", "DeviceType", "HardwareName", "Group" } )
      if( !json_object_is_type( Member( Row, Key ), json_type_string ) ) return Fail( "Invalid string field" );
    for( auto Key : { "Active", "UseHardwareName" } )
      if( !json_object_is_type( Member( Row, Key ), json_type_boolean ) ) return Fail( "Invalid boolean field" );
    for( auto Key : { "HttpPort", "ServiceId" } )
      if( !json_object_is_type( Member( Row, Key ), json_type_int ) ) return Fail( "Invalid integer field" );
    const auto FunctionId = Text( Row, "Function" ), Mac = Text( Row, "MAC" ), IP = Text( Row, "IP" ), Hardware = Text( Row, "HardwareName" );
    const int Service = json_object_get_int( Member( Row, "ServiceId" ) ), Port = json_object_get_int( Member( Row, "HttpPort" ) );
    const bool Active = json_object_get_boolean( Member( Row, "Active" ) );
    const auto Id = Mac + "@" + std::to_string( Service ); in_addr Address{};
    if( !std::regex_match( FunctionId, NamePattern ) || !std::regex_match( Mac, MacPattern ) ||
        ( Active && Mac == "00:00:00:00:00:00" ) || inet_pton( AF_INET, IP.c_str(), &Address ) != 1 ||
        Port < 1 || Port > 65535 || Service < 1 || Service > 4 || !Functions.insert( FunctionId ).second || !Ids.insert( Id ).second ) return Fail( "Invalid or duplicate function/MAC/IP/service/port" );
    if( Mac != "00:00:00:00:00:00" )
     {
      auto ExistingClass = MacServices.find( Mac );
      if( ExistingClass != MacServices.end() && ( Service >= 3 || ExistingClass->second >= 3 ) ) return Fail( "Only the two Pi services may share a MAC" );
      MacServices[Mac] = Service;
     }
    if( Service >= 3 && ( ( ( Active || !Hardware.empty() ) && ( !std::regex_match( Hardware, NamePattern ) || !HardwareNames.insert( Hardware ).second ) ) || ( Mac != "00:00:00:00:00:00" && !PhysicalMacs.insert( Mac ).second ) ) ) return Fail( "Hardware name and physical MAC must be unique" );
    if( Active && !Addresses.insert( IP + ":" + std::to_string( Port ) ).second ) return Fail( "Duplicate active IP/port" );
    if( Service >= 3 && !( Service == 3 && ( Text( Row, "DeviceType" ) == "Feeder" || Text( Row, "DeviceType" ) == "FeederSimple" ) ) &&
        !( Service == 4 && ( Text( Row, "DeviceType" ) == "TmrSwA8T" || Text( Row, "DeviceType" ) == "TmrSwShellyC3V1" ) ) ) return Fail( "DeviceType does not match service" );
    if( Text( Row, "Group" ) != "System" && Text( Row, "Group" ) != "Aq1" && Text( Row, "Group" ) != "Aq2" && Text( Row, "Group" ) != "Aq3" ) return Fail( "Unknown aquarium group" );
    auto Old = FDevices.find( FunctionId );
    if( Old != FDevices.end() && Old->second.Id != Id ) return Fail( "Use function swap instead of changing physical identity" );
    if( Active )
     {
      auto OldPhysical = FDevices.find( Function( Id ) );
      if( OldPhysical != FDevices.end() && ( OldPhysical->second.Function != FunctionId || OldPhysical->second.DeviceType != Text( Row, "DeviceType" ) ) ) return Fail( "Existing identity/function/type cannot be changed here" );
     }
    if( Service <= 2 )
     {
      auto OldRow = Find( Member( Previous.P, "Devices" ), "Function", FunctionId );
      if( !OldRow || !json_object_equal( OldRow, Row ) ) return Fail( "Pi services cannot be edited or added" );
     }
    ById[Id] = Row;
   }
  for( auto const & Item : FDevices )
   {
    auto const & Device = Item.second;
    auto New = ById.find( Device.Id );
    if( New != ById.end() && json_object_get_boolean( Member( New->second, "Active" ) ) ) continue;
    if( Device.Id.back() <= '2' ) return Fail( "Pi services cannot be removed" );
    TJson Times{ json_object_from_file( ( FRoot / "data/Times.json" ).c_str() ) }, Profiles{ json_object_from_file( ( FRoot / "data/HeatProfiles.json" ).c_str() ) };
    if( !Times.P || !Profiles.P ) return Fail( "Cannot validate references" );
    if( Find( Member( Times.P, "Times" ), "Device", Device.Function ) ) return Fail( "Remove function schedules first: " + Device.Function );
    if( Find( Member( Profiles.P, "Profiles" ), "TempId", Device.Function ) ) return Fail( "Device is a temperature source: " + Device.Function );
    auto Endpoints = Member( Document.P, "Endpoints" );
    if( json_object_is_type( Endpoints, json_type_array ) )
      for( size_t E = 0; E < json_object_array_length( Endpoints ); ++E )
       {
        auto Endpoint = json_object_array_get_idx( Endpoints, E );
        if( Text( Endpoint, "DeviceId" ) != Device.Id ) continue;
        if( Find( Member( Profiles.P, "Profiles" ), "TempId", Text( Endpoint, "Function" ) ) )
          return Fail( "Endpoint is a temperature source: " + Text( Endpoint, "Function" ) );
        auto State = Member( Member( Times.P, "Channels" ), Device.Function.c_str() );
        auto ChannelRows = json_object_is_type( State, json_type_array ) ? State : Member( State, "Rows" );
        const int ChannelIndex = json_object_get_int( Member( Endpoint, "Index" ) );
        if( json_object_is_type( ChannelRows, json_type_array ) &&
            !Text( json_object_array_get_idx( ChannelRows, static_cast<size_t>( ChannelIndex ) ), "Profile" ).empty() )
          return Fail( "Endpoint still has a heating profile" );
       }
   }
  auto NewDevices = json_object_new_array(), Inactive = json_object_new_array(), Views = json_object_new_array(), Names = Clone( Member( Configuration.P, "HardwareNames" ) );
  auto OldRows = Member( Document.P, "Devices" );
  for( size_t I = 0; I < json_object_array_length( Rows ); ++I )
   {
    auto Row = json_object_array_get_idx( Rows, I );
    const auto FunctionId = Text( Row, "Function" ), Mac = Text( Row, "MAC" );
    const int Service = json_object_get_int( Member( Row, "ServiceId" ) );
    if( Service == 2 ) continue; // Kept as an unchanged additional Pi service below.
    String( Names, Mac.c_str(), Text( Row, "HardwareName" ) );
    if( !json_object_get_boolean( Member( Row, "Active" ) ) )
     {
      auto New = Clone( Row ); json_object_object_add( New, "Placeholder", json_object_new_boolean( true ) );
      String( New, "PlannedProject", Text( Row, "DeviceType" ) ); json_object_array_add( Inactive, New );
      auto View = Clone( Display( Configuration.P, FunctionId ) );
      if( Text( View, "Name" ).empty() ) String( View, "Name", FunctionId );
      String( View, "BaseUrl", "http://" + Text( Row, "IP" ) + ":" + std::to_string( json_object_get_int( Member( Row, "HttpPort" ) ) ) );
      json_object_object_add( View, "Group", json_object_get( Member( Row, "Group" ) ) );
      json_object_object_add( View, "UseHardwareName", json_object_get( Member( Row, "UseHardwareName" ) ) );
      String( View, "HardwareName", Text( Row, "HardwareName" ) );
      json_object_object_add( View, "Active", json_object_new_boolean( false ) );
      json_object_array_add( Views, View ); continue;
     }
    auto Old = Find( OldRows, "DeviceId", Mac + "@" + std::to_string( Service ) ), New = Clone( Old );
    for( auto Key : { "Function", "MAC", "IP", "HttpPort", "ServiceId", "DeviceType", "Group" } ) json_object_object_add( New, Key, json_object_get( Member( Row, Key ) ) );
    String( New, "DeviceId", Mac + "@" + std::to_string( Service ) ); json_object_array_add( NewDevices, New );
    auto View = Clone( Display( Configuration.P, FunctionId ) );
    if( Text( View, "Name" ).empty() ) String( View, "Name", FunctionId );
    String( View, "BaseUrl", "http://" + Text( Row, "IP" ) + ( json_object_get_int( Member( Row, "HttpPort" ) ) == 80 ? "" : ":" + std::to_string( json_object_get_int( Member( Row, "HttpPort" ) ) ) ) );
    json_object_object_add( View, "Group", json_object_get( Member( Row, "Group" ) ) );
    json_object_object_add( View, "UseHardwareName", json_object_get( Member( Row, "UseHardwareName" ) ) );
    if( !Old )
     {
      String( View, "ConfigUrl", "/Index.html" ); json_object_object_add( View, "Responsive", json_object_new_boolean( true ) );
     }
    json_object_object_add( View, "Active", json_object_new_boolean( true ) );
    json_object_array_add( Views, View );
   }
  // Preserve the local System status card, which is not a remote registry participant.
  auto OldViews = Member( Configuration.P, "Devices" );
  for( size_t I = 0; OldViews && I < json_object_array_length( OldViews ); ++I )
   {
    auto View = json_object_array_get_idx( OldViews, I );
    if( json_object_get_boolean( Member( View, "StatusOnly" ) ) ) json_object_array_add( Views, Clone( View ) );
   }
  json_object_object_add( Configuration.P, "Devices", Views ); json_object_object_add( Configuration.P, "HardwareNames", Names );
  json_object_object_add( Document.P, "Devices", NewDevices ); json_object_object_add( Document.P, "Unresolved", Inactive );
  auto PersistentConfiguration = json_object_new_object();
  json_object_object_add( PersistentConfiguration, "Devices", json_object_get( Views ) );
  json_object_object_add( PersistentConfiguration, "HardwareNames", json_object_get( Names ) );
  json_object_object_add( Document.P, "EditorConfiguration", PersistentConfiguration );
  const auto Revision = RevisionNow(); if( Revision <= FRevision ) return Fail( "Wait for a newer server timestamp" );
  String( Document.P, "Revision", Revision );
  auto Pending = json_object_new_object(); String( Pending, "Kind", "EditDevices" );
  auto Endpoints = Member( Document.P, "Endpoints" );
  if( json_object_is_type( Endpoints, json_type_array ) )
   {
    auto ActiveEndpoints = json_object_new_array();
    for( size_t E = 0; E < json_object_array_length( Endpoints ); ++E )
     {
      auto Endpoint = json_object_array_get_idx( Endpoints, E ); auto It = ById.find( Text( Endpoint, "DeviceId" ) );
      if( It != ById.end() && json_object_get_boolean( Member( It->second, "Active" ) ) ) json_object_array_add( ActiveEndpoints, json_object_get( Endpoint ) );
     }
    json_object_object_add( Document.P, "Endpoints", ActiveEndpoints );
   }
  json_object_object_add( Pending, "Transfer", Transfer ? json_object_get( Transfer ) : json_object_new_object() ); json_object_object_add( Document.P, "PendingOperation", Pending );
  return Save( Document.P, Error );
 }
