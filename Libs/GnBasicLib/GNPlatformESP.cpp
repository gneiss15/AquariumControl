// GNPlatformESP.cpp, Version: 1.01

#include <GnBasics.h>

#if defined( GNPlatformESP )

#include <GNPlatformESP.h>

#if defined( GNPlatformESP32 )
  #include <esp_task_wdt.h>
  #include <esp_wifi_types.h>
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"

  #if __has_include( "esp_idf_version.h" )
    #include "esp_idf_version.h"
  #endif
#elif defined( GNPlatformESP8266 )
  #include <ESP8266WiFi.h>
#endif

//****************************************************************
// Compile-time guards
//****************************************************************

#if defined( GNPlatformESP32 )

static_assert( WIFI_REASON_UNSPECIFIED == 1, "WIFI_REASON_UNSPECIFIED must be 1" );
static_assert( WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD == 212,
               "Maximum known ESP32 reason must be 212" );

#if defined( ESP_IDF_VERSION_MAJOR ) && defined( ESP_IDF_VERSION_MINOR )
  #if ( ESP_IDF_VERSION_MAJOR > 5 ) || \
      ( ( ESP_IDF_VERSION_MAJOR == 5 ) && ( ESP_IDF_VERSION_MINOR > 5 ) )
    #warning "ESP-IDF version is newer than 5.5; review WifiErrReasonName table"
  #endif
#endif

#elif defined( GNPlatformESP8266 )

static_assert( WIFI_DISCONNECT_REASON_UNSPECIFIED == 1,
               "WIFI_DISCONNECT_REASON_UNSPECIFIED must be 1" );
static_assert( WIFI_DISCONNECT_REASON_HANDSHAKE_TIMEOUT == 204,
               "Maximum known ESP8266 reason must be 204" );

#endif

//****************************************************************
// WdtDelay
//****************************************************************

#if defined( GNPlatformESP32 )

void WdtDelay( uint32_t Ms )
 {
  while ( Ms > 0 )
   {
    uint32_t const DelayMs = Ms > 10 ? 10 : Ms;
    TickType_t     Ticks = pdMS_TO_TICKS( DelayMs );
    if ( Ticks == 0 )
     {
      Ticks = 1;
     }

    esp_task_wdt_reset();
    vTaskDelay( Ticks );
    Ms -= DelayMs;
   }

  esp_task_wdt_reset();
 }

#elif defined( GNPlatformESP8266 )

void WdtDelay( uint32_t Ms )
 {
  while ( Ms > 0 )
   {
    uint32_t const DelayMs = Ms > 10 ? 10 : Ms;

    ESP.wdtFeed();
    delay( DelayMs );
    Ms -= DelayMs;
   }

  ESP.wdtFeed();
 }

#endif

//****************************************************************
// WifiErrReasonName
//****************************************************************

struct TWifiReasonMap
 {
  uint8_t      Code;
  char const * Text;
 };

static TWifiReasonMap const SWifiReasonMap[] =
 {
  {   1, "Unspecified reason" },
  {   2, "Authentication expired" },
  {   3, "Deauthentication due to leaving" },
  {   4, "Disassociated due to inactivity" },
  {   5, "Too many associated stations" },
  {   6, "Class 2 frame received from nonauthenticated STA" },
  {   7, "Class 3 frame received from nonassociated STA" },
  {   8, "Deassociated due to leaving" },
  {   9, "Association but not authenticated" },
  {  10, "Disassociated due to poor power capability" },
  {  11, "Disassociated due to unsupported channel" },
  {  12, "Disassociated due to BSS transition" },
  {  13, "Invalid Information Element (IE)" },
  {  14, "MIC failure" },
  {  15, "4-way handshake timeout" },
  {  16, "Group key update timeout" },
  {  17, "IE differs in 4-way handshake" },
  {  18, "Invalid group cipher" },
  {  19, "Invalid pairwise cipher" },
  {  20, "Invalid AKMP" },
  {  21, "Unsupported RSN IE version" },
  {  22, "Invalid RSN IE capabilities" },
  {  23, "802.1X authentication failed" },
  {  24, "Cipher suite rejected" },
  {  25, "TDLS peer unreachable" },
  {  26, "TDLS unspecified" },
  {  27, "SSP requested disassociation" },
  {  28, "No SSP roaming agreement" },
  {  29, "Bad cipher or AKM" },
  {  30, "Not authorized in this location" },
  {  31, "Service change precludes TS" },
  {  32, "Unspecified QoS reason" },
  {  33, "Not enough bandwidth" },
  {  34, "Missing ACKs" },
  {  35, "Exceeded TXOP" },
  {  36, "Station leaving" },
  {  37, "End of Block Ack (BA)" },
  {  38, "Unknown Block Ack (BA)" },
  {  39, "Timeout" },
  {  46, "Peer initiated disassociation" },
  {  47, "AP initiated disassociation" },
  {  48, "Invalid FT action frame count" },
  {  49, "Invalid PMKID" },
  {  50, "Invalid MDE" },
  {  51, "Invalid FTE" },
  {  67, "Transmission link establishment failed" },
  {  68, "Alternative channel occupied" },
  { 200, "Beacon timeout" },
  { 201, "No AP found" },
  { 202, "Authentication failed" },
  { 203, "Association failed" },
  { 204, "Handshake timeout" },
  { 205, "Connection failed" },
  { 206, "AP TSF reset" },
  { 207, "Roaming" },
  { 208, "Association comeback time too long" },
  { 209, "SA query timeout" },
  { 210, "No AP found with compatible security" },
  { 211, "No AP found in auth mode threshold" },
  { 212, "No AP found in RSSI threshold" }
 };

char const * WifiErrReasonName( uint8_t Reason )
 {
  size_t Low  = 0;
  size_t High = TableSize( SWifiReasonMap );

  while ( Low < High )
   {
    size_t const Mid  = Low + ( High - Low ) / 2;
    uint8_t const Code = SWifiReasonMap[ Mid ].Code;

    if ( Reason == Code )
      return SWifiReasonMap[ Mid ].Text;
     else if ( Reason < Code )
      High = Mid;
     else
      Low = Mid + 1;
   }

  static char Buffer[ 48 ];
  snprintf( Buffer, sizeof( Buffer ), "Unknown reason (%u)",
            static_cast<unsigned int>( Reason ) );
  return Buffer;
 }

#endif // GNPlatformESP
