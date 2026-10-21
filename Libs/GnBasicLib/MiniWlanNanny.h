// MiniWlanNanny.h, Version: 1.03
#pragma once

#include <GnPlatform.h>

#if defined( GNPlatformESP32 )

/*
MiniWlanNanny.Begin();              // einmalig zum Instanzieren (wie bei dir üblich)
MiniWlanNanny.TouchTraffic();       // bei JEDEM relevanten RX/TX callen
MiniWlanNanny.TimeoutMs(120000);    // optional: Timeout anpassen
MiniWlanNanny.Enabled(true/false);  // optional: deaktivieren
*/

//****************************************************************
// Includes
//****************************************************************
#include <GnBasics.h>
#include <GnCoopTimer.h>

//****************************************************************
// TMiniWlanNanny
//****************************************************************

class TMiniWlanNanny : public TSingleton<TMiniWlanNanny>
 {
 public:
                        TMiniWlanNanny(void)              : FEnabled( true )
                                                          , FTimeoutMs( 120000UL )  // default: 2 minutes
                                                          , FLastTrafficMs( 0 )
                                                          , FCheckTimer( Closure( this, &TMiniWlanNanny::CheckTimerCb ), 1000 )
                                                          , FSnifferOn( false )
                                                           {}

  // ensure instantiation (call once at boot)
  void                  Begin( bool snifferOn = true )    { EnableSniffer( snifferOn ); }

  // mark traffic observed "now"
  inline void           TouchTraffic(void)                { FLastTrafficMs = millis(); }

  // configuration
  inline void           TimeoutMs( uint32_t ms )          { FTimeoutMs = ms; }
  inline uint32_t       TimeoutMs(void) const             { return FTimeoutMs; }
  inline void           Enabled( bool en )                { FEnabled = en; }
  void                  EnableSniffer( bool on );         // optional: Sniffer steuern (default: off bis EnableSniffer(true) gerufen wird)

  // Für den Promiscuous-Callback: prüft, ob addr die STA-MAC ist
  inline bool           MatchStaMac( const uint8_t * addr ) const;

 private:
  void                  GotMac( const char * msg );
  void                  CheckTimerCb( TGnCoopTimerBase * Tmr, uint16_t IntervalsElapsed );

 private:
  bool                  FEnabled;
  uint32_t              FTimeoutMs;
  volatile uint32_t     FLastTrafficMs;
  TGnCoopTimerMs        FCheckTimer;
  bool                  FSnifferOn;
  bool                  FSnifferPending;
  uint8_t               FStaMac[ 6 ]                      = { 0 };
 };
#define MiniWlanNanny   TMiniWlanNanny::Instance()

#endif // ESP32
