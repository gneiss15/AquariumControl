// FeedbackMonitor.cpp, Version: 1.01
#include <GnBasics.h>
#include "FeedbackMonitor.h"
#include <set>
#include <stdexcept>

namespace
 {
json_object * Get( json_object * Object, char const * Key )
 {
  json_object * Value = nullptr;
  if( Object ) json_object_object_get_ex( Object, Key, &Value );
  return Value;
 }
TGNString Text( json_object * Object, char const * Key )
 {
  auto Value = Get( Object, Key );
  return json_object_is_type( Value, json_type_string ) ? json_object_get_string( Value ) : "";
 }
void Put( json_object * Object, char const * Key, TGNString const & Value )
 { json_object_object_add( Object, Key, json_object_new_string( Value.c_str() ) ); }
void Put( json_object * Object, char const * Key, std::optional<bool> Value )
 { json_object_object_add( Object, Key, Value ? json_object_new_boolean( *Value ) : nullptr ); }
 }

void TFeedbackMonitor::Configure( json_object * Configuration )
 {
  Monitors.clear();
  if( !Configuration ) return;
  auto Rows = Get( Configuration, "Monitors" );
  if( json_object_get_int( Get( Configuration, "Version" ) ) != 1 ||
      !json_object_is_type( Rows, json_type_array ) || json_object_array_length( Rows ) > 32 )
    throw std::runtime_error( "Invalid FeedbackMonitors.json" );
  std::set<TGNString> Ids;
  for( size_t I = 0; I < json_object_array_length( Rows ); ++I )
   {
    auto Row = json_object_array_get_idx( Rows, I ); TMonitor Monitor;
    Monitor.Id = Text( Row, "Id" ); Monitor.SwitchFunction = Text( Row, "SwitchFunction" );
    Monitor.Source = Text( Row, "Source" ); Monitor.Kind = Text( Row, "StatusKind" );
    Monitor.Field = Text( Row, "FeedbackField" ); auto Grace = Get( Row, "GraceSeconds" );
    if( Monitor.Id.empty() || Monitor.SwitchFunction.empty() || Monitor.Source.empty() ||
        Monitor.Kind.empty() || Monitor.Field.empty() || !Ids.insert( Monitor.Id ).second ||
        !json_object_is_type( Grace, json_type_int ) || json_object_get_int( Grace ) < 1 || json_object_get_int( Grace ) > 3600 )
      throw std::runtime_error( "Invalid feedback monitor rule" );
    Monitor.GraceMs = uint64_t( json_object_get_int( Grace ) ) * 1000;
    Monitors.push_back( Monitor );
   }
 }

void TFeedbackMonitor::Bind( json_object * Registry )
 {
  for( auto & Monitor : Monitors )
   {
    Monitor.SwitchId.clear(); Monitor.SourceId.clear(); Monitor.ChannelIndex = -1;
    Monitor.Switch = {}; Monitor.Feedback = {}; Monitor.MismatchSince.reset(); Monitor.MismatchDirection.reset();
    auto External = Get( Registry, "ExternalSources" ); Monitor.SourceId = Text( External, Monitor.Source.c_str() );
    auto Devices = Get( Registry, "Devices" );
    for( size_t I = 0; I < json_object_array_length( Devices ); ++I )
     {
      auto Row = json_object_array_get_idx( Devices, I );
      if( Text( Row, "Function" ) == Monitor.Source && Monitor.SourceId.empty() ) Monitor.SourceId = Text( Row, "DeviceId" );
     }
    auto Endpoints = Get( Registry, "Endpoints" );
    for( size_t I = 0; I < json_object_array_length( Endpoints ); ++I )
     {
      auto Row = json_object_array_get_idx( Endpoints, I );
      if( Text( Row, "Function" ) == Monitor.Source && Monitor.SourceId.empty() ) Monitor.SourceId = Text( Row, "DeviceId" );
      if( Text( Row, "Function" ) == Monitor.SwitchFunction && json_object_get_int( Get( Row, "Capabilities" ) ) == 1 )
       {
        Monitor.SwitchId = Text( Row, "DeviceId" ); auto Index = Get( Row, "Index" );
        if( json_object_is_type( Index, json_type_int ) ) Monitor.ChannelIndex = json_object_get_int( Index );
       }
     }
   }
 }

