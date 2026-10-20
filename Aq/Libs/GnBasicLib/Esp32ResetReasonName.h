// Esp32ResetReasonName.h, Version: 1.01
#pragma once

#include <GnPlatform.h>

#if defined( GNPlatformESP32 )

//****************************************************************
// Includes
//****************************************************************

#include <GnBasics.h>
#include <esp_system.h>

//****************************************************************
// Esp32ResetReasonName
//****************************************************************

extern const char * Esp32ResetReasonName( esp_reset_reason_t reason );

#endif // ESP32
