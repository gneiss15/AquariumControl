// GnUdpCompact.h, Version: 1.00
#pragma once
#include <GnBasics.h>
#include <GnWireJson.h>
#include <map>
#include <set>
#include <algorithm>
#include <cstdio>

constexpr size_t GnUdpRegularBytes = 900;
using TGnUdpInfoParts = std::map<TGNString,TGNString>;
inline bool GnUdpIs( TGNString const & K,std::initializer_list<char const *> Names )
 { for(auto N:Names)if(K==N)return true;return false; }
inline TGNString GnUdpInfoHash(TGnUdpInfoParts const & Parts)
 {
  TGNString Text;
  for(auto const & P:Parts)Text+=P.first+":"+P.second+"\n";
  char Buffer[17];std::snprintf(Buffer,sizeof(Buffer),"%016llx",static_cast<unsigned long long>(GnHash64(Text.data(),Text.size())));
  return Buffer;
 }
inline TGNString GnUdpCompactStatus(TGNString const & Id,TGNString const & Status,TGnUdpInfoParts & Parts)
 {
  TGnWireDocument Original(Status),Frame,RootInfo,Identity;
  auto S=Original.Root();if(!S.Object())return {};
  auto F=Frame.Root();F.UInt("Version",2);F.Text("Type","Status");F.Text("ID",Id);F.Bool("Compact",true);
  auto D=F.NewObject("Status");
  for(auto const & P:S.Members())
   {
    if(GnUdpIs(P.first,{"DeviceId","Identity","Channels","Measurements"}))continue;
    if(GnUdpIs(P.first,{"OnlineSince","Errors","LastError","TimeSyncError","Kind"}))RootInfo.Root().Set(P.first,P.second);
    else D.Set(P.first,P.second);
   }
  for(auto const & P:S.Get("Identity").Members())
   {
    if(P.first=="BootCounter")D.Set("BootCounter",P.second);
    else if(!GnUdpIs(P.first,{"DeviceId","Sources"}))Identity.Root().Set(P.first,P.second);
   }
  Parts.clear();Parts["Root"]=RootInfo.Root().Json();Parts["Identity"]=Identity.Root().Json();
  if(S.Get("Channels").Array())
   {
    auto Channels=D.NewArray("Channels");
    for(auto Row:S.Get("Channels").Items())
     {
      TGnWireDocument Info;auto C=Channels.AddObject();unsigned Bits=0;
      for(auto const & P:Row.Members())
       {
        if(GnUdpIs(P.first,{"Name","HardwareChannel","CapabilityId","HasInput","HasOutput","StateLabels","NextChange","Error"}))Info.Root().Set(P.first,P.second);
        else if(!GnUdpIs(P.first,{"InputState","AutomaticState","OverrideActive","OverrideState","OverrideUntil","CalibrationOverrideActive"}))C.Set(P.first,P.second);
       }
      if(Row.Get("InputState").Null())Bits|=64;
      if(Row.Get("AutomaticState").Null())Bits|=128;
      if(Row.Get("InputState").Bool())Bits|=1;
      if(Row.Get("OverrideActive").Bool())Bits|=2;
      if(Row.Get("OverrideState").Bool())Bits|=4;
      if(Row.Get("CalibrationOverrideActive").Bool())Bits|=8;
      if(Row.Has("AutomaticState")&&Row.Get("AutomaticState").Json()!=Row.Get("State").Json())
       { Bits|=16;if(Row.Get("AutomaticState").Bool())Bits|=32; }
      if(Bits)C.UInt("Bits",Bits);
      if(!Row.Get("OverrideUntil").Null())C.Set("Until",Row.Get("OverrideUntil"));
      Parts["Channel:"+Row.Get("Id").Json()]=Info.Root().Json();
     }
   }
  if(S.Get("Measurements").Array())
   {
    auto Measurements=D.NewArray("Measurements");
    for(auto Row:S.Get("Measurements").Items())
     {
      TGnWireDocument Info;auto M=Measurements.AddObject();
      for(auto const & P:Row.Members())
        if(GnUdpIs(P.first,{"Id","Value","Valid","UpdatedAt"}))M.Set(P.first,P.second);
        else Info.Root().Set(P.first,P.second);
      Parts["Measurement:"+Row.Get("Id").Text()]=Info.Root().Json();
     }
   }
  D.Get("PowerMeter").Remove("Diagnostics");
  F.Text("InfoRevision",GnUdpInfoHash(Parts));
  // PI observation is a temporary diagnostic extension, explicitly exempted.
  TGnWireDocument Budget(F.Json());Budget.Root().Get("Status").Remove("HeatPi");
  if(Budget.Root().Json().size()>GnUdpRegularBytes)return {};
  return F.Json();
 }
