// main.cpp, Version: 1.03
#include <GnBasics.h>
#include "sdkconfig.h"
#include "ShellyLayout.h"
#include "esp_ota_ops.h"
#include "esp_flash.h"
#include "esp_chip_info.h"
#include "esp_secure_boot.h"
#include "esp_flash_encrypt.h"
#include "Libs/Common/TmrSw/TmrSwMain.inc.h"
static_assert(CONFIG_PARTITION_TABLE_OFFSET == 0x10000, "Shelly table offset must match its loader");
bool ShellyLayoutVerified(void)
 {
  const esp_partition_t * Running = esp_ota_get_running_partition();
  esp_chip_info_t Chip; esp_chip_info(&Chip);
  uint32_t Flash = 0;
  if( Chip.model != CHIP_ESP32C3 || Chip.revision != 4 || esp_flash_get_size(nullptr, &Flash) != ESP_OK ||
      Flash != 0x800000 || esp_secure_boot_enabled() || esp_flash_encryption_enabled() ||
      !Running || Running->type != ESP_PARTITION_TYPE_APP || Running->readonly || Running->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0 || Running->address != 0x3A0000 || Running->size != 0x2A0000 ) return false;
  struct TPart { const char * Label; uint32_t Offset, Size; esp_partition_type_t Type; esp_partition_subtype_t Subtype; };
  const TPart Parts[] = { { "safeboot",0x640000,0x180000,ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_FACTORY }, { "spiffs",0x2C0000,0xE0000,ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS },
    { "nvs",0x7C0000,0x20000,ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS }, { "otadata",0x7E0000,0x2000,ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_OTA } };
  for( const auto & Expected : Parts )
   {
    const esp_partition_t * Part = esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, Expected.Label);
    if( !Part || Part->readonly || Part->type != Expected.Type || Part->subtype != Expected.Subtype || Part->address != Expected.Offset || Part->size != Expected.Size ) return false;
   }
  return true;
 }
extern "C" void app_main(void)
 {
  if( !ShellyLayoutVerified() ) { printf("Shelly Main geometry rejected; no network/NVS/output initialization\n"); return; }
  initArduino(); setup();
  for(;;) { loop(); delay(1); }
 }
