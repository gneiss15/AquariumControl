// WlanNanny.cpp, Version: 1.03

#include <GnBasics.h>

#if defined( GNPlatformESP32 )
#include <WlanNanny.h>
#include <WiFi.h>
#include <GNPlatformESP.h>
#include "esp_wifi.h"
#include "AsyncWebServerEx.h"
#include "SpecialPrint.h"
#include "SpecialLog.h"

//****************************************************************
// Utils
//****************************************************************

#if 0
static void ShowWifiPower( const char * txt )
 {
  // --- Logging der aktuellen Settings ---
  // TxPower: Arduino-Enum + echte dBm aus IDF (0.25 dBm Schritte)
  wifi_power_t pEnum = WiFi.getTxPower();
  int8_t qQuarterDbm = 0;
  if( esp_wifi_get_max_tx_power( &qQuarterDbm ) == ESP_OK )
   {
    float pDbm = qQuarterDbm / 4.0f;
    LSPrintf( "[WIFI] (%s) TxPower enum=%d, actual=%.2f dBm\n", txt, (int)pEnum, pDbm );
   }
   else
    LSPrintf("[WIFI] (%s) TxPower enum=%d (get_max_tx_power failed)\n", txt, (int)pEnum );
 }
#endif

static inline const char * BwName( wifi_bandwidth_t bw )
 {
  return ( bw == WIFI_BW_HT20 ) ? "HT20" : ( bw == WIFI_BW_HT40 ) ? "HT40" : "UNK";
 }

static inline void EspResetNow( const char * reason )
 {
  // letzte Logs sichern
  LSPrintf( reason );
  SpecialLog.FlushNow();
  delay( 50 );            // kurz Luft lassen (UART/Flash)
  esp_restart();          // oder: ESP.restart();
 }

//****************************************************************
// static vars
//****************************************************************
static uint32_t       SLastStatusChk     = 0;
static uint32_t       SNextAttempt       = 0;
static uint32_t       SBackoffMs         = REJOIN_MIN_MS;
static uint32_t       SLastGotIpMs       = 0;             // Zeitpunkt des letzten GOT_IP
static uint8_t        SConsecMissingAcks = 0;             // aufeinanderfolgende Reason 34
static bool           SDid2MinReinit     = false;         // true = 2-min-Reinit für die aktuelle Outage schon erfolgt
static uint32_t       SDisConnSinceMs    = 0;             // 0 = IP vorhanden; sonst: Start der „kein IP“-Phase
static volatile bool  SDoFlushNow        = false;
static volatile bool  SDoCloseAll        = false;
static bool           SEventsHooked      = false;
static bool           SConnectRunning    = false;
static uint32_t       SCoreDeferUntilMs  = 0;             // Zeitfenster, in dem wir dem Core den Vortritt lassen
static uint32_t       SBeginAttemptMs    = 0;             // Zeitpunkt unseres letzten WiFi.begin()

// --- Grace & Burst control ---
// Zählt MISSING_ACKS (34) im Fenster und setzt eine kurze Gnadenzeit nach GOT_IP,
// damit direkt nach DHCP auftretende 34er nicht in ein Ping-Pong führen.
static uint8_t        SMaBurstCount      = 0;
static uint32_t       SMaWindowStartMs   = 0;             // Start des Burst-Fensters
static const uint32_t SMaWindowMs        = 30000;         // 30 s Fenster

static uint32_t       SGraceAfterGotIpMs = 0;             // Gnadenzeit-Deadline nach GOT_IP
static const uint32_t SGraceMs           = 3000;          // 3 s

//****************************************************************
// static funcs
//****************************************************************

static inline void ScheduleReconnect( uint32_t now )
 {
  SNextAttempt = now + SBackoffMs;
  SBackoffMs   = std::min<uint32_t>( SBackoffMs * 2, REJOIN_MAX_MS );  // Exponential backoff
 }

