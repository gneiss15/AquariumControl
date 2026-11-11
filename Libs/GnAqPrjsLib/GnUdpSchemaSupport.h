// GnUdpSchemaSupport.h, Version: 1.07
#pragma once
#include <GnBasics.h>
#include <GnWireJson.h>
#include <cmath>
#include <limits>
#include <type_traits>
#include <map>
#include <vector>
#include <variant>
#include <memory>
#include <new>

// Missing, explicit null and a real value remain distinct.
template<class T> struct TGnJsonField
 {
  T Value{};
  bool Present = false;
  bool IsNull = false;
  TGnJsonField & operator=(T const & Next)
   { Value=Next;Present=true;IsNull=false;return *this; }
  TGnJsonField & operator=(std::nullptr_t)
   { Present=true;IsNull=true;return *this; }
  bool Known(void) const {return Present && !IsNull;}
  operator T const &() const {return Value;}
  template<class U> bool operator==(U const & Other) const requires requires {Value==Other;}
   {return Value==Other;}
  bool operator==(TGnJsonField const & Other) const {return Value==Other.Value;}
  auto begin(void) const requires requires {Value.begin();} {return Value.begin();}
  auto end(void) const requires requires {Value.end();} {return Value.end();}
  auto begin(void) requires requires {Value.begin();} {Present=true;return Value.begin();}
  auto end(void) requires requires {Value.end();} {return Value.end();}
  bool empty(void) const requires requires {Value.empty();} {return Value.empty();}
  auto size(void) const requires requires {Value.size();} {return Value.size();}
  auto const & front(void) const requires requires {Value.front();} {return Value.front();}
  template<class U> void push_back(U && Item) requires requires {Value.push_back(std::forward<U>(Item));}
   {Present=true;IsNull=false;Value.push_back(std::forward<U>(Item));}
  template<class U> auto & operator[](U const & Key) requires requires {Value[Key];}
   {Present=true;IsNull=false;return Value[Key];}

 };
struct TGnChannelId { TGNString Json; };
struct TGnJsonNull {};
template<class T> struct TGnJsonType {};
inline bool GnTypeValid(TGnWireValue Value,TGnJsonType<TGnJsonNull>) {return Value.Null();}
inline bool GnRead(TGnWireValue Value,TGnJsonNull &,TGNString &) {return Value.Null();}
inline void GnStore(TGnWireValue Parent,TGNString const & Key,TGnJsonNull const &) {Parent.SetNull(Key);}

inline bool GnTypeValid(TGnWireValue V,TGnJsonType<TGNString>) { return V.IsText(); }
inline bool GnTypeValid(TGnWireValue V,TGnJsonType<bool>) { return V.IsBool(); }
inline bool GnTypeValid(TGnWireValue V,TGnJsonType<double>) { return V.IsNumber() && std::isfinite(V.Number()); }
template<class T> bool GnTypeValid(TGnWireValue V,TGnJsonType<TGnJsonField<T>>)
 {return V.Null() || GnTypeValid(V,TGnJsonType<T>{});}
template<class T> bool GnRead(TGnWireValue V,TGnJsonField<T> & X,TGNString & Error)
 {
  TGnJsonField<T> Next;Next.Present=true;Next.IsNull=V.Null();
  if(!Next.IsNull && !GnRead(V,Next.Value,Error))return false;
  X=std::move(Next);return true;
 }
template<class T> void GnStore(TGnWireValue Parent,TGNString const & Key,TGnJsonField<T> const & X)
 {if(!X.Present || X.IsNull)Parent.SetNull(Key);else GnStore(Parent,Key,X.Value);}

