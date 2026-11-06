// GnJsonObjectCodec.h, Version: 1.01
#pragma once
#include <GnBasics.h>
#include <GnUdpSchemaSupport.h>
#include <tuple>

// The generated metadata supplies names, members and rules. These algorithms
// are shared by all object types; tuple/root-value codecs have their own shape.
template<class T> struct TGnJsonMemberValue
 {
  using TValue=T;
  static constexpr bool Wrapped=false;
 };
template<class T> struct TGnJsonMemberValue<TGnJsonField<T>>
 {
  using TValue=T;
  static constexpr bool Wrapped=true;
 };
template<class TObject,class TMember> struct TGnJsonMember
 {
  char const * Name;
  TMember TObject::* Member;
  bool Required;
  bool Nullable;
  bool (*Rule)(TGnWireValue,TGnWireValue);
  bool (*RequiredWhen)(TGnWireValue);
  char const * Part="Live";
  void (*Initialize)(TGnWireValue,char const *)=nullptr;

  bool Valid(TGnWireValue Input,bool Partial) const
   {
    if(!Input.Has(Name))return !(RequiredWhen(Input) || (Required && !Partial));
    auto Field=Input.Get(Name);
    if(Field.Null() && Nullable)return true;
    return GnTypeValid(Field,TGnJsonType<typename TGnJsonMemberValue<TMember>::TValue>{}) && Rule(Input,Field);
   }
  bool Read(TGnWireValue Input,TObject & Output,TGNString & Error) const
   {
    if(!Input.Has(Name))return !Required && !RequiredWhen(Input);
    auto Field=Input.Get(Name);
    auto & Target=Output.*Member;
    if constexpr(TGnJsonMemberValue<TMember>::Wrapped)
     {
      Target.Present=true;Target.IsNull=Field.Null();
      if(Target.IsNull && Nullable)return true;
      return Rule(Input,Field) && GnRead(Field,Target.Value,Error);
     }
    else return Rule(Input,Field) && GnRead(Field,Target,Error);
   }
  void Store(TGnWireValue Output,TObject const & Value) const
   {
    auto const & Source=Value.*Member;
    if constexpr(TGnJsonMemberValue<TMember>::Wrapped)
     {
      if(!Source.Present)return;
      if(Source.IsNull)Output.SetNull(Name);
      else GnStore(Output,Name,Source.Value);
     }
    else GnStore(Output,Name,Source);
   }
 };
template<class TObject,class TMember>
constexpr auto GnJsonMember(char const * Name,TMember TObject::* Member,bool Required,bool Nullable,
                 bool (*Rule)(TGnWireValue,TGnWireValue),bool (*RequiredWhen)(TGnWireValue),
                 char const * Part="Live",void (*Initialize)(TGnWireValue,char const *)=nullptr)
 {return TGnJsonMember<TObject,TMember>{Name,Member,Required,Nullable,Rule,RequiredWhen,Part,Initialize};}

template<class T> auto const & GnJsonObjectFields(void)
 {
  static constexpr auto Fields=GnJsonFields(TGnJsonType<T>{});
  return Fields;
 }
template<class T> bool GnJsonObjectValid(TGnWireValue Input,bool Partial)
 {
  if(!Input.Object())return false;
  return std::apply([&](auto const &... Fields){return (Fields.Valid(Input,Partial) && ...);},GnJsonObjectFields<T>());
 }
template<class T> bool GnJsonReadMembers(TGnWireValue Input,T & Next,TGNString & Error)
 {
  auto const & Fields=GnJsonObjectFields<T>();
  if(!std::apply([&](auto const &... Field){return (Field.Read(Input,Next,Error) && ...);},Fields))return false;
  Next.ExtraJson=std::apply([&](auto const &... Field){return GnExtraFields(Input,{Field.Name...});},Fields);
  return true;
 }
template<class T> bool GnJsonObjectRead(TGnWireValue Input,T & Value,TGNString & Error)
 {
  if(!Input.Object())return false;
  // Large status/configuration objects must not consume the ESP task stack.
  if constexpr(sizeof(T)>512)
   {
    std::unique_ptr<T> Next(new(std::nothrow) T{});
    if(!Next || !GnJsonReadMembers(Input,*Next,Error))return false;
    Value=std::move(*Next);
   }
  else
   {
    T Next{};
    if(!GnJsonReadMembers(Input,Next,Error))return false;
    Value=std::move(Next);
   }
  return true;
 }
template<class T> void GnJsonObjectStore(TGnWireValue Parent,TGNString const & Key,T const & Value)
 {
  auto Output=Parent.NewObject(Key);
  GnRestoreExtra(Output,Value.ExtraJson);
  std::apply([&](auto const &... Field){(Field.Store(Output,Value),...);},GnJsonObjectFields<T>());
 }
template<class T,class THandle> void GnJsonForEachMember(T const & Value,THandle Handle)
 {
  std::apply([&](auto const &... Field){(Handle(Field.Name,Value.*(Field.Member)),...);},GnJsonObjectFields<T>());
 }