static void OnEvent( arduino_event_id_t ev, arduino_event_info_t info )
 {
  static uint32_t millisAtDicon = 0;
  if( ev == ARDUINO_EVENT_WIFI_STA_DISCONNECTED )
   {
    millisAtDicon = millis();
    // Dem Core kurz den Vortritt lassen (3s): verhindert Race begin() vs. connect()
    SCoreDeferUntilMs = millis() + 3000U;

    SDoFlushNow  = true;
    SDoCloseAll  = true;
    
    if( SDisConnSinceMs == 0 )
      SDisConnSinceMs = millis(); // Outage beginnt

    auto r = info.wifi_sta_disconnected.reason;
    LSPrintf( "[WIFI] DISCONNECTED reason=%d (%s)\n", (int)r, WifiErrReasonName( r ) );

    // Reason-spezifische Backoff-Strategie:
    switch( r )
     {
      case WIFI_REASON_MISSING_ACKS:
       {
        const uint32_t now = millis();
        // 1) Direkt nach GOT_IP: Core kurz den Vortritt lassen, nicht aggressiv reagieren
        if( now < SGraceAfterGotIpMs )
         {
          SCoreDeferUntilMs = now + 2000U;      // 2 s Core-Defer
          LSPrintf( "[WIFI] (34) in grace → defer core\n" );
          // Fenster zurücksetzen, um falsche Eskalation zu vermeiden
          SMaBurstCount    = 0;
          SMaWindowStartMs = 0;
          break;
         }

        // 2) Burst-Fenster pflegen
        if( SMaWindowStartMs == 0 || ( now - SMaWindowStartMs ) > SMaWindowMs )
         {
          SMaWindowStartMs = now;
          SMaBurstCount    = 0;
         }
        if( SMaBurstCount < 255 )        ++SMaBurstCount;
        if( SConsecMissingAcks < 255 )   ++SConsecMissingAcks;

        // 3) Eskalation bei Häufung
        if( SMaBurstCount >= 3 )
         {
          LSPrintf( "[WIFI] (34) burst >=3 → radio restart + defer\n" );
          WiFi.mode( WIFI_OFF );
          delay( 100 );
          WlanNanny.Init();
          SCoreDeferUntilMs = now + 5000U; // 5 s Core Ruhe
          // Fenster neu starten
          SMaWindowStartMs  = now;
          SMaBurstCount     = 0;
         }
         else
         {
          // weich reagieren: Backoff hoch, kein Sturm
          if( SBackoffMs < 12000U ) SBackoffMs = 12000U;
          if( SBackoffMs < REJOIN_MAX_MS )
            SBackoffMs = std::min<uint32_t>( SBackoffMs * 2U, REJOIN_MAX_MS );
          SNextAttempt = now + SBackoffMs;
          //LSPrintf( "[WIFI] (34) backoff → %u ms (burst=%u)\n", (unsigned)SBackoffMs, (unsigned)SMaBurstCount );
          //LSPrintf( "[WIFI] ScheduleReconnect\n" );
         }
        break;
       }

      // Funk/Infra Probleme → Backoff deutlich erhöhen
      case WIFI_REASON_BEACON_TIMEOUT:    // 200 „Funk weg“
      case WIFI_REASON_NO_AP_FOUND:       // 201 „AP nicht gefunden“
        // exponentiell bis REJOIN_MAX_MS
        if( SBackoffMs == 0 )
          SBackoffMs = REJOIN_MIN_MS * 2U;
         else if( SBackoffMs < REJOIN_MAX_MS )
          SBackoffMs = std::min<uint32_t>( SBackoffMs * 2U, REJOIN_MAX_MS );
        
        SNextAttempt = millis() + SBackoffMs;
        //LSPrintf( "[WIFI] backoff raised (reason=%u) → %u ms\n", (unsigned)r, (unsigned)SBackoffMs );
        break;
      // Auth/Key-Refresh / Client verlässt Assoc → schneller retry ok
      case WIFI_REASON_AUTH_EXPIRE:      // 2
      case WIFI_REASON_AUTH_LEAVE:       // 3
      case WIFI_REASON_ASSOC_LEAVE:      // 8
        SBackoffMs   = REJOIN_MIN_MS;
        SNextAttempt = millis() + SBackoffMs;
        break;

      default:   // normaler Backoff
        break;
     }
    //LSPrintf( "[WIFI] ScheduleReconnect\n" ); 
    ScheduleReconnect( millis() );
   }
   else if( ev == ARDUINO_EVENT_WIFI_STA_GOT_IP )
   {
    const auto ip = WiFi.localIP();
    if( millisAtDicon )
      LSPrintf( "[WIFI] GOT_IP %u.%u.%u.%u, RSSI: %d dBm, offTMillis: %u\n",
                ip[0], ip[1], ip[2], ip[3], (int)WiFi.RSSI(), millis() - millisAtDicon );
     else
      LSPrintf( "[WIFI] GOT_IP %u.%u.%u.%u, RSSI: %d dBm\n",
                ip[0], ip[1], ip[2], ip[3], (int)WiFi.RSSI() );
 
    SConsecMissingAcks = 0;
    SLastGotIpMs       = millis();
    SDisConnSinceMs    = 0;
    SDid2MinReinit     = false;
    SConnectRunning    = false;
    SBeginAttemptMs    = 0;
    SBackoffMs         = REJOIN_MIN_MS;

    // Grace-Periode nach GOT_IP: (34) zunächst nicht aggressiv behandeln
    SGraceAfterGotIpMs = SLastGotIpMs + SGraceMs;
    // Burst-Tracking zurücksetzen
    SMaBurstCount      = 0;
    SMaWindowStartMs   = 0;
 
    return;
   }
   else 
   {
    LSPrintf( "[WIFI] EVENT:%d RSSI=%d dBm\n", (int)ev, (int)WiFi.RSSI() );
   }
 }