inline std::vector<TGNString> GnUdpInfoReplies(TGNString const & Id,TGNString const & Revision,
                                            TGnUdpInfoParts const & Parts,std::vector<TGNString> const & Fields)
 {
  std::vector<TGNString> Replies;
  for(auto const & Field:Fields)
   {
    auto P=Parts.find(Field);if(P==Parts.end())continue;
    // String chunks keep every description reply below the same datagram limit.
    std::vector<TGNString> Chunks;
    for(size_t Offset=0;Offset<P->second.size();)
     {
      size_t N=std::min(size_t(400),P->second.size()-Offset);
      while(N)
       {
        while(N && Offset+N<P->second.size() && (static_cast<unsigned char>(P->second[Offset+N])&0xC0)==0x80)--N;
        if(!N)break;
        TGnWireDocument Probe;auto F=Probe.Root();F.UInt("Version",2);F.Text("Type","Info");F.Text("ID",Id);
        F.Text("InfoRevision",Revision);F.Text("Field",Field);F.UInt("Chunk",99);F.UInt("Chunks",99);
        F.Text("Data",P->second.substr(Offset,N));
        if(F.Json().size()<=GnUdpRegularBytes)break;
        N/=2;
       }
      if(!N||Chunks.size()>=64)return {};
      Chunks.push_back(P->second.substr(Offset,N));Offset+=N;
     }
    for(size_t I=0;I<Chunks.size();++I)
     {
      TGnWireDocument Reply;auto F=Reply.Root();F.UInt("Version",2);F.Text("Type","Info");F.Text("ID",Id);
      F.Text("InfoRevision",Revision);F.Text("Field",Field);F.UInt("Chunk",I);F.UInt("Chunks",Chunks.size());F.Text("Data",Chunks[I]);
      Replies.push_back(F.Json());
     }
   }
  return Replies;
 }
