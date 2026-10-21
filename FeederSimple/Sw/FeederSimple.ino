// FeederSimple.ino, Version: 1.11
#include <GnBasics.h>
#include <GnUdpStatus.h>
#include <LittleFS.h>

#include "FeederConfig.h"
#include "FeederDrive.h"
#include "FeederWifi.h"
#include "FeederStatusApi.h"
#include "FeederTemperature.h"
#include "FeederWebSocket.h"
#include "FeedTimes.h"
#include "FeederTimeSync.h"
#include "Libs/Common/EspUpdate/EspIndexHtmlCheck.h"
#include <MyFtpServer.h>

#if !defined( ESP32 )
void StartStartupError( uint8_t StartupLevel, char const * Error )
 {
  Debug( "StartupLevel %u: %s\n", StartupLevel, Error );
  bool Started = false;
  if( StartupLevel == 0 )
    Started = FeederWifi.SetupRecovery( "FeederSimple" );
   else if( StartupLevel == 1 )
    Started = FeederWifi.SetupRecoveryNetwork( "FeederSimple" );
   else
    Started = FeederWifi.SetupStartupError( "FeederSimple", Error );
  if( !Started )
    Debug( "Can't start startup update mode\n" );
 }
#endif

void setup(void)
 {
#if !defined( ESP32 )
  uint8_t StartupLevel = 0;
#endif
  WiFi.persistent( false );
  Debug( "\nStart\n" );

#if !defined( ESP32 )
  LittleFS.setConfig( LittleFSConfig( false ) );
#endif
  const bool FileSystemMounted = LittleFS.begin();
  FeederStatusApi.FileSystemMounted( FileSystemMounted );
#if defined( ESP32 )
  if( !FileSystemMounted )
    Debug( "Can't mount LittleFS\n" );
#else
  if( !FileSystemMounted )
   {
    StartStartupError( StartupLevel, "Can't mount LittleFS" );
    return;
   }
#endif


  if( !FeederConfigInstance.Setup() )
   {
#if defined( ESP32 )
    Debug( "Can't load /Config.json\n" );
#else
    StartStartupError( StartupLevel, "Can't load /Config.json" );
    return;
#endif
   }
#if !defined( ESP32 )
  StartupLevel = 1;
#endif

#if !defined( ESP32 )
  if( !EspIndexHtmlValid() )
   {
    StartStartupError( StartupLevel, "Invalid /Index.html" );
    return;
   }
  StartupLevel = 2;
#endif

#if defined( ESP32 )
  FeedTimes.Setup();
#else
  if( !FeedTimes.Setup() )
   {
    StartStartupError( StartupLevel, "Can't load /FeedTimes.json" );
    return;
   }
#endif

  MyFtpServer.SetConfig( FeederConfig.Hostname, FeederConfig.FtpPw, FeederConfig.WifiMode );
  MyFtpServer.FileSystemMounted( FileSystemMounted );

  FeederDrive.Setup();
  if( FeederDrive.Error()[ 0 ] != '\0' )
   {
#if defined( ESP32 )
    FeederStatusApi.SetupError( FeederDrive.Error() );
#else
    StartStartupError( StartupLevel, FeederDrive.Error() );
    return;
#endif
   }
  FeederTemperature.Setup();
  if( !FeederWifi.SetupNetwork() )
   {
#if defined( ESP32 )
    FeederStatusApi.SetupError( "Feeder WiFi setup failed" );
#else
    StartStartupError( StartupLevel, "Feeder WiFi setup failed" );
    return;
#endif
   }

  if( !GnUdpStatus.Setup( FeederConfig.Hostname.c_str(),
       []( uint64_t & Signature ) { return FeederStatusApi.StatusJson( Signature ); },
       []()
        {
         TGnUdpTemperature Temperature;
         Temperature.Available = FeederTemperature.Enabled();
         Temperature.Valid = FeederTemperature.Valid();
         if( Temperature.Valid )
           Temperature.Celsius = FeederTemperature.TemperatureCelsius();
         return Temperature;
        } ) )
   {
    Debug( "UDP status setup failed: %s\n", GnUdpStatus.Error().c_str() );
    FeederStatusApi.SetupError( GnUdpStatus.Error() );
   }

  TGNString WebSocketError;
  const bool WebSocketStarted = FeederWebSocket.Setup( 81, WebSocketError );
  FeederStatusApi.WebSocketStarted( WebSocketStarted );
  if( !WebSocketStarted )
   {
    FeederStatusApi.SetupError( WebSocketError );
#if !defined( ESP32 )
    StartStartupError( StartupLevel, WebSocketError.c_str() );
    return;
#endif
   }

  if( !FeederWifi.SetupWeb() )
   {
#if defined( ESP32 )
    FeederStatusApi.SetupError( "Feeder HTTP setup failed" );
#else
    StartStartupError( StartupLevel, "Feeder HTTP setup failed" );
    return;
#endif
   }
  GnTimeSync.Setup( FeederTimeSyncConfig, FeederConfig.Hostname, FeederTimeSyncApply );

#if !defined( ESP32 )
  StartupLevel = 3;
#endif
 }

void loop(void)
 {
  LoopInstances.Loop();
 }