template<class T> requires std::is_unsigned_v<T>
bool GnTypeValid(TGnWireValue V,TGnJsonType<T>) { return V.IsUnsigned() && V.UInt()<=std::numeric_limits<T>::max(); }
inline bool GnTypeValid(TGnWireValue V,TGnJsonType<TGnChannelId>) { return V.IsText() || V.IsUnsigned(); }
template<class T> bool GnTypeValid(TGnWireValue V,TGnJsonType<std::vector<T>>)
 {
  if(!V.Array())return false;
  for(auto X:V.Items())if(!GnTypeValid(X,TGnJsonType<T>{}))return false;
  return true;
 }
template<class T> bool GnTypeValid(TGnWireValue V,TGnJsonType<std::map<TGNString,T>>)
 {
  if(!V.Object())return false;
  for(auto const & X:V.Members())if(!GnTypeValid(X.second,TGnJsonType<T>{}))return false;
  return true;
 }
inline bool GnRead(TGnWireValue V,TGNString & X,TGNString &) { if(!V.IsText())return false;X=V.Text();return true; }
inline bool GnRead(TGnWireValue V,bool & X,TGNString &) { if(!V.IsBool())return false;X=V.Bool();return true; }
inline bool GnRead(TGnWireValue V,double & X,TGNString &) { if(!GnTypeValid(V,TGnJsonType<double>{}))return false;X=V.Number();return true; }
template<class T> requires std::is_unsigned_v<T>
bool GnRead(TGnWireValue V,T & X,TGNString &) { if(!GnTypeValid(V,TGnJsonType<T>{}))return false;X=static_cast<T>(V.UInt());return true; }
inline bool GnRead(TGnWireValue V,TGnChannelId & X,TGNString &) { if(!GnTypeValid(V,TGnJsonType<TGnChannelId>{}))return false;X.Json=V.Json();return true; }
template<class T> bool GnRead(TGnWireValue V,std::vector<T> & X,TGNString & Error)
 {
  if(!V.Array())return false;
  std::vector<T> Next;
  for(auto Item:V.Items()) { T Value{};if(!GnRead(Item,Value,Error))return false;Next.push_back(std::move(Value)); }
  X=std::move(Next);return true;
 }
template<class T> bool GnRead(TGnWireValue V,std::map<TGNString,T> & X,TGNString & Error)
 {
  if(!V.Object())return false;
  std::map<TGNString,T> Next;
  for(auto Item:V.Members()) { T Value{};if(!GnRead(Item.second,Value,Error))return false;Next.emplace(Item.first,std::move(Value)); }
  X=std::move(Next);return true;
 }
inline void GnStore(TGnWireValue O,TGNString const & K,TGNString const & X) { O.Text(K,X); }
inline void GnStore(TGnWireValue O,TGNString const & K,bool X) { O.Bool(K,X); }
inline void GnStore(TGnWireValue O,TGNString const & K,double X) { O.Number(K,X); }
template<class T> requires std::is_unsigned_v<T>
void GnStore(TGnWireValue O,TGNString const & K,T X) { O.UInt(K,X); }
inline void GnStore(TGnWireValue O,TGNString const & K,TGnChannelId const & X) { TGnWireDocument D(X.Json);O.Set(K,D.Root()); }
template<class T> void GnAppendJson(TGnWireValue Output,T const & Value);

template<class T> void GnStore(TGnWireValue O,TGNString const & K,std::vector<T> const & X)
 {
  auto Array=O.NewArray(K);
  for(auto const & Item:X) GnAppendJson(Array,Item);
 }
template<class T> void GnStore(TGnWireValue O,TGNString const & K,std::map<TGNString,T> const & X)
 { auto M=O.NewObject(K);for(auto const & Item:X)GnStore(M,Item.first,Item.second); }
inline bool GnKnownKey(TGNString const & K,std::initializer_list<char const *> Keys)
 { for(auto Key:Keys)if(K==Key)return true;return false; }
inline TGNString GnExtraFields(TGnWireValue Input,std::initializer_list<char const *> Keys)
 {
  TGnWireDocument Extra;bool Any=false;
  for(auto const & P:Input.Members())if(!GnKnownKey(P.first,Keys)) { Extra.Root().Set(P.first,P.second);Any=true; }
  return Any?Extra.Root().Json():TGNString{};
 }
