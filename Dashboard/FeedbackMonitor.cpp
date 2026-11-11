// FeedbackMonitor.cpp, Version: 1.07
#define Do_TJsonFeedbackConfiguration
#define Do_TJsonFeedbackState
#define Do_TJsonFeedbackStates
#define Do_TJsonRegistryView
#include <GnBasics.h>
#include <AddressResolver.h>
#include "FeedbackMonitor.h"
#include <GnJsonSchema.h>
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

 }

void TFeedbackMonitor::Configure( json_object * Configuration )
 {
  Monitors.clear();
  if( !Configuration ) return;
  TJsonFeedbackConfiguration Settings;
  TGNString Error;
  if(!GnRead(TGnWireValue(Configuration),Settings,Error))
    throw std::runtime_error("Invalid FeedbackMonitors.json");
  std::set<TGNString> Ids;
  for(auto const & Row:Settings.Monitors)
   {
    if(!Ids.insert(Row.Id).second)throw std::runtime_error("Duplicate feedback monitor identifier");
    TMonitor Monitor;
    Monitor.Id=Row.Id;
    Monitor.SwitchFunction=Row.SwitchFunc;
    Monitor.Source=Row.Src;
    Monitor.Kind=Row.StatusKind;
    Monitor.Field=Row.FeedbackField;
    Monitor.GraceMs=uint64_t(Row.GraceSec)*1000;
    Monitors.push_back(std::move(Monitor));
   }
 }

void TFeedbackMonitor::Bind( json_object * Registry )
 {
  TJsonRegistryView View;
  TGNString Error;
  if(!GnRead(TGnWireValue(Registry),View,Error))throw std::runtime_error("Invalid feedback registry bindings");
  for(auto & Monitor:Monitors)
   {
    Monitor.SwitchId.clear();Monitor.SwitchChannelId.clear();Monitor.SourceId.clear();
    Monitor.Switch={};Monitor.Feedback={};Monitor.MismatchSince.reset();Monitor.MismatchDirection.reset();
    auto Source=View.ExternalSrcs.Value.find(Monitor.Source);
    if(Source!=View.ExternalSrcs.Value.end())Monitor.SourceId=Source->second;
    for(auto const & Device:View.Devices.Value)
      if(Device.Func==Monitor.Source && Monitor.SourceId.empty())Monitor.SourceId=Device.DevId;
    for(auto const & Endpoint:View.Endpoints.Value)
     {
      if(Endpoint.Func==Monitor.Source && Monitor.SourceId.empty()){TChannelAddress Address;if(TChannelAddress::Parse(Endpoint.OutAddress.Value,Address))Monitor.SourceId=ServiceAddress(Address);};
      if(Endpoint.Func==Monitor.SwitchFunction && (uint8_t(Endpoint.Caps.Value)&1)!=0)
       {
        TChannelAddress Address;
        if(!TChannelAddress::Parse(Endpoint.OutAddress.Value,Address))continue;
        Monitor.SwitchId=ServiceAddress(Address);
        Monitor.SwitchChannelId=Endpoint.OutAddress.Value;
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
      auto Channels = Get( Status, "Chs" );
      for( size_t I = 0; I < json_object_array_length( Channels ); ++I )
       {
        auto Row = json_object_array_get_idx( Channels, I ); auto State = Get( Row, "State" );
        if( Text( Row, "Id" ) == Monitor.SwitchChannelId )
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
  TJsonFeedbackStates Result;
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
    TJsonFeedbackState Row;
    Row.Id=Monitor.Id;
    Row.SwitchFunc=Monitor.SwitchFunction;
    Row.Src=Monitor.Source;
    Row.State=State;
    Row.Reason=Reason;
    if(SwitchKnown)Row.SwitchState=*Monitor.Switch.Value;else Row.SwitchState=nullptr;
    if(FeedbackKnown)Row.FeedbackState=*Monitor.Feedback.Value;else Row.FeedbackState=nullptr;
    Row.GraceSec=static_cast<uint32_t>(Monitor.GraceMs/1000);
    Result.Value.push_back(std::move(Row));
   }
  return json_tokener_parse(GnWriteJson(Result).c_str());
 }
