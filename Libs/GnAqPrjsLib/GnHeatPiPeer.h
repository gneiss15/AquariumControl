// GnHeatPiPeer.h, Version: 1.04
#pragma once
#include <GnBasics.h>
#include <AddressResolver.h>
#include <GnUdpSchema.h>
#include <GnWireJson.h>
#include <map>
#include <cmath>

// Only explicitly requested main identities are cached; descriptions never refresh leases.
class TGnHeatPiPeers
 {
  struct TEntry { TGNString Status; uint64_t Deadline = 0, LastTime = 0; };
  std::map<TGNString,TEntry> Entries;
 public:
  bool Get(TGNString const & Id,uint64_t Now,TGNString & Status)
   {
    auto P=Entries.find(Id);
    if(P==Entries.end())
     { if(Entries.size()<8)Entries.emplace(Id,TEntry{});return false; }
    if(P->second.Status.empty() || Now>P->second.Deadline)return false;
    Status=P->second.Status;return true;
   }
  // Called by the dispatcher after validating a status packet.
  void AcceptStatus(TGnWireValue Root,uint64_t Now,int64_t WallTime)
   {
    auto P=Entries.find(Root.Get("ID").Text());if(P==Entries.end())return;
    auto S=Root.Get("Status");
    auto Pi=S.Get("HeatControl");
    if(!Pi.Object() || !Pi.Get("Active").IsBool() || !Pi.Get("FloorRequest").IsBool())return;
    if(!S.Get("TimeSynchronized").Bool() || std::abs(double(WallTime)-double(S.Get("CurrentTime").UInt()))>60)return;
    // A valid inactive snapshot revokes the previous request immediately.
    const uint64_t Stamp=S.Get("CurrentTime").UInt();
    if(Stamp<P->second.LastTime)return;
    P->second.LastTime=Stamp;
    const uint64_t Age=WallTime>int64_t(Stamp)?uint64_t(WallTime-int64_t(Stamp))*1000:0;
    P->second.Status=Pi.Json();P->second.Deadline=Now+60000-Age;
   }
 };

inline bool GnHeatPiFloorRequest(TGNString const & Text,TGNString const & Profile,
                                 TGNString const & Device,TGNString const & Channel,bool & On)
 {
  TGnWireDocument D(Text);auto P=D.Root();
  TChannelAddress Endpoint;
  if(!P.Get("Active").IsBool() || P.Get("Prof").Text()!=Profile ||
     !TChannelAddress::Parse(P.Get("FloorEndpoint").Text(),Endpoint) ||
     ServiceAddress(Endpoint)!=Device || !Endpoint.HasChannel ||
     std::to_string(Endpoint.Channel)!=Channel)return false;
  // A missing field must not become an OFF command.
  const auto Value=P.Get("FloorRequest").Json();if(Value!="true" && Value!="false")return false;
  On=P.Get("Active").Bool() && P.Get("FloorRequest").Bool();return true;
 }
