// MessageTypes.h, Version: 1.00
#pragma once
#include <GnBasics.h>
#include <GnUdpSchemaSupport.h>
#include <type_traits>

using TJsonValue=TGnJsonValue;
using TJsonNull=TGnJsonNull;

#define JD_ALIAS(Name, Type, Rule, Description) using Name=Type;
#define JD_ENUM_BEGIN(Name, Type, Description) enum class Name : Type {
#define JD_ENUM(Name, Value, Description) Name=Value,
#define JD_ENUM_END(Name) };
#include "Definitions/Contract/SharedTypes.inc.h"

// Enum codecs retain the numeric JSON representation and reject end markers.
#define JD_ENUM_BEGIN(Name, Type, Description)   inline bool GnTypeValid(TGnWireValue Input,TGnJsonType<Name>,bool=false)    {return GnTypeValid(Input,TGnJsonType<Type>{}) && Input.Number() < static_cast<Type>(Name::Name##_MAX);}   inline bool GnRead(TGnWireValue Input,Name & Output,TGNString &)    {if(!GnTypeValid(Input,TGnJsonType<Name>{}))return false;Output=static_cast<Name>(Input.Number());return true;}   inline void GnStore(TGnWireValue Parent,TGNString const & Key,Name Value)    {GnStore(Parent,Key,static_cast<Type>(Value));}
#define JD_ENUM(Name, Value, Description)
#define JD_ENUM_END(Name)
#include "Definitions/Contract/SharedTypes.inc.h"
