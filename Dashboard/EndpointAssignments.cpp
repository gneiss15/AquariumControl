// EndpointAssignments.cpp, Version: 1.03
#include "EndpointAssignments.h"
#include <regex>
#include <AddressResolver.h>
#include <GnWireJson.h>
#include <set>
#include <stdexcept>

namespace
 {
struct TJson { json_object * P; ~TJson(){ if( P ) json_object_put( P ); } };
TGNString Text(json_object * O,char const * K)
 {
  auto V=TGnWireValue(O).Get(K);
  if(V.IsText())return V.Text();
  if(TGNString(K)=="Dev")return TGnWireValue(O).Get("DevName").Text();
  if(TGNString(K)=="DevId")
   {TChannelAddress Address;if(TChannelAddress::Parse(TGnWireValue(O).Get("OutAddress").Text(),Address))return ServiceAddress(Address);}
  return {};
 }
int Number( json_object * O, char const * K )
 { json_object * V = nullptr; return O && json_object_object_get_ex( O, K, &V ) ? json_object_get_int( V ) : 0; }
json_object * Get( json_object * O, char const * K )
 { json_object * V = nullptr; if( O ) json_object_object_get_ex( O, K, &V ); return V; }
size_t Length( json_object * O )
 { return json_object_is_type( O, json_type_array ) ? json_object_array_length( O ) : 0; }
json_object * At( json_object * O, size_t I )
 { return json_object_is_type( O, json_type_array ) ? json_object_array_get_idx( O, I ) : nullptr; }
json_object * Copy( json_object * O )
 { return O ? json_tokener_parse( json_object_to_json_string_ext( O, JSON_C_TO_STRING_PLAIN ) ) : nullptr; }
void Put( json_object * O, char const * K, TGNString const & V )
 { json_object_object_add( O, K, json_object_new_string( V.c_str() ) ); }
json_object * Rows( json_object * Times, TGNString const & Device )
 { auto State = Get( Get( Times, "Chs" ), Device.c_str() ); return json_object_is_type( State, json_type_array ) ? State : Get( State, "Rows" ); }
json_object * Find( json_object * List, TGNString const & Id )
 { for( size_t I = 0; I < Length( List ); ++I ) { auto R = At( List, I ); if( Text( R, "OutAddress" ) == Id ) return R; } return nullptr; }
json_object * FunctionGroups( json_object * Configuration, TGNString const & Device, TGNString const & Function )
 {
  auto Result = json_object_new_array();
  auto List = Get( Configuration, "Devices" );
  for( size_t I = 0; I < Length( List ); ++I )
   {
    auto R = At( List, I );
    if( Text( R, "Func" ) != Device ) continue;
    auto Views = Get( R, "GroupViews" );
    for( size_t J = 0; J < Length( Views ); ++J )
     {
      auto View = At( Views, J ), Channels = Get( View, "Chs" );
      for( size_t K = 0; K < Length( Channels ); ++K )
        if( Function == json_object_get_string( At( Channels, K ) ) )
          json_object_array_add( Result, json_object_new_string( Text( View, "Group" ).c_str() ) );
     }
    if( Length( Result ) == 0 && !Text( R, "Group" ).empty() )
      json_object_array_add( Result, json_object_new_string( Text( R, "Group" ).c_str() ) );
   }
  if( !Length( Result ) ) json_object_array_add( Result, json_object_new_string( "System" ) );
  return Result;
 }
void SetChannel( json_object * Times, json_object * Endpoint, json_object * Function )
 {
  auto List = Rows( Times, Text( Endpoint, "Dev" ) );
  if( !List )
   {
    auto State = json_object_new_object(); List = json_object_new_array();
    json_object_object_add( State, "Rows", List ); Put( State, "TimeStamp", "1970-01-01T00:00:00.000Z" );
    json_object_object_add( Get( Times, "Chs" ), Text( Endpoint, "Dev" ).c_str(), State );
   }
  const int Index = Number( Endpoint, "Index" );
  auto Row = Copy( At( List, Index ) );
  if( !Row ) Row = json_object_new_object();
  // Preserve physical pins, polarity, position and all unknown hardware fields.
  Put( Row, "Id", Text( Function, "Func" ) );
  Put( Row, "Role", Text( Function, "Role" ).empty() ? "Timer" : Text( Function, "Role" ) );
  if( Text( Function, "Prof" ).empty() ) json_object_object_del( Row, "Prof" );
  else Put( Row, "Prof", Text( Function, "Prof" ) );
  json_object_array_put_idx( List, static_cast<size_t>( Index ), Row );
 }
 } // namespace

