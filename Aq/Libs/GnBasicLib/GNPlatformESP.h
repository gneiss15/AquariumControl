// GNPlatformESP.h, Version: 1.00
#pragma once

#include <GnBasics.h>

#if defined( GNPlatformESP )

//****************************************************************
// ESP platform helpers
//****************************************************************

extern void         WdtDelay( uint32_t Ms );
extern char const * WifiErrReasonName( uint8_t Reason );

#endif // GNPlatformESP