//****************************************************************
// TWlanNanny
//****************************************************************

void TWlanNanny::Init( const TGNString & Hostname, const TGNString & WifiSsid,
                       const TGNString & WifiPassword )
 {
  FHostname     = Hostname;
  FWifiSsid     = WifiSsid;
  FWifiPassword = WifiPassword;
  Init();
 }

void TWlanNanny::Init(void)
 {
  #if defined( HOSTNAME ) && defined( WIFI_SID ) && defined( WIFI_PW )
    if ( FHostname.empty() && FWifiSsid.empty() )
     {
      FHostname     = HOSTNAME;
      FWifiSsid     = WIFI_SID;
      FWifiPassword = WIFI_PW;
     }
  #endif

  if ( FHostname.empty() || FWifiSsid.empty() )
   {
    LSPrintf( "[WlanNanny::Init] hostname or SSID not configured\n" );
    return;
   }

  static uint32_t SLastInitMs = 0;

  const uint32_t now = millis();
  if( SLastInitMs && now - SLastInitMs < 500 ) return;   // min 500 ms Abstand
  SLastInitMs = now;

  WiFi.mode( WIFI_STA );
  WiFi.setSleep( false ); // Powersave aus
  WiFi.persistent( false );
  WiFi.setHostname( FHostname.c_str() );  

  // Robustere PHY-Einstellungen (Bandbreite/Protokoll setzen)
  esp_wifi_set_bandwidth( WIFI_IF_STA, WIFI_BW_HT20 );
  esp_wifi_set_protocol( WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N );

  WiFi.setAutoReconnect( false ); // Nanny steuert Reconnects

  // Bandbreite/Protokoll/AutoReconnect loggen
  LSPrintf( "[WIFI] BW=%s, Proto=bgn, AutoReconnect=%d, Hostname=%s\n",
            BwName( WIFI_BW_HT20 ), 0, FHostname.c_str() );

  WiFi.begin( FWifiSsid.c_str(), FWifiPassword.c_str() );
  #if 0 // Ist hier schon auf max eingestellt
    ShowWifiPower( "Before setTxPower" ); 
    // Sendeleistung festsetzen (maximale Leistung)
    // Macros WIFI_POWER_..  sind falsch!
    // WiFi.setTxPower( WIFI_POWER_19_5dBm );
    // direkte func nutzen!
    esp_wifi_set_max_tx_power( 19.5 * 4 );   // direkt in 0.25-dBm Steps
    ShowWifiPower( "After setTxPower" ); 
  #endif

  if( SEventsHooked )
   {
    TSPrintf( "[WlanNanny::Init] olready started\n" );
    return;
   }
  SEventsHooked = true;
  TSPrintf( "[WlanNanny::Init] start\n" );
  WiFi.onEvent( OnEvent );
 }

