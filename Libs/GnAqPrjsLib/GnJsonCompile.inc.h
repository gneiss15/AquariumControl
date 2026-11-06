// GnJsonCompile.inc.h, Version: 1.02
// Repeated compiler expansion of the selected message definitions.
#define ArrayOf(Type) std::vector<Type>
#define MapOf(Type) std::map<TGNString,Type>
#define VariantOf(...) std::variant<__VA_ARGS__>

// Forward declarations make dependencies visible to every compiler pass.
#define JD(...)
#define JD_BEGIN(Name, Description) template<bool> struct GnImpl_##Name; using Name=GnImpl_##Name<false>;
#define JD_TUPLE_BEGIN(Name, Description) JD_BEGIN(Name, Description)
#define JD_VALUE(Name, Type, Rule, Description) JD_BEGIN(Name, Description)
#include "GnJsonCatalog.inc.h"

#define GN_JSON_OPTIONAL_Required false
#define GN_JSON_OPTIONAL_Optional true
#define GN_JSON_WRAPPED_Required true
#define GN_JSON_WRAPPED_Optional true
#define GN_JSON_NULL_Required true
#define GN_JSON_NULL_Optional true
#define GN_JSON_BASE_Keep false
#define GN_JSON_BASE_Optional true

// Base implementations retain the same field policy when a whole view is optional.
#define JD(...)
#define JD_BEGIN(Name, Description) template<bool AllOptional> struct GnBases_##Name : TGnJsonBases<TGnJsonBaseSentinel
#define JD_TUPLE_BEGIN(Name, Description) JD_BEGIN(Name, Description)
#define JD_VALUE(Name, Type, Rule, Description) template<bool> struct GnBases_##Name : TGnJsonExtra {};
#define JD_BASE(Name, Presence) ,GnImpl_##Name<AllOptional || GN_JSON_BASE_##Presence>
#define JD_END(Name) > {};
#include "GnJsonCatalog.inc.h"

// Actual C++ storage.
#define JD_BEGIN(Name, Description) template<bool AllOptional> struct GnImpl_##Name : GnBases_##Name<AllOptional> { static constexpr TGnJsonShape JsonShape=TGnJsonShape::Object;
#define JD_TUPLE_BEGIN(Name, Description) template<bool AllOptional> struct GnImpl_##Name : GnBases_##Name<AllOptional> { static constexpr TGnJsonShape JsonShape=TGnJsonShape::Tuple;
#define JD(Type, Member, Presence, Rule, Description, Part) std::conditional_t<AllOptional || GN_JSON_WRAPPED_##Presence,TGnJsonField<Type>,Type> Member{};
#define JD_CONST(Type, Member, Value) Type Member=Value;
#define JD_END(Name) };
#define JD_VALUE(Name, Type, Rule, Description)   template<bool AllOptional> struct GnImpl_##Name : TGnJsonExtra {     static constexpr TGnJsonShape JsonShape=TGnJsonShape::Value;     static constexpr TGnJsonRuleText JsonRule{#Rule};     Type Value{};     GnImpl_##Name(void) = default;     GnImpl_##Name(Type Input) : Value(std::move(Input)) {}     operator Type() const requires std::is_arithmetic_v<Type> { return Value; }   };
#ifndef GN_UDP_BIT_CONSTANTS_DEFINED
  #define GN_UDP_BIT_CONSTANTS_DEFINED
  #define JD_BIT(Name, Value, Description) constexpr uint8_t GnUdp##Name=Value;
#endif
#include "GnJsonCatalog.inc.h"

// Own fields and their compile-time rules. Inheritance is handled in the next pass.
#define JD_BEGIN(Name, Description) template<bool AllOptional> constexpr auto GnJsonOwnFields(TGnJsonType<GnImpl_##Name<AllOptional>>) { using TObject [[maybe_unused]]=GnImpl_##Name<AllOptional>; return std::tuple_cat(std::tuple<>{}
#define JD_TUPLE_BEGIN(Name, Description) JD_BEGIN(Name, Description)
#define JD(Type, Member, Presence, Rule, Description, Part) ,std::make_tuple(GnJsonMember(#Member,&TObject::Member,!AllOptional && !GN_JSON_OPTIONAL_##Presence,GN_JSON_NULL_##Presence,     +[](TGnWireValue Input,TGnWireValue Field){return GnJsonRule<#Rule>(TGnJsonContext<TObject>{Input},Field);},     +[](TGnWireValue Input){return GnJsonRequired<#Rule>(TGnJsonContext<TObject>{Input});},#Part))
#define JD_CONST(Type, Member, Value)   ,std::make_tuple(GnJsonMember(#Member,&TObject::Member,!AllOptional,false,     +[](TGnWireValue,TGnWireValue Field){return GnJsonConstant(Field,Type(Value));},     +[](TGnWireValue){return false;},"Live",     +[](TGnWireValue Output,char const * Key){GnStore(Output,Key,Type(Value));}))
#define JD_END(Name) ); }
#define JD_VALUE(Name, Type, Rule, Description) template<bool P> constexpr auto GnJsonOwnFields(TGnJsonType<GnImpl_##Name<P>>) {return std::tuple<>{};}
#include "GnJsonCatalog.inc.h"

#define JD(...)
#define JD_BEGIN(Name, Description) template<bool AllOptional> constexpr auto GnJsonBaseFields(TGnJsonType<GnImpl_##Name<AllOptional>>) { using TObject [[maybe_unused]]=GnImpl_##Name<AllOptional>;return std::tuple_cat(std::tuple<>{}
#define JD_TUPLE_BEGIN(Name, Description) JD_BEGIN(Name, Description)
#define JD_BASE(Name, Presence) ,GnJsonInheritedFields<TObject>(GnJsonFields(TGnJsonType<GnImpl_##Name<AllOptional || GN_JSON_BASE_##Presence>>{}))
#define JD_END(Name) ); }
#define JD_VALUE(Name, Type, Rule, Description) template<bool P> constexpr auto GnJsonBaseFields(TGnJsonType<GnImpl_##Name<P>>) {return std::tuple<>{};}
#include "GnJsonCatalog.inc.h"

#undef ArrayOf
#undef MapOf
#undef VariantOf
#undef GN_JSON_OPTIONAL_Required
#undef GN_JSON_OPTIONAL_Optional
#undef GN_JSON_WRAPPED_Required
#undef GN_JSON_WRAPPED_Optional
#undef GN_JSON_NULL_Required
#undef GN_JSON_NULL_Optional
#undef GN_JSON_BASE_Keep
#undef GN_JSON_BASE_Optional
