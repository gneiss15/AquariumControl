// GnUdpSources.h, Version: 1.00
#pragma once
#include <GnBasics.h>
#include <GnWireJson.h>
#include <map>

inline bool GnUdpSourceId(TGNString const & Id)
 {
  if(Id.size()!=19||Id.substr(17)!="@3")return false;
  bool Nonzero=false;
  for(unsigned I=0;I<17;++I)
   {if(I%3==2){if(Id[I]!=':')return false;}else {if(!((Id[I]>='0'&&Id[I]<='9')||(Id[I]>='A'&&Id[I]<='F')))return false;Nonzero=Nonzero||Id[I]!='0';}}
  return Nonzero;
 }
inline bool GnUdpSourceRevision(TGNString const & R)
 {
  if(R.size()!=24)return false;
  for(unsigned I=0;I<24;++I)
   {
    const char Expected=I==4||I==7?'-':I==10?'T':I==13||I==16?':':I==19?'.':I==23?'Z':0;
    if(Expected ? R[I]!=Expected : !(R[I]>='0'&&R[I]<='9'))return false;
   }
  return true;
 }
class TGnUdpSources
 {
  struct TClaim { TGNString Source,Revision;bool Conflict=false; };
  struct TQuery { uint64_t Last=0;unsigned Attempts=0; };
  std::map<TGNString,TClaim> Claims;
  std::map<TGNString,TQuery> Queries;
 public:
  bool Accept(TGNString const & Text)
   {
    TGnWireDocument D(Text);auto R=D.Root();
    if(R.Get("Version").UInt()!=2||R.Get("Type").Text()!="SourceInfo"||!R.Get("Confirmed").Bool())return false;
    auto F=R.Get("Function").Text(),S=R.Get("Source").Text(),V=R.Get("Revision").Text();
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
      TGnWireDocument D;auto R=D.Root();R.UInt("Version",2);R.Text("Type","SourceRequest");R.Text("Function",Function);
      Request=R.Json();Q.Last=Now;++Q.Attempts;
     }
    return {};
   }
 };
inline TGNString GnUdpSourceReply(TGNString const & Owner,TGNString const & Function,TGNString const & Source,TGNString const & Revision)
 {
  if(!GnUdpSourceId(Source)||!GnUdpSourceRevision(Revision))return {};
  TGnWireDocument D;auto R=D.Root();R.UInt("Version",2);R.Text("Type","SourceInfo");R.Text("ID",Owner);
  R.Text("Function",Function);R.Text("Source",Source);R.Text("Revision",Revision);R.Bool("Confirmed",true);return R.Json();
 }
