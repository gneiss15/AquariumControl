// MiniWlanNanny.cpp, Version: 1.02

#include <GnPlatform.h>

#if defined( GNPlatformESP32 )
#include "MiniWlanNanny.h"
#include "SpecialPrint.h"
#include "SpecialLog.h"
#include <esp_wifi.h>
#include <cstring>  // memcmp

//****************************************************************
// Utils
//****************************************************************
static inline void EspResetNow( const char* reason )
 {
  // letzte Logs sichern
  LSPrintf( reason );
  SpecialLog.FlushNow();
  delay( 50 );            // kurz Luft lassen (UART/Flash)
  esp_restart();          // oder: ESP.restart();
 }

static inline void ResetNow( const char * reason )
 {
  LSPrintf( "[NANNY] %s", reason ? reason : "reset" );
  SpecialLog.FlushNow();
  delay( 50 );
  esp_restart();
 }

//****************************************************************
// TMiniWlanNanny
//****************************************************************

inline bool TMiniWlanNanny::MatchStaMac( const uint8_t * addr ) const
 {
  return addr && (std::memcmp( addr, FStaMac, 6 ) == 0);
 }

// --- promiscuous callback -------------------------------------------------
static TMiniWlanNanny * SMiniNannySelf = nullptr;
static void IRAM_ATTR MiniPromiscCb( void * buf, wifi_promiscuous_pkt_type_t type )
 {
  if( !SMiniNannySelf ) return;
  if( type != WIFI_PKT_DATA ) return; // nur DATA-Frames
  const wifi_promiscuous_pkt_t * p = static_cast<const wifi_promiscuous_pkt_t*>( buf );
  const uint8_t * h = p->payload;     // 802.11 MAC header
  // FrameCtrl(2) Dur(2) Addr1(6)=DA Addr2(6)=SA Addr3(6) ...
  const uint8_t * addr1 = h + 4;      // DA
  // nur Frames an UNS (addr1 == unsere STA-MAC) zählen:
  if( SMiniNannySelf->MatchStaMac( addr1 ) )
   {
    // leicht bleiben: nur Timestamp setzen
    SMiniNannySelf->TouchTraffic();
   }
 }

void TMiniWlanNanny::GotMac( const char * msg )
 {
  // nur DATA-Frames zulassen
  wifi_promiscuous_filter_t f{};
  f.filter_mask = WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter( &f );
  // Callback setzen und einschalten
  SMiniNannySelf = this;
  esp_wifi_set_promiscuous_rx_cb( &MiniPromiscCb );
  esp_wifi_set_promiscuous( true );
  FSnifferOn = true;
  FSnifferPending = false;
  TouchTraffic();
  LSPrintf( "[NANNY] sniffer ON (%s)\n", msg );
 }

void TMiniWlanNanny::EnableSniffer( bool on )
 {
  if( on == FSnifferOn ) return;
  if( on )
   {
    // Versuch, MAC zu holen; wenn noch nicht bereit → pendend markieren
    if( esp_wifi_get_mac( WIFI_IF_STA, FStaMac ) == ESP_OK )
      GotMac( "direct" );
     else
     {
      FSnifferPending = true;
      LSPrintf( "[NANNY] sniffer pending (wifi not init)\n" );
     }
   }
   else
   {
    esp_wifi_set_promiscuous( false );
    esp_wifi_set_promiscuous_rx_cb( nullptr );
    SMiniNannySelf = nullptr;
    FSnifferOn = false;
    FSnifferPending = false;
    LSPrintf(" [NANNY] sniffer OFF\n" );
   }
 }

void TMiniWlanNanny::CheckTimerCb( TGnCoopTimerBase *, uint16_t )
 {
  if( !FEnabled )
    return;

  const uint32_t now = millis();

  // Falls Sniffer noch aussteht: jetzt (erneut) versuchen zu armen
  if( FSnifferPending && !FSnifferOn && esp_wifi_get_mac( WIFI_IF_STA, FStaMac) == ESP_OK )
    GotMac( "late" );

  if( !FSnifferOn )
    return;

  const uint32_t last = FLastTrafficMs;  // volatile read once
  if( last == 0 ) return;                // not armed yet

  const uint32_t idle = now - last;
  if( idle > FTimeoutMs )                // fail-safe: restart the whole ESP
    ResetNow( "[NANNY] no traffic - esp_restart" );
 }

#endif // ESP32