inline void GnRestoreExtra(TGnWireValue Output,TGNString const & Text)
 { if(!Text.empty()) { TGnWireDocument D(Text);for(auto const & P:D.Root().Members())Output.Set(P.first,P.second); } }
inline bool GnHex16(TGNString const & X)
 {
  if(X.size()!=16)return false;
  for(char C:X)if(!((C>='0'&&C<='9')||(C>='a'&&C<='f')||(C>='A'&&C<='F')))return false;
  return true;
 }
inline bool GnWireId(TGNString const & X,bool Source=false)
 {
  if(X.size()!=19||X[17]!='@'||(Source?X[18]!='3':X[18]<'1'||X[18]>'4'))return false;
  bool Nonzero=false;
  for(size_t I=0;I<17;++I) { char C=X[I];if(I%3==2) { if(C!=':')return false; }
    else { if(!((C>='0'&&C<='9')||(C>='A'&&C<='F')))return false;Nonzero=Nonzero||C!='0'; } }
  return Nonzero;
 }
inline bool GnRevision(TGNString const & X)
 {
  if(X.size()!=24)return false;
  for(size_t I=0;I<24;++I)
   {
    const char Expected=I==4||I==7?'-':I==10?'T':I==13||I==16?':':I==19?'.':I==23?'Z':0;
    if(Expected?X[I]!=Expected:X[I]<'0'||X[I]>'9')return false;
   }
  const auto Number=[&](size_t Start,size_t Count)
   {
    unsigned Result=0;
    for(size_t I=Start;I<Start+Count;++I)Result=Result*10+unsigned(X[I]-'0');
    return Result;
   };
  const unsigned Year=Number(0,4),Month=Number(5,2),Day=Number(8,2);
  if(!Year || !Month || Month>12 || Number(11,2)>23 || Number(14,2)>59 || Number(17,2)>59)return false;
  constexpr unsigned Days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  const bool Leap=Year%4==0 && (Year%100!=0 || Year%400==0);
  return Day>=1 && Day<=Days[Month-1]+unsigned(Month==2 && Leap);
 }

inline bool GnItemsHave(TGnWireValue Array,TGNString const & Key)
 { for(auto Value:Array.Items())if(!Value.Has(Key))return false;return true; }

// Format checks shared by generated configuration and message validators.
inline bool GnServiceId(TGNString const & Value,std::initializer_list<unsigned> Services)
 {
  if(!GnWireId(Value))return false;
  for(auto Service:Services)if(Value.back()==char('0'+Service))return true;
  return false;
 }
inline bool GnEmptyOrUuid(TGNString const & Value)
 {
  if(Value.empty())return true;
  if(Value.size()!=36)return false;
  for(size_t I=0;I<Value.size();++I)
   {
    const char C=Value[I];
    if(I==8||I==13||I==18||I==23) { if(C!='-')return false; }
    else if(!((C>='0'&&C<='9')||(C>='a'&&C<='f')||(C>='A'&&C<='F')))return false;
   }
  return true;
 }
inline bool GnSensorId(TGNString const & Value)
 {
  if(Value.size()!=15||Value.substr(0,3)!="28-")return false;
  for(size_t I=3;I<Value.size();++I)
   { const char C=Value[I];if(!((C>='0'&&C<='9')||(C>='a'&&C<='f')||(C>='A'&&C<='F')))return false; }
  return true;
 }
inline bool GnIpv4(TGNString const & Value)
 {
  unsigned Parts=0,Digits=0,Number=0;
  for(char C:Value)
   {
    if(C=='.') { if(!Digits||++Parts>3)return false;Digits=Number=0; }
    else { if(C<'0'||C>'9'||++Digits>3)return false;Number=Number*10+unsigned(C-'0');if(Number>255)return false; }
   }
  return Parts==3&&Digits!=0;
 }

