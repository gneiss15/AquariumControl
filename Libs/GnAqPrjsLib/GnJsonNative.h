// GnJsonNative.h, Version: 1.01
#pragma once
#include <GnBasics.h>
#include <GnJsonRules.h>
#include <GnJsonObjectCodec.h>
#include <tuple>
#include <cstring>

enum class TGnJsonShape { Object, Tuple, Value };
struct TGnJsonBaseSentinel {};
struct TGnJsonExtra { TGNString ExtraJson; };
template<class... T> struct TGnJsonBases;
template<> struct TGnJsonBases<TGnJsonBaseSentinel> : TGnJsonExtra {};
template<class T> struct TGnJsonBases<TGnJsonBaseSentinel,T> : T {};
template<class T> constexpr auto GnJsonFields(TGnJsonType<T>);

template<class T> struct TGnJsonContext
 {
  TGnWireValue Input;
  TGnWireValue Get(char const * Name) const
   {
    if constexpr(T::JsonShape!=TGnJsonShape::Tuple)return Input.Get(Name);
    else
     {
      size_t Index=0;TGnWireValue Result;
      std::apply([&](auto const &... Field)
       {((std::strcmp(Name,Field.Name)==0 ? void(Result=Input.At(Index++)) : void(++Index)),...);},GnJsonFields(TGnJsonType<T>{}));
      return Result;
     }
   }
  bool Has(char const * Name) const
   {
    if constexpr(T::JsonShape!=TGnJsonShape::Tuple)return Input.Has(Name);
    else
     {
      size_t Index=0;bool Found=false;
      std::apply([&](auto const &... Field)
       {((Found=Found || (Index<Input.Size() && std::strcmp(Name,Field.Name)==0),++Index),...);},GnJsonFields(TGnJsonType<T>{}));
      return Found;
     }
   }
 };
template<class T> constexpr bool GnJsonOwnKey(char const * Name)
 {
  return std::apply([&](auto const &... Field){return ((std::strcmp(Name,Field.Name)==0) || ...);},GnJsonOwnFields(TGnJsonType<T>{}));
 }
template<class T,class TField> struct TGnJsonInheritedMember : TField
 {
  bool Valid(TGnWireValue Input,bool Partial) const
   {return GnJsonOwnKey<T>(this->Name) || TField::Valid(Input,Partial);}
  bool Read(TGnWireValue Input,T & Output,TGNString & Error) const
   {return GnJsonOwnKey<T>(this->Name) || TField::Read(Input,Output,Error);}
  void Store(TGnWireValue Output,T const & Value) const
   {if(!GnJsonOwnKey<T>(this->Name))TField::Store(Output,Value);}
 };
template<class T,class... TFields>
constexpr auto GnJsonInheritedFields(std::tuple<TFields...> const & Fields)
 {
  return std::apply([](auto const &... Field)
   {return std::make_tuple(TGnJsonInheritedMember<T,TFields>{Field}...);},Fields);
 }
template<class T> constexpr auto GnJsonFields(TGnJsonType<T> Tag)
 {return std::tuple_cat(GnJsonBaseFields(Tag),GnJsonOwnFields(Tag));}

template<class T> bool GnTypeValid(TGnWireValue Input,TGnJsonType<T>,bool Partial=false)
 requires requires { T::JsonShape; }
 {
  if constexpr(T::JsonShape==TGnJsonShape::Value)
    return GnTypeValid(Input,TGnJsonType<decltype(T::Value)>{}) && GnJsonRule<T::JsonRule>(Input,Input);
  else if constexpr(T::JsonShape==TGnJsonShape::Object)return GnJsonObjectValid<T>(Input,Partial);
  else
   {
    if(!Input.Array())return false;
    const auto Fields=GnJsonFields(TGnJsonType<T>{});
    constexpr size_t Size=std::tuple_size_v<decltype(Fields)>;
    if(Input.Size()>Size)return false;
    size_t Index=0;
    return std::apply([&](auto const &... Field)
     {
      return (([&] {
        const size_t Column=Index++;
        if(Column>=Input.Size())return !Field.Required;
        auto Value=Input.At(Column);
        return (Value.Null() && Field.Nullable) ||
          (GnTypeValid(Value,TGnJsonType<typename TGnJsonMemberValue<std::remove_cvref_t<decltype(T{}.*Field.Member)>>::TValue>{}) && Field.Rule(Input,Value));
      }()) && ...);
     },Fields);
   }
 }
template<class T> bool GnRead(TGnWireValue Input,T & Output,TGNString & Error)
 requires requires { T::JsonShape; }
 {
  if constexpr(T::JsonShape==TGnJsonShape::Value)
    return GnTypeValid(Input,TGnJsonType<T>{}) && GnRead(Input,Output.Value,Error);
  else if constexpr(T::JsonShape==TGnJsonShape::Object)return GnJsonObjectRead(Input,Output,Error);
  else
   {
    if(!GnTypeValid(Input,TGnJsonType<T>{}))return false;
    T Next{};size_t Index=0;
    const bool Valid=std::apply([&](auto const &... Field)
     {return (([&] {
       const size_t Column=Index++;
       if(Column>=Input.Size())return true;
       auto Value=Input.At(Column);auto & Member=Next.*Field.Member;
       if constexpr(TGnJsonMemberValue<std::remove_reference_t<decltype(Member)>>::Wrapped)
        {
         Member.Present=true;Member.IsNull=Value.Null();
         return Member.IsNull || GnRead(Value,Member.Value,Error);
        }
       else return GnRead(Value,Member,Error);
     }()) && ...);},GnJsonFields(TGnJsonType<T>{}));
    if(Valid)Output=std::move(Next);
    return Valid;
   }
 }
template<class T> void GnAppendJson(TGnWireValue Array,T const & Value)
 requires requires { T::JsonShape; } && (T::JsonShape==TGnJsonShape::Object)
 {
  // Write into the destination document without allocating a second JSON tree.
  GnJsonObjectStore(Array.AddObject(),Value);
 }

template<class T> void GnStore(TGnWireValue Parent,TGNString const & Key,T const & Value)
 requires requires { T::JsonShape; }
 {
  if constexpr(T::JsonShape==TGnJsonShape::Value)GnStore(Parent,Key,Value.Value);
  else if constexpr(T::JsonShape==TGnJsonShape::Object)GnJsonObjectStore(Parent.NewObject(Key),Value);
  else
   {
    auto Array=Parent.NewArray(Key);bool End=false;
    std::apply([&](auto const &... Field) {
      (([&] {
        if(End)return;
        auto const & Member=Value.*Field.Member;
        if constexpr(TGnJsonMemberValue<std::remove_cvref_t<decltype(Member)>>::Wrapped)
         {
          if(!Member.Present){End=true;return;}
          if(Member.IsNull)GnAppendJson(Array,nullptr);else GnAppendJson(Array,Member.Value);
         }
        else GnAppendJson(Array,Member);
      }()),...);
    },GnJsonFields(TGnJsonType<T>{}));
   }
 }
template<class T> TGNString GnWriteJson(T const & Value)
 {
  TGnWireDocument Document;
  GnStore(Document.Root(),"Value",Value);
  auto Output=Document.Root().Get("Value");
  return GnTypeValid(Output,TGnJsonType<T>{})?Output.Json():TGNString{};
 }
template<class T,class THandle> void GnForEachDefinedMember(T const & Value,THandle Handle)
 {GnJsonForEachMember(Value,Handle);}