class TGnUdpDescriptions
 {
  struct TChunks { unsigned Count=0;std::map<unsigned,TGNString> Values; };
  struct TEntry
   {
    TGNString Revision;
    TGnUdpInfoParts Parts;
    std::map<TGNString,TChunks> Chunks;
    std::set<TGNString> Required;
    uint64_t LastRequest=0;
    unsigned Attempts=0;
    bool Requested=false;
   };
  std::map<TGNString,TEntry> Entries;
 public:
  bool AcceptInfo(TGNString const & Packet)
   {
    TGnWireDocument D(Packet);auto R=D.Root();
    if(R.Get("Version").UInt()!=2||R.Get("Type").Text()!="Info")return false;
    auto E=Entries.find(R.Get("ID").Text());if(E==Entries.end())return false;
    auto & S=E->second;const auto Field=R.Get("Field").Text();
    const unsigned Count=R.Get("Chunks").UInt(),Index=R.Get("Chunk").UInt();
    const auto Text=R.Get("Data").Text();
    if(S.Revision!=R.Get("InfoRevision").Text()||!S.Requested||!S.Required.count(Field)||!Count||Count>64||Index>=Count||Text.size()>400)return false;
    auto & C=S.Chunks[Field];if(C.Count&&C.Count!=Count)return false;C.Count=Count;C.Values[Index]=Text;
    if(C.Values.size()!=Count)return true;
    TGNString Value;for(unsigned I=0;I<Count;++I)Value+=C.Values[I];
    if(Value.size()>8192)return false;
    TGnWireDocument Check(Value);if(!Check.Root().Object())return false;
    S.Parts[Field]=Value;S.Chunks.erase(Field);return true;
   }
  TGNString Expand(TGNString const & Packet,uint64_t Now,TGNString & Request,bool Partial=false)
   {
    Request.clear();TGnWireDocument D(Packet);auto R=D.Root();
    if(R.Get("Version").UInt()!=2||R.Get("Type").Text()!="Status"||!R.Get("Status").Object())return {};
    if(!R.Get("Compact").Bool())return Packet;
    const auto Id=R.Get("ID").Text(),Revision=R.Get("InfoRevision").Text();
    if(Id.empty()||Revision.size()!=16)return {};
    if(!Entries.count(Id)&&Entries.size()>=32)return {};
    auto & E=Entries[Id];if(E.Revision!=Revision){E=TEntry{};E.Revision=Revision;}
    auto S=R.Get("Status");E.Required={"Root","Identity"};
    for(auto C:S.Get("Channels").Items())E.Required.insert("Channel:"+C.Get("Id").Json());
    for(auto M:S.Get("Measurements").Items())E.Required.insert("Measurement:"+M.Get("Id").Text());
    if(E.Required.size()>64)return {};
    bool Complete=true;for(auto const & K:E.Required)Complete=Complete&&E.Parts.count(K);
    if(!Complete&&(!E.Requested||Now-E.LastRequest>=(E.Attempts<3?3000:30000)))
     {
      TGnWireDocument Q;auto F=Q.Root();F.UInt("Version",2);F.Text("Type","InfoRequest");F.Text("ID",Id);F.Text("InfoRevision",Revision);
      auto Fields=F.NewArray("Fields");for(auto const & K:E.Required)if(!E.Parts.count(K))Fields.AddText(K);
      Request=F.Json();if(Request.size()>GnUdpRegularBytes)Request.clear();
      E.LastRequest=Now;E.Requested=true;++E.Attempts;
     }
    if(!Complete&&!Partial)return {};
    auto Merge=[&](TGnWireValue Target,TGNString const & Key)
     {
      auto P=E.Parts.find(Key);if(P==E.Parts.end())return;
      TGnWireDocument Info(P->second);for(auto const & V:Info.Root().Members())Target.Set(V.first,V.second);
     };
    Merge(S,"Root");S.Text("DeviceId",Id);
    auto Identity=S.NewObject("Identity");Merge(Identity,"Identity");Identity.Text("DeviceId",Id);
    if(S.Has("BootCounter")){Identity.Set("BootCounter",S.Get("BootCounter"));S.Remove("BootCounter");}
    for(auto C:S.Get("Channels").Items())
     {
      Merge(C,"Channel:"+C.Get("Id").Json());const auto Bits=C.Get("Bits").UInt();
      C.Bool("InputState",Bits&1);if(Bits&64)C.SetNull("InputState");C.Bool("OverrideActive",Bits&2);C.Bool("OverrideState",Bits&4);
      if(!(Bits&2))C.SetNull("OverrideState");
      C.Bool("CalibrationOverrideActive",Bits&8);
      if(Bits&16)C.Bool("AutomaticState",Bits&32);else C.Set("AutomaticState",C.Get("State"));
      if(Bits&128)C.SetNull("AutomaticState");
      if(C.Has("Until"))C.Set("OverrideUntil",C.Get("Until"));else C.SetNull("OverrideUntil");
      C.Remove("Bits");C.Remove("Until");
     }
    for(auto M:S.Get("Measurements").Items())Merge(M,"Measurement:"+M.Get("Id").Text());
    R.Remove("Compact");R.Remove("InfoRevision");return R.Json();
   }
 };
