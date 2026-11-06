// GnUdpCompact.h, Version: 1.10
#pragma once
#include <GnBasics.h>
#include <GnWireJson.h>
#include <GnUdpSchema.h>
#include <map>
#include <set>
#include <algorithm>
#include <cstdio>

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
  auto F=Frame.Root();GnInitMessage(F,TGnJsonType<TUdpStatusMessage>{});F.Text("ID",Id);F.Bool("Compact",true);
  auto D=F.NewObject("Status");
  for(auto const & P:S.Members())
   {
    if(GnUdpIs(P.first,{"DevId","Identity","Chs","Meas"}))continue;
    const auto Part=GnJsonFieldPart<TUdpStatusData>(P.first);
    if(Part && TGNString(Part)=="Info")RootInfo.Root().Set(P.first,P.second);
    else if(!Part || TGNString(Part)!="Omit")D.Set(P.first,P.second);
   }
  for(auto const & P:S.Get("Identity").Members())
   {
    if(P.first=="BootCnt")D.Set("BootCnt",P.second);
    else if(!GnUdpIs(P.first,{"ServiceId","Srcs"}))Identity.Root().Set(P.first,P.second);
   }
  Parts.clear();Parts["Root"]=RootInfo.Root().Json();Parts["Identity"]=Identity.Root().Json();
  if(S.Get("Chs").Array())
   {
    auto Channels=D.NewArray("Chs");
    for(auto Row:S.Get("Chs").Items())
     {
      TGnWireDocument Info;auto C=Channels.AddObject();unsigned Bits=0;
      for(auto const & P:Row.Members())
       {
        const auto Part=GnJsonFieldPart<TUdpChannel>(P.first);
        if(Part && TGNString(Part)=="Info")Info.Root().Set(P.first,P.second);
        else if(!GnUdpIs(P.first,{"InState","AutomaticState","OvrActive","OvrState","OvrUntil","CalOvrActive"}))C.Set(P.first,P.second);
       }
      if(Row.Get("InState").Null())Bits|=GnUdpUnknownInput;
      if(Row.Get("AutomaticState").Null())Bits|=GnUdpUnknownAutomatic;
      if(Row.Get("InState").Bool())Bits|=GnUdpInputState;
      if(Row.Get("OvrActive").Bool())Bits|=GnUdpOverrideActive;
      if(Row.Get("OvrState").Bool())Bits|=GnUdpOverrideOn;
      if(Row.Get("CalOvrActive").Bool())Bits|=GnUdpCalibrationOverride;
      if(Row.Has("AutomaticState")&&Row.Get("AutomaticState").Json()!=Row.Get("State").Json())
       { Bits|=GnUdpHasAutomatic;if(Row.Get("AutomaticState").Bool())Bits|=GnUdpAutomaticOn; }
      if(Bits)C.UInt("Bits",Bits);
      if(!Row.Get("OvrUntil").Null())C.Set("Until",Row.Get("OvrUntil"));
      Parts["Channel:"+Row.Get("Id").Text()]=Info.Root().Json();
     }
   }
  if(S.Get("Meas").Array())
   {
    auto Measurements=D.NewArray("Meas");
    for(auto Row:S.Get("Meas").Items())
     {
      TGnWireDocument Info;auto M=Measurements.AddObject();
      for(auto const & P:Row.Members())
       {
        const auto Part=GnJsonFieldPart<TUdpMeasurement>(P.first);
        if(!Part || TGNString(Part)=="Live")M.Set(P.first,P.second);
        else Info.Root().Set(P.first,P.second);
       }
      Parts["Measurement:"+Row.Get("Kind").Text()]=Info.Root().Json();
     }
   }
  D.Get("PowerMeter").Remove("Diag");
  // Keep the setpoint used by the history charts and the live PI diagnostics.
  // The full controller configuration remains available in the device status API.
  auto Pi=D.Get("HeatDiag");
  if(Pi.Object())
   {
    auto Config=Pi.Get("Cfg");
    if(Config.Object())
      for(auto const & Field:Config.Members())
        if(Field.first!="Setpoint")Config.Remove(Field.first);
    for(auto const * Key:{"ActualMainOn","ErrorK","Learning","TiSeconds"})Pi.Remove(Key);

   }
  F.Text("InfoRev",GnUdpInfoHash(Parts));
  // PI observation is a temporary diagnostic extension, explicitly exempted.
  return F.Json();
 }
inline std::vector<TGNString> GnUdpInfoReplies(TGNString const & Id,TGNString const & Revision,
                                            TGnUdpInfoParts const & Parts,std::vector<TGNString> const & Fields)
 {
  std::vector<TGNString> Replies;
  for(auto const & Field:Fields)
   {
    auto P=Parts.find(Field);if(P==Parts.end())continue;
    TUdpInfoMessage Reply;Reply.ID=Id;Reply.InfoRev=Revision;Reply.Field=Field;Reply.Data=P->second;
    auto Json=GnWriteUdpMessage(Reply);
    if(!Json.empty())Replies.push_back(std::move(Json));
   }
  return Replies;
 }
