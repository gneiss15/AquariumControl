// ActivationGate.cpp, Version: 1.02
#include "ActivationGate.h"
#include "Geometry.h"
#include <cstring>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_image_format.h"

const char * ShellyMainActivationError(void)
 {
  const esp_partition_t * Main = esp_partition_find_first(
    ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  if( !Main || Main->address != MIG_MAIN_OFFSET || Main->size != MIG_MAIN_SIZE )
    return "Main partition geometry differs";
  esp_app_desc_t Description = {};
  if( esp_ota_get_partition_description(Main, &Description) != ESP_OK ||
      strncmp(Description.project_name, "TmrSwShelly", sizeof(Description.project_name)) != 0 )
    return "Main is not installed; upload TmrSwShelly firmware first";
  esp_partition_pos_t Position = { Main->address, Main->size };
  esp_image_metadata_t Image = {};
  if( esp_image_verify(ESP_IMAGE_VERIFY, &Position, &Image) != ESP_OK )
    return "Main image is invalid";
  return nullptr;
 }
