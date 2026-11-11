// GnHeatPiPeer.h, Version: 1.02
#pragma once
#include <GnBasics.h>
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
  void Accept(TGnWireValue Root,uint64_t Now,int64_t WallTime)
   {
    if(Root.Get("Version").UInt()!=2 || Root.Get("Type").Text()!="Status")return;
    auto P=Entries.find(Root.Get("ID").Text());if(P==Entries.end())return;
    auto S=Root.Get("Status");
    // Prefer the permanent control contract; legacy telemetry is rollout fallback only.
    auto Pi=S.Has("HeatControl")?S.Get("HeatControl"):S.Get("HeatPi");
    if(!S.Get("TimeSynchronized").Bool() || std::abs(double(WallTime)-double(S.Get("CurrentTime").UInt()))>60)return;
    // An explicit inactive/invalid snapshot revokes the previous request immediately.
    const uint64_t Stamp=S.Get("CurrentTime").UInt();
    if(Stamp<P->second.LastTime)return;
    P->second.LastTime=Stamp;
    const uint64_t Age=WallTime>int64_t(Stamp)?uint64_t(WallTime-int64_t(Stamp))*1000:0;
    P->second.Status=Pi.Object()?Pi.Json():TGNString{};P->second.Deadline=Now+60000-Age;
   }
 };

inline bool GnHeatPiFloorRequest(TGNString const & Text,TGNString const & Profile,
                                 TGNString const & Device,TGNString const & Channel,bool & On)
 {
  TGnWireDocument D(Text);auto P=D.Root();
  if(P.Get("Mode").Text()!="Active" || P.Get("Profile").Text()!=Profile ||
     P.Get("Config").Get("FloorDevice").Text()!=Device ||
     P.Get("Config").Get("FloorChannel").Text()!=Channel)return false;
  // A missing field must not become an OFF command.
  const auto Value=P.Get("FloorRequest").Json();if(Value!="true" && Value!="false")return false;
  On=P.Get("FloorRequest").Bool();return true;
 }
