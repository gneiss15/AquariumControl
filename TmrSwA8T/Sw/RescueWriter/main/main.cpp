// main.cpp, Version: 1.01
#include "Arduino.h"
#include "DeviceType.inc.h"
#include "EspSignedArtifact.h"
#include "EspRescueProject.h"
#include "../../TmrSwA8THardware.h"

#include "esp_event.h"
#include "esp_flash.h"
#include "esp_flash_internal.h"
#include "esp_flash_encrypt.h"
#include "esp_http_server.h"
#include "esp_image_format.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_secure_boot.h"
#include "esp_wifi.h"
#include "sdkconfig.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace
 {
  constexpr uint32_t MainStart = 0x0E0000;
  constexpr uint32_t MainSize = 0x2D0000;
  constexpr uint32_t StageStart = 0x2E0000;
  constexpr uint32_t RescueStart = 0x010000;
  constexpr uint32_t RescueSize = 0x0D0000;
  constexpr uint32_t SectorSize = 4096;
  constexpr size_t FixedPrefix = 64 + 48;
  constexpr size_t MaxSignature = 512;
  constexpr size_t BufferSize = 4096;
  static_assert( CONFIG_PARTITION_TABLE_OFFSET == 0x8000 );
  static_assert( StageStart + RescueSize == MainStart + MainSize );
  static_assert( StageStart % SectorSize == 0 );

  // The IDF HTTP server handles requests serially. Only this handler writes flash.
  uint8_t Buffer[BufferSize];
  const esp_partition_t * Rescue = nullptr;
  bool ProtectionRestored = true;

  bool ImageValid( uint32_t Address, uint32_t Size )
   {
    const esp_partition_pos_t Position = { Address, Size };
    esp_image_metadata_t Image = {};
    return esp_image_verify( ESP_IMAGE_VERIFY, &Position, &Image ) == ESP_OK
        && Image.image_len <= Size;
   }

  bool GeometryValid(void)
   {
    uint32_t FlashSize = 0;
    const auto * Running = esp_ota_get_running_partition();
    Rescue = esp_partition_find_first( ESP_PARTITION_TYPE_APP,
      ESP_PARTITION_SUBTYPE_APP_FACTORY, "safeboot" );
    const auto * FileSystem = esp_partition_find_first( ESP_PARTITION_TYPE_DATA,
      ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs" );
    return ProtectionRestored && !esp_secure_boot_enabled() && !esp_flash_encryption_enabled()
        && esp_flash_get_physical_size( nullptr, &FlashSize ) == ESP_OK && FlashSize == 0x400000
        && Running && Running->type == ESP_PARTITION_TYPE_APP
        && Running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 && !Running->readonly
        && Running->address == MainStart && Running->size == MainSize
        && FileSystem && FileSystem->address == StageStart + RescueSize && FileSystem->size == 0x50000
        && Rescue && !Rescue->readonly && Rescue->address == RescueStart && Rescue->size == RescueSize
        && ImageValid( MainStart, StageStart - MainStart );
   }

  esp_err_t StageFlash( uint32_t Offset, const void * Data, size_t Size )
   {
    if( !ProtectionRestored || !Size || Offset > RescueSize || Size > RescueSize - Offset
        || (!Data && (Offset % SectorSize || Size % SectorSize)) )
      return ESP_ERR_INVALID_ARG;
    auto Error = esp_flash_set_dangerous_write_protection( esp_flash_default_chip, false );
    if( Error == ESP_OK )
      Error = Data ? esp_flash_write( nullptr, Data, StageStart + Offset, Size )
                   : esp_flash_erase_region( nullptr, StageStart + Offset, Size );
    ProtectionRestored = esp_flash_set_dangerous_write_protection( esp_flash_default_chip, true ) == ESP_OK;
    return ProtectionRestored ? Error : ESP_FAIL;
   }

  bool Receive( httpd_req_t * Request, void * Data, size_t Size )
   {
    auto * Bytes = static_cast<char *>( Data );
    while( Size )
     {
      const int Read = httpd_req_recv( Request, Bytes, Size );
      if( Read <= 0 )
        return false;
      Bytes += Read;
      Size -= Read;
     }
    return true;
   }

  bool VerifyFlashHash( uint32_t Address, const TEspSignedArtifactInfo & Info )
   {
    TEspSignedArtifactHash Hash;
    Hash.Begin();
    for( uint32_t Offset = 0; Offset < Info.PayloadSize; )
     {
      const size_t Count = std::min<size_t>( sizeof( Buffer ), Info.PayloadSize - Offset );
      if( esp_flash_read( nullptr, Buffer, Address + Offset, Count ) != ESP_OK )
        return false;
      Hash.Add( Buffer, Count );
      Offset += Count;
     }
    return Hash.EndAndCompare( Info.PayloadHash );
   }

  esp_err_t Upload( httpd_req_t * Request )
   {
    if( !GeometryValid() )
      return httpd_resp_send_err( Request, HTTPD_500_INTERNAL_SERVER_ERROR, "Unexpected flash layout or running image; write blocked." );
    uint8_t Prefix[FixedPrefix + MaxSignature];
    uint16_t SignatureSize = 0;
    String Error;
    if( Request->content_len <= FixedPrefix || Request->content_len > RescueSize + sizeof( Prefix )
        || !Receive( Request, Prefix, FixedPrefix )
        || !EspSignedArtifactReadSignatureSize( Prefix, FixedPrefix, SignatureSize, Error )
        || SignatureSize > MaxSignature || Request->content_len <= FixedPrefix + SignatureSize
        || !Receive( Request, Prefix + FixedPrefix, SignatureSize ) )
      return httpd_resp_send_err( Request, HTTPD_400_BAD_REQUEST, "Incomplete or invalid signed Rescue header." );
    TEspSignedArtifactInfo Info = {};
    if( !EspSignedArtifactVerifyPrefix( Prefix, FixedPrefix + SignatureSize,
          TEspSignedArtifactType::Rescue, ESP_UPDATE_DEVICE_TYPE, Request->content_len,
          RescueSize, Info, Error ) )
      return httpd_resp_send_err( Request, HTTPD_400_BAD_REQUEST, Error.c_str() );

    const uint32_t EraseSize = (Info.PayloadSize + SectorSize - 1) & ~(SectorSize - 1);
    if( StageFlash( 0, nullptr, EraseSize ) != ESP_OK )
      return httpd_resp_send_err( Request, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not erase staging area; Rescue untouched." );
    for( uint32_t Offset = 0; Offset < Info.PayloadSize; )
     {
      const size_t Count = std::min<size_t>( sizeof( Buffer ), Info.PayloadSize - Offset );
      if( !Receive( Request, Buffer, Count ) )
        return httpd_resp_send_err( Request, HTTPD_400_BAD_REQUEST, "Upload incomplete; Rescue untouched." );
      if( StageFlash( Offset, Buffer, Count ) != ESP_OK )
        return httpd_resp_send_err( Request, HTTPD_500_INTERNAL_SERVER_ERROR, "Staging write failed; Rescue untouched." );
      Offset += Count;
     }

    if( !VerifyFlashHash( StageStart, Info ) || !ImageValid( StageStart, Info.PayloadSize ) )
      return httpd_resp_send_err( Request, HTTPD_400_BAD_REQUEST, "Staged Rescue hash or ESP image invalid; Rescue untouched." );

    // Stay in this writer until the user explicitly power-cycles the device.
    if( esp_partition_erase_range( Rescue, 0, EraseSize ) != ESP_OK )
      return httpd_resp_send_err( Request, HTTPD_500_INTERNAL_SERVER_ERROR, "Rescue erase failed. Stay in this writer and retry." );
    for( uint32_t Offset = 0; Offset < Info.PayloadSize; )
     {
      const size_t Count = std::min<size_t>( sizeof( Buffer ), Info.PayloadSize - Offset );
      if( esp_flash_read( nullptr, Buffer, StageStart + Offset, Count ) != ESP_OK
          || esp_partition_write( Rescue, Offset, Buffer, Count ) != ESP_OK )
        return httpd_resp_send_err( Request, HTTPD_500_INTERNAL_SERVER_ERROR, "Rescue copy failed. Stay in this writer and retry." );
      Offset += Count;
     }
    if( !VerifyFlashHash( RescueStart, Info ) || !ImageValid( RescueStart, Info.PayloadSize ) )
      return httpd_resp_send_err( Request, HTTPD_500_INTERNAL_SERVER_ERROR, "Rescue readback failed. Stay in this writer and retry." );
    return httpd_resp_sendstr( Request, "Rescue written and verified. Use the hardware button at power-on to start Rescue." );
   }

  esp_err_t Page( httpd_req_t * Request )
   {
    httpd_resp_set_type( Request, "text/html; charset=utf-8" );
    return httpd_resp_sendstr( Request,
      "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'>"
      "<title>Nous Rescue Writer</title><h1>Nous Rescue Writer</h1>"
      "<p>Upload a signed TmrSwA8T Rescue image. Keep the device powered.</p>"
      "<input type=file id=f accept=.rescue_signed><button id=b>Write Rescue</button><pre id=s></pre>"
      "<script>const f=document.querySelector('#f'),b=document.querySelector('#b'),s=document.querySelector('#s');"
      "b.onclick=()=>{if(!f.files.length)return;b.disabled=true;let x=new XMLHttpRequest();"
      "x.open('POST','/Rescue');x.upload.onprogress=e=>s.textContent=e.loaded+' / '+f.files[0].size+' bytes sent';"
      "x.upload.onload=()=>s.textContent='Verifying staged image and writing Rescue...';"
      "x.onload=()=>{s.textContent=x.responseText;b.disabled=false};"
      "x.onerror=()=>{s.textContent='Connection lost. Keep the device powered.';b.disabled=false};x.send(f.files[0]);};</script>" );
   }
 }

extern "C" void app_main(void)
 {
  using namespace TmrSwA8THardware;
  gpio_set_level( static_cast<gpio_num_t>( RelayPin ), !RelayActiveLevel );
  gpio_set_direction( static_cast<gpio_num_t>( RelayPin ), GPIO_MODE_OUTPUT );
  ESP_ERROR_CHECK( esp_netif_init() );
  ESP_ERROR_CHECK( esp_event_loop_create_default() );
  esp_netif_t * Station = esp_netif_create_default_wifi_sta();
  ESP_ERROR_CHECK( Station ? ESP_OK : ESP_ERR_NO_MEM );
  ESP_ERROR_CHECK( esp_netif_set_hostname( Station, "Nous-RescueWriter" ) );
  wifi_init_config_t Init = WIFI_INIT_CONFIG_DEFAULT();
  Init.nvs_enable = 0;
  ESP_ERROR_CHECK( esp_wifi_init( &Init ) );
  ESP_ERROR_CHECK( esp_wifi_set_storage( WIFI_STORAGE_RAM ) );
  ESP_ERROR_CHECK( esp_wifi_set_mode( WIFI_MODE_STA ) );
  ESP_ERROR_CHECK( esp_wifi_start() );

  httpd_config_t Config = HTTPD_DEFAULT_CONFIG();
  Config.stack_size = 8192;
  Config.recv_wait_timeout = 15;
  httpd_handle_t Server = nullptr;
  ESP_ERROR_CHECK( httpd_start( &Server, &Config ) );
  httpd_uri_t Home = { .uri = "/", .method = HTTP_GET, .handler = Page, .user_ctx = nullptr };
  httpd_uri_t Update = { .uri = "/Rescue", .method = HTTP_POST, .handler = Upload, .user_ctx = nullptr };
  ESP_ERROR_CHECK( httpd_register_uri_handler( Server, &Home ) );
  ESP_ERROR_CHECK( httpd_register_uri_handler( Server, &Update ) );

  const char * const Ssids[] = { ESP_RESCUE_WLAN_SSID, ESP_RESCUE_WLAN_SSID2 };
  const char * const Passwords[] = { ESP_RESCUE_WLAN_PW, ESP_RESCUE_WLAN_PW2 };
  unsigned Next = 0;
  for(;;)
   {
    esp_netif_ip_info_t Ip = {};
    wifi_ap_record_t AccessPoint = {};
    if( esp_wifi_sta_get_ap_info( &AccessPoint ) != ESP_OK ||
        esp_netif_get_ip_info( Station, &Ip ) != ESP_OK || !Ip.ip.addr )
     {
      const unsigned Profile = Ssids[Next][0] ? Next : 1 - Next;
      wifi_config_t WiFi = {};
      const size_t SsidSize = strlen( Ssids[Profile] );
      const size_t PasswordSize = strlen( Passwords[Profile] );
      if( SsidSize && SsidSize <= sizeof( WiFi.sta.ssid ) && PasswordSize < sizeof( WiFi.sta.password ) )
       {
        memcpy( WiFi.sta.ssid, Ssids[Profile], SsidSize );
        memcpy( WiFi.sta.password, Passwords[Profile], PasswordSize );
        esp_wifi_disconnect();
        if( esp_wifi_set_config( WIFI_IF_STA, &WiFi ) == ESP_OK )
          esp_wifi_connect();
       }
      Next = 1 - Profile;
     }
    vTaskDelay( pdMS_TO_TICKS( 10000 ) );
   }
 }