template<class T> bool GnReadJsonValue(TGNString const & Text,T & Value)
 {
  TGnWireDocument Document(Text);
  TGNString Error;
  return Document.Valid() && GnRead(Document.Root(),Value,Error);
 }
template<class T> bool GnReadJsonValue(TGNString const & Text,TGnJsonField<T> & Field)
 {
  TGnWireDocument Document(Text);
  if(!Document.Valid())return false;
  if(Document.Root().Null()) { Field=nullptr;return true; }
  T Next{};
  TGNString Error;
  if(!GnRead(Document.Root(),Next,Error))return false;
  Field=Next;
  return true;
 }

inline bool GnMeasurementName(TGNString const & Value)
 {
  if(Value.empty()||Value.size()>64||Value=="Kind")return false;
  for(char C:Value)if(!((C>='a'&&C<='z')||(C>='A'&&C<='Z')||(C>='0'&&C<='9')||C=='_'))return false;
  return true;
 }

template<class T> void GnAppendJson(TGnWireValue Output,T const & Value)
 {
  TGnWireDocument Document;
  GnStore(Document.Root(),"Value",Value);
  Output.AddValue(Document.Root().Get("Value"));
 }
inline void GnAppendJson(TGnWireValue Output,std::nullptr_t)
 { TGnWireDocument Document("null");Output.AddValue(Document.Root()); }
template<class T> bool GnEachJson(TGnWireValue Input,T Check)
 {
  if(Input.Array()) { for(auto Value:Input.Items())if(!Check(Value))return false; }
  else if(Input.Object()) { for(auto const & Value:Input.Members())if(!Check(Value.second))return false; }
  else return false;
  return true;
 }
inline bool GnDuration(TGNString const & Value)
 {
  if(Value.size()!=5||Value[2]!=':')return false;
  for(unsigned I:{0u,1u,3u,4u})if(Value[I]<'0'||Value[I]>'9')return false;
  return Value[3]<'6';
 }
inline bool GnTimeOfDay(TGNString const & Value)
 { return GnDuration(Value) && (Value[0]<'2'||(Value[0]=='2'&&Value[1]<'4')); }

template<class... T> bool GnTypeValid(TGnWireValue Input,TGnJsonType<std::variant<T...>>)
 { return (GnTypeValid(Input,TGnJsonType<T>{}) || ...); }
template<class T,class... V> bool GnReadJsonChoice(TGnWireValue Input,std::variant<V...> & Target,TGNString & Error)
 {
  if(!GnTypeValid(Input,TGnJsonType<T>{}))return false;
  T Next{};
  if(!GnRead(Input,Next,Error))return false;
  Target=std::move(Next);
  return true;
 }
template<class... T> bool GnRead(TGnWireValue Input,std::variant<T...> & Value,TGNString & Error)
 { return (GnReadJsonChoice<T>(Input,Value,Error) || ...); }
template<class... T> void GnStore(TGnWireValue Parent,TGNString const & Key,std::variant<T...> const & Value)
 { std::visit([&](auto const & Item){GnStore(Parent,Key,Item);},Value); }

// Opaque provenance/extension JSON is accepted only from an already parsed node.
class TGnJsonValue
 {
  TGNString Text = "null";
  friend bool GnRead(TGnWireValue,TGnJsonValue &,TGNString &);
  friend void GnStore(TGnWireValue,TGNString const &,TGnJsonValue const &);
 };
inline bool GnTypeValid(TGnWireValue,TGnJsonType<TGnJsonValue>) { return true; }
inline bool GnRead(TGnWireValue Input,TGnJsonValue & Value,TGNString &)
 { Value.Text=Input.Json();return true; }
inline void GnStore(TGnWireValue Parent,TGNString const & Key,TGnJsonValue const & Value)
 { TGnWireDocument Document(Value.Text);Parent.Set(Key,Document.Root()); }