json_object * EndpointView( TDeviceRegistry const & Registry )
 {
  TJson Times{ json_object_from_file( ( Registry.Root() / "data/Times.json" ).c_str() ) };
  TJson Configuration{ json_object_from_file( ( Registry.Root() / "Devices.json" ).c_str() ) };
  TJson Journal{ json_object_from_file( ( Registry.Root() / "data/DeviceAssignments.json" ).c_str() ) };
  if( !Times.P || !Configuration.P || !Journal.P ) throw std::runtime_error( "Endpoint source data unavailable" );
  Registry.ApplyAddresses( Configuration.P );
  auto Stored = Get( Journal.P, "Endpoints" ), Result = json_object_new_array();
  std::set<TGNString> Addresses;
  for( auto const & Item : Registry.Devices() )
   {
    auto const & Device = Item.second;
    const char Service = Device.Id.back();
    if( Service != '1' && Service != '3' && Service != '4' ) continue;
    auto Channels = Rows( Times.P, Item.first );
    const size_t Count = Service == '1' ? Length( Channels ) : 1;
    for( size_t I = 0; I < Count; ++I )
     {
      auto Channel = At( Channels, I );
      int Nr = 0;
      if( Service == '1' )
       {
        std::smatch Match; const auto Position = Text( Channel, "PhysPos" );
        if( !std::regex_match( Position, Match, std::regex( "Main ([1-8])" ) ) )
          { json_object_put( Result ); throw std::runtime_error( "MT PhysPos must be unique Main 1..8" ); }
        Nr = std::stoi( Match[1] );
       }
      const auto Id = Device.Id + "/" + std::to_string( Nr );
      if( !Addresses.insert( Id ).second ) { json_object_put( Result ); throw std::runtime_error( "Duplicate hardware endpoint" ); }
      auto R = json_object_new_object(); Put( R, "OutAddress", Id ); Put( R, "DevName", Item.first );
      Put( R, "HwName", Service == '1' ? "MultiTimer1@" + std::to_string( Nr ) : Device.HardwareName );
      json_object_object_add( R, "Index", json_object_new_int( static_cast<int>( I ) ) );
      const int Capabilities = Service == '3' ? 2 : Service == '1' && !json_object_is_type( Get( Channel, "OutPin" ), json_type_int ) ? 0 : 1;
      json_object_object_add( R, "Caps", json_object_new_int( Capabilities ) );
      auto Previous = Find( Stored, Id );
      const auto ChannelId = Text( Channel, "Id" );
      const auto Function = Previous ? Text( Previous, "Func" ) : Service == '3' || ChannelId.empty() || ChannelId == "Schaltausgang" || ChannelId == "Main" || ChannelId == "Relay" ? Item.first : ChannelId;
      Put( R, "Func", Function ); Put( R, "ChName", Service == '3' ? Item.first : ChannelId );
      Put( R, "Role", Text( Channel, "Role" ).empty() ? "Timer" : Text( Channel, "Role" ) );
      Put( R, "Prof", Text( Channel, "Prof" ) );
      bool HardwareLabel = Previous && json_object_get_boolean( Get( Previous, "UseHwName" ) );
      if( !Previous )
       {
        auto Views = Get( Configuration.P, "Devices" );
        for( size_t V = 0; V < Length( Views ); ++V )
         { auto View = At( Views, V ); if( Text( View, "Func" ) == Item.first ) HardwareLabel = json_object_get_boolean( Get( View, "UseHwName" ) ); }
       }
      json_object_object_add( R, "UseHwName", json_object_new_boolean( HardwareLabel ) );
      json_object_object_add( R, "Groups", Previous ? Copy( Get( Previous, "Groups" ) ) : FunctionGroups( Configuration.P, Item.first, ChannelId ) );
      json_object_array_add( Result, R );
     }
   }
  return Result;
 }

