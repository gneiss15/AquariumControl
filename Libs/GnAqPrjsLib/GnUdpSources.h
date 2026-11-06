// GnUdpSources.h, Version: 1.05
#pragma once
#include <GnBasics.h>
#include <GnWireJson.h>
#include <GnUdpSchema.h>
#include <map>

inline bool GnUdpSourceId(TGNString const & Id)
 { return GnWireId(Id,true); }
inline bool GnUdpSourceRevision(TGNString const & Revision)
 { return GnRevision(Revision); }
class TGnUdpSources
 {
  struct TClaim { TGNString Source,Revision;bool Conflict=false; };
  struct TQuery { uint64_t Last=0;unsigned Attempts=0; };
  std::map<TGNString,TClaim> Claims;
  std::map<TGNString,TQuery> Queries;
 public:
  bool Accept(TGNString const & Text)
   {
    TGnWireDocument D(Text);return GnTypeValid(D.Root(),TGnJsonType<TUdpSourceInfoMessage>{}) && Accept(D.Root());
   }
  bool Accept(TGnWireValue R)
   {
    auto F=R.Get("Func").Text(),S=R.Get("Src").Text(),V=R.Get("Rev").Text();
    if(F.empty()||F.size()>80||!GnUdpSourceId(S)||!GnUdpSourceRevision(V)||(!Claims.count(F)&&Claims.size()>=32))return false;
    auto & C=Claims[F];
    if(V>C.Revision){C={S,V,false};return true;}
    if(V==C.Revision&&S!=C.Source)C.Conflict=true;
    return true;
   }
  TGNString Resolve(TGNString const & Function,TGNString const & Preferred,TGNString const & Revision,
                    uint64_t Now,TGNString & Request)
   {
    Request.clear();auto C=Claims.find(Function);
    if(C!=Claims.end()&&!C->second.Conflict&&C->second.Revision>=Revision)
     {
      if(C->second.Revision==Revision&&!Preferred.empty()&&Preferred!=C->second.Source)return Preferred;
      return C->second.Source;
     }
    if(!Preferred.empty())return Preferred;
    if(Function.empty()||Function.size()>80||(!Queries.count(Function)&&Queries.size()>=32))return {};
    auto & Q=Queries[Function];
    if(!Q.Attempts||Now-Q.Last>=(Q.Attempts<3?3000:30000))
     {
      TUdpSourceRequestMessage Query;Query.Func=Function;
      Request=GnWriteUdpMessage(Query);Q.Last=Now;++Q.Attempts;
     }
    return {};
   }
 };
inline TGNString GnUdpSourceReply(TGNString const & Owner,TGNString const & Function,TGNString const & Source,TGNString const & Revision)
 {
  if(!GnUdpSourceId(Source)||!GnUdpSourceRevision(Revision))return {};
  TUdpSourceInfoMessage Reply;Reply.ID=Owner;Reply.Func=Function;Reply.Src=Source;
  Reply.Rev=Revision;
  return GnWriteUdpMessage(Reply);
 }
