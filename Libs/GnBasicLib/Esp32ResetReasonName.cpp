// Esp32ResetReasonName.cpp, Version: 1.01

#include <GnPlatform.h>

#if defined( GNPlatformESP32 )
#include <Esp32ResetReasonName.h>

//****************************************************************
// Esp32ResetReasonName
//****************************************************************

const char * Esp32ResetReasonName( esp_reset_reason_t reason )
 {
  switch( reason )
   {
    case ESP_RST_UNKNOWN:    return "UNKNOWN";
    case ESP_RST_POWERON:    return "POWERON";
    case ESP_RST_EXT:        return "EXT";
    case ESP_RST_SW:         return "SW";
    case ESP_RST_PANIC:      return "PANIC";
    case ESP_RST_INT_WDT:    return "INT_WDT";
    case ESP_RST_TASK_WDT:   return "TASK_WDT";
    case ESP_RST_WDT:        return "WDT";
    case ESP_RST_DEEPSLEEP:  return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:   return "BROWNOUT";
    case ESP_RST_SDIO:       return "SDIO";
    case ESP_RST_USB:        return "USB";
    case ESP_RST_JTAG:       return "JTAG";
    case ESP_RST_EFUSE:      return "EFUSE";
    case ESP_RST_PWR_GLITCH: return "PWR_GLITCH";
    case ESP_RST_CPU_LOCKUP: return "CPU_LOCKUP";
    #ifdef ESP_RST_CORE
      case ESP_RST_CORE:     return "CORE";
    #endif
    #ifdef ESP_RST_CPU
      case ESP_RST_CPU:      return "CPU";
    #endif
    #ifdef ESP_RST_SYS
      case ESP_RST_SYS:      return "SYS";
    #endif
    #ifdef ESP_RST_RTC_WDT
      case ESP_RST_RTC_WDT:  return "RTC_WDT";
    #endif
    default: return "???";
   }
 }

#endif // ESP32
