// EspRescueUpdate.ino, Version: 1.17
//****************************************************************
// Includes
//****************************************************************

#include <GnBasics.h>
#include <GnCoopTimer.h>
#ifdef ESP32
  #include <WiFi.h>
  #include <WebServer.h>
  #include <LittleFS.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <LittleFS.h>
#endif

#include "EspRescueProject.h"
#include "EspRescueHardware.h"
#ifndef ESP32
  #include "EspUpdateSigning.h"
#endif
#include "EspWebUpdate.h"

//****************************************************************
// Configuration
//****************************************************************

namespace
 {
  constexpr unsigned long StaRetryMs = 10000;


 }

//****************************************************************
// Vars
//****************************************************************

#ifdef ESP32
WebServer HttpServer( 80 );
#else
ESP8266WebServer HttpServer( 80 );
#endif
TEspWebUpdate WebUpdate;

uint8_t NextWlanIndex = 0;

//****************************************************************
// Functions
//****************************************************************

void SetupHardware(void)
 {
  if( EspRescueHardware::KeyPin >= 0 )
    pinMode( EspRescueHardware::KeyPin, INPUT_PULLUP );

  if( EspRescueHardware::StatusLedPin >= 0 )
   {
    digitalWrite( EspRescueHardware::StatusLedPin, EspRescueHardware::StatusLedActiveLevel );
    pinMode( EspRescueHardware::StatusLedPin, OUTPUT );
   }
 }

void StartNextStation(void)
 {
  for( uint8_t Try = 0;  Try < 2;  ++Try )
   {
    const uint8_t Index = NextWlanIndex;
    NextWlanIndex = 1 - NextWlanIndex;

    char const * Ssid = Index ? ESP_RESCUE_WLAN_SSID2 : ESP_RESCUE_WLAN_SSID;
    char const * Pw = Index ? ESP_RESCUE_WLAN_PW2 : ESP_RESCUE_WLAN_PW;
    if( !Ssid[0] )
      continue;

    WiFi.begin( Ssid, Pw );
    return;
   }

 }

void StationRetryTimerCb( TGnCoopTimerBase * Tmr, uint16_t )
 {
  if( WiFi.status() == WL_CONNECTED )
   {
    Tmr->Stop();
    return;
   }
  StartNextStation();
 }

TGnCoopTimerMs & StationRetryTimer(void)
 {
  static TGnCoopTimerMs Timer( StationRetryTimerCb, StaRetryMs, false );
  return Timer;
 }

void PrepareFileSystem(void)
 {
#ifdef ESP32
  // Initial Shelly takeover must not format or rewrite an unknown stock filesystem.
  return;
#else
  LittleFS.setConfig( LittleFSConfig( false ) );
  if( LittleFS.begin() )
   {
    return;
   }

  Debug( "Can't mount LittleFS; rescue leaves it unchanged\n" );
  LittleFS.end();
#endif
 }

void SetupWifi(void)
 {
  WiFi.persistent( false );
  WiFi.mode( WIFI_AP_STA );
#ifdef ESP32
  WiFi.setHostname( ESP_RESCUE_PROJECT_NAME );
#else
  WiFi.hostname( ESP_RESCUE_PROJECT_NAME );
#endif
  WiFi.setAutoReconnect( false );

  String ApSsid( ESP_RESCUE_PROJECT_NAME );
  ApSsid += F( "-Rescue" );
  WiFi.softAP( ApSsid.c_str() );

  NextWlanIndex = 0;
  StartNextStation();
  StationRetryTimer().Start();
 }

void SetupHttpServer(void)
 {
#ifdef ESP32
  WebUpdate.SetupRecovery( HttpServer, ESP_RESCUE_PROJECT_NAME );
#else
  WebUpdate.SetupRecovery( HttpServer,
    &EspUpdateSigning::Hash, &EspUpdateSigning::Verifier, ESP_RESCUE_PROJECT_NAME );
#endif
  HttpServer.begin();
 }

//****************************************************************
// Main
//****************************************************************

void setup(void)
 {
  SetupHardware();
  PrepareFileSystem();
  SetupWifi();
  SetupHttpServer();
 }

void loop(void)
 {
  LoopInstances.Loop();
  HttpServer.handleClient();

  if( WiFi.status() == WL_CONNECTED )
   {
    StationRetryTimer().Stop();
    return;
   }

  if( !StationRetryTimer().Enabled() )
   {
    StartNextStation();
    StationRetryTimer().Start();
   }
 }
