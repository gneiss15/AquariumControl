// main.cpp, Version: 1.03
#include "Arduino.h"
#include "RollbackService.h"
#include "Geometry.h"
#include "esp_ota_ops.h"
#include "esp_flash.h"
#include "esp_chip_info.h"
#include "esp_secure_boot.h"
#include "esp_flash_encrypt.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include <cstdio>
#include <initializer_list>
static_assert(CONFIG_PARTITION_TABLE_OFFSET == 0x10000, "Rescue must use the actual node/Shelly table offset");
void setup(void);
void loop(void);
extern "C" void app_main(void)
 {
  const esp_partition_t * Running = esp_ota_get_running_partition();
  const esp_partition_t * Main = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  const esp_partition_t * Fs = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
  const esp_partition_t * Nvs = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "nvs");
  const esp_partition_t * Ota = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
  esp_chip_info_t Chip;
  esp_chip_info(&Chip);
  uint32_t FlashSize = 0;
  const bool FlashKnown = esp_flash_get_size(nullptr, &FlashSize) == ESP_OK;
  bool GeometryValid = !( !FlashKnown || Chip.model != CHIP_ESP32C3 || Chip.revision != 4 || FlashSize != MIG_FLASH_SIZE || esp_secure_boot_enabled() || esp_flash_encryption_enabled() ||
      !Running || Running->type != ESP_PARTITION_TYPE_APP || Running->subtype != ESP_PARTITION_SUBTYPE_APP_FACTORY || Running->readonly || Running->address != MIG_RESCUE_OFFSET || Running->size != MIG_RESCUE_SIZE ||
      !Main || Main->readonly || Main->address != MIG_MAIN_OFFSET || Main->size != MIG_MAIN_SIZE ||
      !Fs || Fs->readonly || Fs->address != MIG_FS_OFFSET || Fs->size != MIG_FS_SIZE ||
      !Nvs || Nvs->readonly || Nvs->address != MIG_NVS_OFFSET || Nvs->size != 0x20000 ||
      !Ota || Ota->readonly || Ota->address != MIG_OTA_OFFSET || Ota->size != 0x2000 );
  if( !GeometryValid ) printf("RESCUE_GEOMETRY_REJECTED; diagnostics only\n");
  if( GeometryValid ) printf("RESCUE_GEOMETRY_OK table=0x10000 main=0x%X fs=0x%X factory=0x%X\n", MIG_MAIN_OFFSET, MIG_FS_OFFSET, MIG_RESCUE_OFFSET);
  if( GeometryValid && nvs_flash_init() != ESP_OK )
   {
    GeometryValid = false; printf("RESCUE_NVS_REJECTED; diagnostics only\n");
   }
  ShellyRollbackConfigure(GeometryValid);
  if( GeometryValid )
   {
    for( int Pin : {10, 19} )
     {
      digitalWrite(Pin, HIGH);
      pinMode(Pin, OUTPUT);
     }
   }
  setup();
  for(;;) { loop(); delay(1); }
 }
