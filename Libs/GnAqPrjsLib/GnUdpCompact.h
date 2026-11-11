// GnUdpCompact.h, Version: 1.16
#pragma once
#include <GnBasics.h>
#include <GnWireJson.h>
#include <GnUdpSchema.h>
#include <GnUdpInfoParts.h>
#include <GnUdpChannelBits.h>
#include <set>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <type_traits>

template<class T> bool GnUdpFieldsHavePart( TGnWireValue Object, char const * Expected )
 {
  if(!Object.Object())return false;
  for(auto const & Field:Object.Members())
   {
    const auto Part=GnJsonFieldPart<T>(Field.first);
    if(!Part || (std::strcmp(Part,Expected)!=0 &&
       !(Field.first=="Ver" && std::strcmp(Part,"Live")==0)))return false;
   }
  return true;
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
    std::map<TGNString,uint16_t> ChannelStates;
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
    TGNString Value=R.Get("Data").Text();
    auto Check=std::make_unique<TGnWireDocument>(Value);
    if(!Check->Root().Object() && Value.size()>=4 && Value[0]=='{' && Value[1]=='{' &&
       Value[Value.size()-2]=='}' && Value.back()=='}')
     {
      Value=Value.substr(1,Value.size()-2);
      Check=std::make_unique<TGnWireDocument>(Value);
     }
    if(!Check->Root().Object())return false;
    bool Valid=false;
    if(Field=="Root")
     {
      Valid=GnTypeValid(Check->Root(),TGnJsonType<TUdpStatusData>{},true) &&
            GnUdpFieldsHavePart<TUdpStatusData>(Check->Root(),"Info");
     }
    else if(Field=="Identity")
      Valid=GnTypeValid(Check->Root(),TGnJsonType<TJsonIdentity>{},true) &&
            GnUdpFieldsHavePart<TJsonIdentity>(Check->Root(),"Info");
    else if(Field.rfind("Channel:",0)==0)
      Valid=GnTypeValid(Check->Root(),TGnJsonType<TUdpChannel>{},true) &&
            GnUdpFieldsHavePart<TUdpChannel>(Check->Root(),"Info");
    else if(Field.rfind("Measurement:",0)==0)
      Valid=GnTypeValid(Check->Root(),TGnJsonType<TUdpMeasurement>{},true) &&
            GnUdpFieldsHavePart<TUdpMeasurement>(Check->Root(),"Info");
    if(!Valid)return false;
    S.Parts[Field]=Value;return true;
   }
  TGNString Expand(TGNString const & Packet,uint64_t Now,TGNString & Request,bool Partial=false)
   {
    TGnWireDocument D(Packet);if(!GnTypeValid(D.Root(),TGnJsonType<TUdpStatusMessage>{}))return {};return Expand(D.Root(),Now,Request,Partial);
   }
  TGNString Expand(TGnWireValue R,uint64_t Now,TGNString & Request,bool Partial=false)
   {
    return ExpandInPlace(R,Now,Request,Partial) ? R.Json() : TGNString{};
   }
  bool ExpandInPlace(TGnWireValue R,uint64_t Now,TGNString & Request,bool Partial=false)
   {
    Request.clear();
    if(!R.Get("Compact").Bool())return R.Object();
    const auto Id=R.Get("ID").Text(),Revision=R.Get("InfoRev").Text();
    if(Id.empty()||Revision.size()!=16)return false;
    if(!Entries.count(Id)&&Entries.size()>=32)return false;
    auto & E=Entries[Id];if(E.Revision!=Revision){E=TEntry{};E.Revision=Revision;}
    auto S=R.Get("Status");E.Required={"Root","Identity"};
    for(auto C:S.Get("Chs").Items())E.Required.insert("Channel:"+C.Get("Id").Text());
    for(auto M:S.Get("Meas").Items())E.Required.insert("Measurement:"+M.Get("Kind").Text());
    if(E.Required.size()>64)return false;
    bool Complete=true;for(auto const & K:E.Required)Complete=Complete&&E.Parts.count(K);
    if(!Complete&&(!E.Requested||Now-E.LastRequest>=(E.Attempts<3?3000:30000)))
     {
      TUdpInfoRequestMessage Query;Query.ID=Id;Query.InfoRev=Revision;
      for(auto const & K:E.Required)if(!E.Parts.count(K))Query.Fields.push_back(K);
      Request=GnWriteUdpMessage(Query);
      E.LastRequest=Now;E.Requested=true;++E.Attempts;
     }
    if(!Complete&&!Partial)return false;
    auto Merge=[&](TGnWireValue Target,TGNString const & Key)
     {
      auto P=E.Parts.find(Key);if(P==E.Parts.end())return;
      TGnWireDocument Info(P->second);for(auto const & V:Info.Root().Members())Target.Set(V.first,V.second);
     };
    Merge(S,"Root");
    auto Identity=S.NewObject("Identity");Merge(Identity,"Identity");
    for(auto C:S.Get("Chs").Items())
     {
      Merge(C,"Channel:"+C.Get("Id").Text());
      const TGNString ChannelId=C.Get("Id").Text();
      const uint16_t Incoming=static_cast<uint16_t>(C.Get("Bits").UInt());
      const uint16_t ValidValues=static_cast<uint16_t>((Incoming>>1)&0x5555U);
      const uint16_t Replace=static_cast<uint16_t>(ValidValues|(ValidValues<<1));
      auto & Previous=E.ChannelStates[ChannelId];
      Previous=static_cast<uint16_t>((Previous&~Replace)|(Incoming&Replace));
      TGnUdpChannelBits Bits{};Bits.All=Previous;
#define JD(Type, Member, Presence, Rule, Description, Part) JD_##Part(Type, Member, Presence, Rule, Description)
#define JD_Live(Type, Member, Presence, Rule, Description)
#define JD_Info(Type, Member, Presence, Rule, Description)
#define JD_Omit(Type, Member, Presence, Rule, Description) \
      if(Bits.Bit.Member ## Msk)C.Bool(#Member,Bits.Bit.Member);else C.SetNull(#Member);
#define Do_TUdpChannel
#include <Definitions/Contract/TUdpChannel.inc.h>
#undef Do_TUdpChannel
#undef JD_Omit
#undef JD_Info
#undef JD_Live
#undef JD
      C.Remove("Bits");
     }
    for(auto M:S.Get("Meas").Items())Merge(M,"Measurement:"+M.Get("Kind").Text());
    R.Remove("Compact");R.Remove("InfoRev");return true;
   }
 };