bool SwapEndpoints( TDeviceRegistry & Registry, TGNString const & First, TGNString const & Second, TGNString & Error )
 {
  if( Registry.Pending() ) { Error = "Previous operation still pending"; return false; }
  TJson Endpoints{ EndpointView( Registry ) };
  auto A = Find( Endpoints.P, First ), B = Find( Endpoints.P, Second );
  if( !A || !B || A == B ) { Error = "Select two different hardware endpoints"; return false; }
  const int Capability = Number( A, "Caps" );
  if( Capability == 0 || Capability != Number( B, "Caps" ) ) { Error = "Capabilities must be identical"; return false; }
  TJson Times{ json_object_from_file( ( Registry.Root() / "data/Times.json" ).c_str() ) };
  if( !Times.P ) { Error = "Times unavailable"; return false; }
  TJson OldA{ Copy( A ) }, OldB{ Copy( B ) };
  if( Capability == 1 )
   {
    auto Entries = Get( Times.P, "Times" );
    for( size_t I = 0; I < Length( Entries ); ++I )
     {
      auto Entry = At( Entries, I );
      if( Text( Entry, "Type" ) != "SwitchTimes" ) continue;
      auto Data = Get( Entry, "Data" ); const auto Kind = Text( Entry, "Kind" );
      if(Kind!="SwitchTime" && Kind!="Interval")continue;
      const char * IntervalAddress=json_object_get_string(At(Data,4));
      const auto Address=Kind=="SwitchTime" ? Text(Data,"Ch") : TGNString(IntervalAddress ? IntervalAddress : "");
      json_object * Target=nullptr;
      for(auto Pair:{std::pair{OldA.P,OldB.P},std::pair{OldB.P,OldA.P}})
        if(Text(Entry,"Dev")==Text(Pair.first,"Dev") && Address==Text(Pair.first,"OutAddress"))
         {Target=Pair.second;break;}
      if(!Target)continue;
      Put(Entry,"Dev",Text(Target,"Dev"));
      if(Kind=="SwitchTime")Put(Data,"Ch",Text(Target,"OutAddress"));
      else json_object_array_put_idx(Data,4,json_object_new_string(Text(Target,"OutAddress").c_str()));
     }
    SetChannel( Times.P, OldA.P, OldB.P ); SetChannel( Times.P, OldB.P, OldA.P );
    for( auto const & Device : { Text( OldA.P, "Dev" ), Text( OldB.P, "Dev" ) } )
     {
      std::set<TGNString> Names; auto Channels = Rows( Times.P, Device );
      for( size_t I = 0; I < Length( Channels ); ++I )
       {
        const auto Name = Text( At( Channels, I ), "Id" );
        if( Name.empty() || !Names.insert( Name ).second )
         { Error = "Function name already exists on destination device"; return false; }
       }
     }
   }
  for( auto Pair : { std::pair{ A, OldB.P }, std::pair{ B, OldA.P } } )
   {
    Put( Pair.first, "Func", Text( Pair.second, "Func" ) );
    Put( Pair.first, "Role", Text( Pair.second, "Role" ) );
    Put( Pair.first, "Prof", Text( Pair.second, "Prof" ) );
    json_object_object_add( Pair.first, "UseHwName", Copy( Get( Pair.second, "UseHwName" ) ) );
    json_object_object_add( Pair.first, "Groups", Copy( Get( Pair.second, "Groups" ) ) );
   }
  return Registry.ChangeEndpoints( Endpoints.P, Times.P, Error );
 }
