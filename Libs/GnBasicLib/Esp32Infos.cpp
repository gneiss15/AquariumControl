// Esp32Infos.cpp, Version: 1.03

#include <GnPlatform.h>

#if defined( GNPlatformESP32 )
#include "Esp32Infos.h"
#include "esp_partition.h"
#include <LittleFS.h>

// Collect partitions, sort by address and deduplicate by address,
// then format into a human-readable string returned as Arduino String.
String GetPartitionInfos(void)
 {
  std::vector<esp_partition_t> parts;

  // put all found partition data into vector (partition data ptr is owned by iterator, so copy by value)
  esp_partition_iterator_t it = esp_partition_find
   (
    ESP_PARTITION_TYPE_ANY,
    ESP_PARTITION_SUBTYPE_ANY,
    NULL
   );
  while( it )
   {
    const esp_partition_t * p = esp_partition_get( it );
    parts.push_back( *p ); // copy iterator-owned struct
    it = esp_partition_next( it );
   }
  esp_partition_iterator_release( it );

  // Sort ascending by address
  std::sort
   (
    parts.begin(),
    parts.end(),
    []( const esp_partition_t & a, const esp_partition_t & b)
     {
      return a.address < b.address;
     }
   );

  // Build output string
  String out;
  out += "############ Partitions ############\n";
  out += "Label     type sub  addr       size\n";

  uint32_t lastAddr = UINT32_MAX;
  for( const auto & p : parts )
   {
    if( p.address == lastAddr ) // dedupe by address
      continue;
    out += StringFormat( "%-9s 0x%02x 0x%02x 0x%06x %4u kB\n",
                         p.label, p.type, p.subtype, p.address, (unsigned)(p.size / 1024) );
    lastAddr = p.address;
   }

  out += "####################################\n";
  return out;
 }

String GetFlashInfos(void)
 {
  uint32_t flash   = ESP.getFlashChipSize();     // in Bytes
  uint32_t sketch  = ESP.getSketchSize();
  uint32_t freeSk  = ESP.getFreeSketchSpace();
  size_t psramSize = ESP.getPsramSize();

  String out;
  out += "########### Flash-Infos ############\n";

  out += StringFormat( "Flash  size: %4u MB\n", flash / ( 1024 * 1024 ) );
  out += StringFormat( "Sketch size: %4u KB\n", sketch / 1024 );
  out += StringFormat( "       free: %4u KB\n", freeSk / 1024 );
  if( psramSize )
    out += StringFormat( "PSRAM  size: %4u bytes\n", psramSize );
   else
    out += StringFormat( "PSRAM      : not found\n" );

  out += "####################################\n";
  return out;
 }

String GetAllLittleFSFiles(void)
 {
  LittleFS.begin();

  String out;
  out += "########## LittleFS. Files #########\n";

  File root = LittleFS.open( "/" );
  if( !root )
    out += "Failed to open directory\n";
   else
   {
    out += StringFormat( "%-20s   %s\n", "Name", "Size (bytes)" );
    for( File file = root.openNextFile();  file;  file = root.openNextFile() )
      out += StringFormat( "%-20s %6u\n", file.name(), file.size() );

    size_t total = LittleFS.totalBytes();
    size_t used  = LittleFS.usedBytes();
    out += StringFormat( "Sizes:\n  Total: %8u bytes\n  Used:  %8u bytes\n  Free:  %8u bytes\n",
                         total, used, total - used );
   }

  out += "####################################\n";
  return out;
 }

#endif // ESP32