void TFeedbackMonitor::Receive( json_object * Packet, uint64_t Now )
 {
  auto Status = Get( Packet, "Status" ); const auto Id = Text( Packet, "ID" );
  if( !json_object_is_type( Status, json_type_object ) ) return;
  bool Relevant = false;
  for( auto const & Monitor : Monitors )
    Relevant = Relevant || ( !Id.empty() && ( Id == Monitor.SourceId || Id == Monitor.SwitchId ) );
  if( !Relevant ) return;
  // Expiry must reset a pending discrepancy before a resumed sender becomes fresh.
  auto Before = this->Status( Now ); json_object_put( Before );
  for( auto & Monitor : Monitors )
   {
    if( !Id.empty() && Id == Monitor.SourceId )
     {
      // Fresh packets with unknown/invalid fields must not preserve an old boolean.
      auto Field = Get( Status, Monitor.Field.c_str() ); Monitor.Feedback.Received = Now;
      Monitor.Feedback.Value.reset();
      json_object * Present = nullptr;
      const bool HasField = json_object_object_get_ex( Status, Monitor.Field.c_str(), &Present );
      Monitor.Feedback.Invalid = Text( Status, "Kind" ) != Monitor.Kind ||
        ( HasField && !json_object_is_type( Field, json_type_boolean ) );
      if( !Monitor.Feedback.Invalid && HasField )
        Monitor.Feedback.Value = bool( json_object_get_boolean( Field ) );
     }
    if( !Id.empty() && Id == Monitor.SwitchId )
     {
      Monitor.Switch.Received = Now; Monitor.Switch.Value.reset(); Monitor.Switch.Invalid = false;
      auto Channels = Get( Status, "Channels" );
      for( size_t I = 0; I < json_object_array_length( Channels ); ++I )
       {
        auto Row = json_object_array_get_idx( Channels, I ); auto Index = Get( Row, "Id" ), State = Get( Row, "State" );
        if( json_object_is_type( Index, json_type_int ) && json_object_get_int( Index ) == Monitor.ChannelIndex )
         {
          Monitor.Switch.Invalid = !json_object_is_type( State, json_type_boolean );
          if( !Monitor.Switch.Invalid ) Monitor.Switch.Value = bool( json_object_get_boolean( State ) );
         }
       }
     }
   }
  // Evaluate on every packet too: recovery and direction changes reset the timer
  // even while no browser is open or between two browser status requests.
  auto Snapshot = this->Status( Now ); json_object_put( Snapshot );
 }

json_object * TFeedbackMonitor::Status( uint64_t Now )
 {
  auto Result = json_object_new_array();
  for( auto & Monitor : Monitors )
   {
    auto Fresh = [Now]( TSignal const & Signal ) { return Signal.Received && Signal.Value.has_value() && Now >= *Signal.Received && Now - *Signal.Received <= 20000; };
    const bool SwitchKnown = Fresh( Monitor.Switch ), FeedbackKnown = Fresh( Monitor.Feedback );
    auto Invalid = [Now]( TSignal const & Signal ) { return Signal.Received && Signal.Invalid && Now >= *Signal.Received && Now - *Signal.Received <= 20000; };
    TGNString State = "Unknown", Reason = !SwitchKnown
      ? ( Invalid( Monitor.Switch ) ? "InvalidSwitch" : "MissingSwitch" )
      : ( Invalid( Monitor.Feedback ) ? "InvalidFeedback" : "MissingFeedback" );
    if( SwitchKnown && FeedbackKnown )
     {
      if( Monitor.Switch.Value == Monitor.Feedback.Value ) { State = "OK"; Reason.clear(); }
      else
       {
        if( !Monitor.MismatchSince || Monitor.MismatchDirection != Monitor.Switch.Value )
         { Monitor.MismatchSince = Now; Monitor.MismatchDirection = Monitor.Switch.Value; }
        State = Now - *Monitor.MismatchSince >= Monitor.GraceMs ? "Mismatch" : "Pending";
        Reason = *Monitor.Switch.Value ? "OnWithoutFeedback" : "OffWithFeedback";
       }
     }
    if( State == "Unknown" || State == "OK" ) { Monitor.MismatchSince.reset(); Monitor.MismatchDirection.reset(); }
    auto Row = json_object_new_object(); Put( Row, "Id", Monitor.Id ); Put( Row, "SwitchFunction", Monitor.SwitchFunction );
    Put( Row, "Source", Monitor.Source ); Put( Row, "State", State ); Put( Row, "Reason", Reason );
    Put( Row, "SwitchState", SwitchKnown ? Monitor.Switch.Value : std::nullopt );
    Put( Row, "FeedbackState", FeedbackKnown ? Monitor.Feedback.Value : std::nullopt );
    json_object_object_add( Row, "GraceSeconds", json_object_new_int64( Monitor.GraceMs / 1000 ) );
    json_object_array_add( Result, Row );
   }
  return Result;
 }
