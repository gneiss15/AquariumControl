// CollectMacros.inc.h, Version: 1.03
#define JD_BASE(Name,Presence) {"Kind":"Base","Name":#Name,"Presence":#Presence}
#define JD_VALUE(Name,Type,Rule,Description) {"Kind":"Value","Name":#Name,"Type":#Type,"Rule":#Rule,"Description":Description}
#define JD_TUPLE_BEGIN(Name,Description) {"Kind":"TupleBegin","Name":#Name,"Description":Description}
#define JD_BIT(Name,Description) {"Kind":"Bit","Name":#Name,"Description":Description}
#define JD_BEGIN(Name,Description) {"Kind":"Begin","Name":#Name,"Description":Description}
#define JD_CONST(Type,Name,Value) {"Kind":"Const","Type":#Type,"Name":#Name,"ValueText":#Value}
#define JD(Type,Name,Presence,Rule,Description,Part) {"Kind":"Field","Type":#Type,"Name":#Name,"Presence":#Presence,"Rule":#Rule,"Description":Description,"Part":#Part}
#define JD_END(Name) {"Kind":"End","Name":#Name}
#define JD_MESSAGE(Type,Tag,Value) {"Kind":"Message","Name":#Type,"Tag":#Tag,"ValueText":#Value}

#define JD_ALIAS(Name,Type,Rule,Description) {"Kind":"Alias","Name":#Name,"Type":#Type,"Rule":#Rule,"Description":Description}
#define JD_ENUM_BEGIN(Name,Type,Description) {"Kind":"EnumBegin","Name":#Name,"Type":#Type,"Description":Description}
#define JD_ENUM(Name,Value,Description) {"Kind":"Enum","Name":#Name,"Value":Value,"Description":Description}
#define JD_ENUM_END(Name) {"Kind":"EnumEnd","Name":#Name}
