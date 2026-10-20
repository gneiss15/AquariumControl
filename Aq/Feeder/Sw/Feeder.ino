// Feeder.ino, Version: 1.19
//****************************************************************
// Includes
//****************************************************************

#include <GnBasics.h>
#include <GnUdpStatus.h>
#include <LittleFS.h>

#include "FeederConfig.h"
/*
#error Check Hw-PWM
#https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/api/ledc.html
#	ledc... uses Hardware PWM !!
#https://deepbluembedded.com/high-resolution-pwm-using-ccp-compare-and-timer-module/
#https://dronebotworkshop.com/esp32-servo/
*/
#include "FeederDrive.h"

#include "FeederWifi.h"
#include "FeederStatusApi.h"
#include "FeederTemperature.h"
#include "FeederWebSocket.h"
#include "FeedTimes.h"
#include "FeederTimeSync.h"
#include "Libs/Common/EspUpdate/EspIndexHtmlCheck.h"
#include <MyFtpServer.h>

//****************************************************************
// Sw Configuration
//****************************************************************

//****************************************************************
// Hw Configuration
//****************************************************************

/******************************************
               Wireing

Servo        RotEnc  +++-WifiKit8-+++   Farbe
                          gpio
-----------+-------+-----------------+-------
GND/braun  |       | GND         GND | schwarz
5V/rot     |       | 5V          5V  | weiß
           |  3V3  | 3V3         3V3 | rot
           |  GND  | GND         GND | braun
           |       | D2    4     CTS |
PWM/orange |       | D3    0     DTR | grau
           |       | D8   15  5  D1  |
           |  Dt   | D7   13  1  TX  | gelb
           |  Clk  | D6   12  3  RX  | grün
           |  Sw   | SCL  14     RST | orange
           |       | D0   16  2  SDA |
           |       | A0          EN  |
******************************************/

/* Pins are defined in FeederConfig!
ServoPin  0 // Output Pin for Servo
DirPin   13 // Direction-Input-Pin from RotEncoder
ClkPin   12 // Clock-Input-Pin from RotEncoder
BtnPin   14 // Button-Input-Pin from RotEncoder
DallasPin 2 // OneWire data pin for optional DS18B20, external pull-up to 3V3 required
            // GPIO4/D2 and GPIO5/D1 are used by the on-board OLED.
            // GPIO15/D8 is not suitable because Dallas needs a pull-up while GPIO15 must boot LOW.
*/

//****************************************************************
// Main
//****************************************************************

#if !defined( ESP32 )
void StartStartupError( uint8_t StartupLevel, char const * Error )
 {
  Debug( "StartupLevel %u: %s\n", StartupLevel, Error );
  bool Started = false;
  if( StartupLevel == 0 )
    Started = FeederWifi.SetupRecovery( "Feeder" );
   else if( StartupLevel == 1 )
    Started = FeederWifi.SetupRecoveryNetwork( "Feeder" );
   else
    Started = FeederWifi.SetupStartupError( "Feeder", Error );
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
  FeederServo.SetupSettings();
  FeedTimes.Setup();
#else
  if( !FeederServo.SetupSettings() )
   {
    StartStartupError( StartupLevel, "Can't load /ServoSteps.json" );
    return;
   }
  if( !FeedTimes.Setup() )
   {
    StartStartupError( StartupLevel, "Can't load /FeedTimes.json" );
    return;
   }
#endif

  MyFtpServer.SetConfig( FeederConfig.Hostname, FeederConfig.FtpPw, FeederConfig.WifiMode );
  MyFtpServer.FileSystemMounted( FileSystemMounted );

  #if 0 //D
  if( FileSystemMounted )
   {
    struct FSInfo fs_info;
    LittleFS.info( fs_info );
    Debug
     (
      "FSInfo:\n"
      "  totalBytes: %u\n"
      "  usedBytes: %u\n"
      "  blockSize: %u\n"
      "  pageSize: %u\n"
      "  maxOpenFiles: %u\n"
      "  maxPathLength: %u\n\n",
      fs_info.totalBytes,
      fs_info.usedBytes,
      fs_info.blockSize,
      fs_info.pageSize,
      fs_info.maxOpenFiles,
      fs_info.maxPathLength
     );
   }
  #endif

  if( !FeederServo.SetupHardware() )
   {
#if defined( ESP32 )
    FeederStatusApi.SetupError( "Invalid servo hardware configuration" );
#else
    StartStartupError( StartupLevel, "Invalid servo hardware configuration" );
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
