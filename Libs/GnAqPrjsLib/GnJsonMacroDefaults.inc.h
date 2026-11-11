// GnJsonMacroDefaults.inc.h, Version: 1.04
// Defaults for one repeated definition expansion. JD is the required field hook.
// The definition entry point removes the hooks after its expansion.

#ifndef JD
  #error Define JD before including a JSON definition
#endif

#ifndef JD_BEGIN
  #define JD_BEGIN(Name, Description)
#endif

#ifndef JD_END
  #define JD_END(Name)
#endif

#ifndef JD_CONST
  #define JD_CONST(Type, Name, Value)
#endif

#ifndef JD_BIT
#define JD_BIT(Name, Description)
#endif

#ifndef JD_BASE
  #define JD_BASE(Name, Presence)
#endif

#ifndef JD_VALUE
  #define JD_VALUE(Name, Type, Rule, Description)
#endif

#ifndef JD_TUPLE_BEGIN
  #define JD_TUPLE_BEGIN(Name, Description)
#endif


// A message hook supplies both its C++ type and dispatch identifier.
#ifndef JD_MESSAGE
  #define JD_MESSAGE(Type, Tag, Value)
#endif