class TGnUdpDescriptions
 {
  struct TEntry
   {
    TGNString Revision;
    TGnUdpInfoParts Parts;
    std::set<TGNString> Required;
    uint64_t LastRequest=0;
    unsigned Attempts=0;
    bool Requested=false;
   };
  std::map<TGNString,TEntry> Entries;
 public:
  bool AcceptInfo(TGNString const & Packet)
   { TGnWireDocument D(Packet);return GnTypeValid(D.Root(),TGnJsonType<TUdpInfoMessage>{}) && AcceptInfo(D.Root()); }
  bool AcceptInfo(TGnWireValue R)
   {
    auto E=Entries.find(R.Get("ID").Text());if(E==Entries.end())return false;
    auto & S=E->second;const auto Field=R.Get("Field").Text();
    if(S.Revision!=R.Get("InfoRev").Text()||!S.Requested||!S.Required.count(Field))return false;
    const auto Value=R.Get("Data").Text();
    TGnWireDocument Check(Value);if(!Check.Root().Object())return false;
    bool Valid=false;
    if(Field=="Root")
     {
      Valid=GnTypeValid(Check.Root(),TGnJsonType<TUdpStatusData>{},true);
      for(auto const & P:Check.Root().Members())
       { auto Part=GnJsonFieldPart<TUdpStatusData>(P.first);Valid=Valid && Part && TGNString(Part)=="Info"; }
     }
    else if(Field=="Identity")Valid=GnTypeValid(Check.Root(),TGnJsonType<TJsonIdentity>{},true);
    else if(Field.rfind("Channel:",0)==0)Valid=GnTypeValid(Check.Root(),TGnJsonType<TUdpChannel>{},true);
    else if(Field.rfind("Measurement:",0)==0)Valid=GnTypeValid(Check.Root(),TGnJsonType<TUdpMeasurement>{},true);
    if(!Valid)return false;
    S.Parts[Field]=Value;return true;
   }
  TGNString Expand(TGNString const & Packet,uint64_t Now,TGNString & Request,bool Partial=false)
   {
    TGnWireDocument D(Packet);if(!GnTypeValid(D.Root(),TGnJsonType<TUdpStatusMessage>{}))return {};return Expand(D.Root(),Now,Request,Partial);
   }
  TGNString Expand(TGnWireValue R,uint64_t Now,TGNString & Request,bool Partial=false)
   {
    Request.clear();
    if(!R.Get("Compact").Bool())return R.Json();
    const auto Id=R.Get("ID").Text(),Revision=R.Get("InfoRev").Text();
    if(Id.empty()||Revision.size()!=16)return {};
    if(!Entries.count(Id)&&Entries.size()>=32)return {};
    auto & E=Entries[Id];if(E.Revision!=Revision){E=TEntry{};E.Revision=Revision;}
    auto S=R.Get("Status");E.Required={"Root","Identity"};
    for(auto C:S.Get("Chs").Items())E.Required.insert("Channel:"+C.Get("Id").Text());
    for(auto M:S.Get("Meas").Items())E.Required.insert("Measurement:"+M.Get("Kind").Text());
    if(E.Required.size()>64)return {};
    bool Complete=true;for(auto const & K:E.Required)Complete=Complete&&E.Parts.count(K);
    if(!Complete&&(!E.Requested||Now-E.LastRequest>=(E.Attempts<3?3000:30000)))
     {
      TUdpInfoRequestMessage Query;Query.ID=Id;Query.InfoRev=Revision;
      for(auto const & K:E.Required)if(!E.Parts.count(K))Query.Fields.push_back(K);
      Request=GnWriteUdpMessage(Query);
      E.LastRequest=Now;E.Requested=true;++E.Attempts;
     }
    if(!Complete&&!Partial)return {};
    auto Merge=[&](TGnWireValue Target,TGNString const & Key)
     {
      auto P=E.Parts.find(Key);if(P==E.Parts.end())return;
      TGnWireDocument Info(P->second);for(auto const & V:Info.Root().Members())Target.Set(V.first,V.second);
     };
    Merge(S,"Root");S.Text("DevId",Id);
    auto Identity=S.NewObject("Identity");Merge(Identity,"Identity");Identity.Text("ServiceId",Id);
    if(S.Has("BootCnt")){Identity.Set("BootCnt",S.Get("BootCnt"));S.Remove("BootCnt");}
    for(auto C:S.Get("Chs").Items())
     {
      Merge(C,"Channel:"+C.Get("Id").Text());const auto Bits=C.Get("Bits").UInt();
      C.Bool("InState",Bits&GnUdpInputState);if(Bits&GnUdpUnknownInput)C.SetNull("InState");C.Bool("OvrActive",Bits&GnUdpOverrideActive);C.Bool("OvrState",Bits&GnUdpOverrideOn);
      if(!(Bits&GnUdpOverrideActive))C.SetNull("OvrState");
      C.Bool("CalOvrActive",Bits&GnUdpCalibrationOverride);
      if(Bits&GnUdpHasAutomatic)C.Bool("AutomaticState",Bits&GnUdpAutomaticOn);else C.Set("AutomaticState",C.Get("State"));
      if(Bits&GnUdpUnknownAutomatic)C.SetNull("AutomaticState");
      if(C.Has("Until"))C.Set("OvrUntil",C.Get("Until"));else C.SetNull("OvrUntil");
      C.Remove("Bits");C.Remove("Until");
     }
    for(auto M:S.Get("Meas").Items())Merge(M,"Measurement:"+M.Get("Kind").Text());
    R.Remove("Compact");R.Remove("InfoRev");return R.Json();
   }
 };
