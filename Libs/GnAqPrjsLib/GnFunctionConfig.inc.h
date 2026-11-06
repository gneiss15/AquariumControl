// GnFunctionConfig.inc.h, Version: 1.02
// Expand function-bound member names directly from their shared definition.
#ifndef GN_FUNCTION_CONFIG_FIELD
  #error Define GN_FUNCTION_CONFIG_FIELD before including GnFunctionConfig.inc.h
#endif

#define JD(Type, Member, Presence, Rule, Description, Part) GN_FUNCTION_CONFIG_FIELD(Member)
#pragma push_macro("Do_TJsonFunctionConfig")
#ifndef Do_TJsonFunctionConfig
  #define Do_TJsonFunctionConfig
#endif
#include "Definitions/Contract/TJsonFunctionConfig.inc.h"
#pragma pop_macro("Do_TJsonFunctionConfig")
#include "GnJsonMacroCleanup.inc.h"
#undef GN_FUNCTION_CONFIG_FIELD
