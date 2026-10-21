// TmrSwMain.inc.h, Version: 1.06
#pragma once

#include <GnBasics.h>
#include <GnCoopTimer.h>
#include <GnUdpStatus.h>
#include "TmrSwProject.h"
#include "TmrSwIncludes.h"
#include "TmrSwConfig.h"
#include "TmrSwHw.h"
#include "TmrSwChannels.h"
#include "SwitchTimes.h"
#include "TmrSwTimeSync.h"
#include "TmrSwWifi.h"
#include "TmrSwStatusApi.h"
#include "TmrSwWebSocket.h"
#include "../EspUpdate/EspIndexHtmlCheck.h"
#include <MyFtpServer.h>

void Every500msTimerCb( TGnCoopTimerBase *, uint16_t )
 {
  static uint8_t Tick = 0;
  TmrSwWifi.Every500ms( ++Tick );
  if( TmrSwWifi.SwitchingEnabled() )
    SwitchTimes.UpdateHeater();
 }

void StartStartupError( uint8_t StartupLevel, char const * Error )
 {
  Debug( "StartupLevel %u: %s\n", StartupLevel, Error );
  bool Started = false;
  if( StartupLevel == 0 )
    Started = TmrSwWifi.SetupRecovery( TMR_SW_PROJECT_NAME );
   else if( StartupLevel == 1 )
    Started = TmrSwWifi.SetupRecoveryNetwork( TMR_SW_PROJECT_NAME );
   else
    Started = TmrSwWifi.SetupStartupError( TMR_SW_PROJECT_NAME, Error );
  if( !Started )
    Debug( "Can't start startup recovery mode\n" );
 }

void setup(void)
 {
  uint8_t StartupLevel = 0;
  TmrSwHw.Setup();

  WiFi.persistent( false );
  Debug( "\nStart %s\n", TMR_SW_PROJECT_NAME );

#if defined( ESP8266 )
  LittleFS.setConfig( LittleFSConfig( false ) );
  const bool FileSystemMounted = LittleFS.begin();
#elif defined( ESP32 )
  const bool FileSystemMounted = LittleFS.begin( false, "/littlefs", 10,
    TMR_SW_FILESYSTEM_PARTITION_LABEL );
#else
  #error Unsupported platform
#endif
  TmrSwStatusApi.FileSystemMounted( FileSystemMounted );
  if( !FileSystemMounted )
   {
    StartStartupError( StartupLevel, "Can't mount LittleFS" );
    return;
   }

  if( !TmrSwConfigInstance.Setup() )
   {
    StartStartupError( StartupLevel, "Can't load /Config.json" );
    return;
   }
  StartupLevel = 1;

  if( !EspIndexHtmlValid() )
   {
    StartStartupError( StartupLevel, "Invalid /Index.html" );
    return;
   }
  StartupLevel = 2;

  if( !TmrSwRecoverConfiguration() )
   { StartStartupError( StartupLevel, "Can't recover configuration transaction" ); return; }
  if( !TmrSwChannels.Setup() )
   {
    StartStartupError( StartupLevel, "Can't load /Channels.json" );
    return;
   }

  if( !SwitchTimes.Setup() )
   {
    StartStartupError( StartupLevel, "Can't load /SwitchTimes.json" );
    return;
   }

  MyFtpServer.SetConfig( TmrSwConfig.Hostname, TmrSwConfig.FtpPw,
    TmrSwConfig.WifiMode );
  MyFtpServer.FileSystemMounted( FileSystemMounted );

  if( !TmrSwWifi.SetupNetwork() )
   {
    StartStartupError( StartupLevel, "TmrSw WiFi setup failed" );
    return;
   }

  if( !GnUdpStatus.Setup( TmrSwConfig.Hostname.c_str(),
       []( uint64_t & Signature ) { return TmrSwStatusApi.StatusJson( Signature ); } ) )
   {
    Debug( "UDP status setup failed: %s\n", GnUdpStatus.Error().c_str() );
    TmrSwStatusApi.SetupError( GnUdpStatus.Error() );
   }

  TGNString WebSocketError;
  const bool WebSocketStarted = TmrSwWebSocket.Setup( 81, WebSocketError );
  TmrSwStatusApi.WebSocketStarted( WebSocketStarted );
  if( !WebSocketStarted )
   {
    TmrSwStatusApi.SetupError( WebSocketError );
    StartStartupError( StartupLevel, WebSocketError.c_str() );
    return;
   }

  if( !TmrSwWifi.SetupWeb() )
   {
    StartStartupError( StartupLevel, "TmrSw HTTP setup failed" );
    return;
   }
  GnTimeSync.Setup( TmrSwTimeSyncConfig, TmrSwConfig.Hostname, TmrSwTimeSyncApply );
 }

void loop(void)
 {
  static TGnCoopTimerMs Every500msTimer( Every500msTimerCb, 500 );
  LoopInstances.Loop();
 }
