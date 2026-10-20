// OtaHandler.cpp, Version: 1.00
#include "GnOtaHandler.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>

#if 0
      esp_task_wdt_config_t cfg =
       {
        .timeout_ms = 30000,          // Timeout in Millisekunden, 30 Sekunden
        .idle_core_mask = 0x3,        // Watchdog auf beiden CPU-Kernen aktivieren (Core 0 und 1)
        .trigger_panic = true         // Bei Timeout Panic auslösen (Reset)
       };
      esp_task_wdt_reconfigure( &cfg );
#endif
#if 0
      esp_task_wdt_reset();
#endif

TGnOtaHandler::TGnOtaHandler( uint32_t normalWdtMs, uint32_t otaWdtMs )
: FOtaActive( false )
, FNormalWdtMs( normalWdtMs )
, FOtaWdtMs( otaWdtMs )
 {
 }
void TGnOtaHandler::Begin( const char * hostName, const char * password )
 {
  // Set Hostname
  #if defined( OTA_HOSTNAME ) || defined( HOSTNAME )
    if( !( hostName && *hostName ) )
      #if defined( OTA_HOSTNAME )
        hostName = OTA_HOSTNAME;
       #else
        hostName = HOSTNAME "_OTA";
      #endif
  #endif
  if( hostName && *hostName )
    ArduinoOTA.setHostname( hostName );

  // Set Password
  #if defined( OTA_PW )
   if( !( password && *password ) )
     password = OTA_PW;
  #endif
  if( password && *password )
    ArduinoOTA.setPassword( password );

  // OTA Callbacks
  ArduinoOTA.onStart
   (
    []()
     {
      //Serial.println( "[OTA] Start — WDT set to 10s" );
      if( ArduinoOTA.getCommand() != U_FLASH )
        Serial.println( "[OTA] Start — FS\n" );
      ArduinoOTA.setTimeout( 30000 );  // Timeout auf 30 Sekunden erhöhen
      
      // Remove all Tasks from Task-Watchdog
      esp_task_wdt_delete( NULL );                         // Aktual Task (loopTask ?)
      esp_task_wdt_delete( xTaskGetCurrentTaskHandle() );  // Aktual Task (loopTask ?)
      esp_task_wdt_delete( xTaskGetHandle( "loopTask" ) ); // loopTask explizit
      // swich of Watchdog kompletly
      esp_task_wdt_deinit();
     }
   );
  ArduinoOTA.onEnd
   (
    []()
     {
      //Serial.println( "\n[OTA] End — WDT back to 3s" );
      ArduinoOTA.end();
      LittleFS.end();
      esp_restart(); 
     }
   );
  ArduinoOTA.onProgress
   (
    []( unsigned int /*progress*/, unsigned int /*total*/ )
     {
      //Serial.printf( "[OTA] %u%%\r", (progress * 100) / total ); 
     }
   );
  ArduinoOTA.onError
   (
    []( ota_error_t error )
     {
      const char * txt;
      char buffer[ 20 ];
      switch( error )
       {
        case OTA_AUTH_ERROR:
          txt = "Auth"; break;
        case OTA_BEGIN_ERROR:
          txt = "Begin"; break;
        case OTA_CONNECT_ERROR:
          txt = "Connect"; break;
        case OTA_RECEIVE_ERROR:
          txt = "Receive"; break;
        case OTA_END_ERROR:
          txt = "End"; break;
        default:
          snprintf( buffer, sizeof( buffer ), "%u", error );
          txt = buffer; break;
       }
      Serial.printf( "\n[OTA] Error [%s]\n", txt );
     }
   );

  ArduinoOTA.begin();
  //Serial.printf( "[OTA] Ready — Hostname: %s, Port: 3232\n", hostName );
 }
void TGnOtaHandler::Loop(void)
 {
  if( WiFi.status() == WL_CONNECTED )
    ArduinoOTA.handle();
 }