void TWlanNanny::Loop(void)
 {
  static uint8_t SFailRounds = 0;
  const uint32_t now = millis();

  if( now - SLastStatusChk < 500 )
    return;
  // alle 500ms
  SLastStatusChk = now;

  // Unser eigener Connect-Versuch hängt? → Slot nach 15s freigeben
  if( SConnectRunning && SBeginAttemptMs && ( millis() - SBeginAttemptMs ) > 15000U )
   {
    LSPrintf( "[WIFI] connect attempt timeout → releasing slot\n" );
    SConnectRunning = false;
    SBeginAttemptMs = 0;
   }

  if( SDoCloseAll || SDoFlushNow )
   {
    bool doClose = SDoCloseAll;
    bool doFlush = SDoFlushNow;
    SDoCloseAll = false;
    SDoFlushNow = false;

    // zuerst Log sichern, dann WS schließen
    if( doFlush )
      SpecialLog.FlushNow();
    if( doClose )
      TAsyncWebServerEx::CloseAll();
   }

  // 2-Minuten-Reinit: genau 1× pro Outage
  if( SDisConnSinceMs && !SDid2MinReinit && ( now - SDisConnSinceMs ) > 2UL*60UL*1000UL )
   {
    LSPrintf( "[WIFI] seit >2 Minuten keine IP → hartes Re-Init\n" ); 
    SpecialLog.FlushNow();
    WiFi.disconnect( true, true );
    delay( 200 );
    Init();
    // Backoff neu starten
    SBackoffMs     = REJOIN_MIN_MS;
    SNextAttempt   = now + SBackoffMs;
    // Kein erneutes Reinit bis wieder GOT_IP gesehen wurde
    SDid2MinReinit = true;
    return;	
   }
   
  // Seit > 5 min kein GOT_IP  → hart neu
  if( SDisConnSinceMs && ( now - SDisConnSinceMs ) > 5UL*60UL*1000UL )
   EspResetNow( "[WIFI] no IP >5min -> esp_restart" );
  // Zu viele fehlende ACKs am Stück → hart neu
  if( SConsecMissingAcks >= 5 )
   EspResetNow( "[WIFI] too many MISSING_ACKS -> esp_restart" );

  if( WiFi.status() == WL_CONNECTED || now < SNextAttempt )
    return;

  // keine Versuche während Core-Defers
  if( millis() < SCoreDeferUntilMs ) return;
  if( SConnectRunning ) return;
  SConnectRunning = true;

  LSPrintf( "[WIFI] rejoin (backoff=%u ms)\n", (unsigned)SBackoffMs ); 

  bool kicked = WiFi.begin( FWifiSsid.c_str(), FWifiPassword.c_str() );
  SBeginAttemptMs = millis();
  ScheduleReconnect( now );

  // Wenn mehrfach erfolglos bei kleinem Backoff → Radio-Reset versuchen
  if( !kicked || SBackoffMs <= 2 * REJOIN_MIN_MS )
   {
    if( ++SFailRounds >= 3 )
     {
      LSPrintf( "[WIFI] radio restart\n" );
      WiFi.mode( WIFI_OFF );
      delay( 50 );
      Init();
      SFailRounds = 0;
     }
   }
   else
    SFailRounds = 0;
 }

#endif // ESP32
