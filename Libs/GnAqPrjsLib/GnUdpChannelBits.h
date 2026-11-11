// GnUdpChannelBits.h, Version: 1.01
#pragma once
#include <cstdint>

// The field list and its order come from SharedTypes.inc.h. Each value bit is
// immediately followed by its validity-mask bit.
union TGnUdpChannelBits
 {
  struct
   {
#define JD_ALIAS(Name, Type, Rule, Description)
#define JD_ENUM_BEGIN(Name, Type, Description)
#define JD_ENUM(Name, Value, Description)
#define JD_ENUM_END(Name)
#define JD_BIT(Name, Description) uint16_t Name : 1; uint16_t Name ## Msk : 1;
#include "Definitions/Contract/SharedTypes.inc.h"
#undef JD_ALIAS
#undef JD_ENUM_BEGIN
#undef JD_ENUM
#undef JD_ENUM_END
#undef JD_BIT
   } Bit;
  uint16_t All;
 };

static_assert(sizeof(TGnUdpChannelBits)==sizeof(uint16_t));
