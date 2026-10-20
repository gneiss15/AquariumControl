// TmrSwWifi.cpp, Version: 4.18
//****************************************************************
// Includes
//****************************************************************

#include <GnBasics.h>
#include <GnDeviceIdentity.h>
#include "TmrSwWifi.h"

#include "TmrSwConfig.h"
#include "TmrSwChannels.h"
#include "TmrSwHw.h"
#include "TmrSwA8TPowerMeter.h"
#include "SwitchTimes.h"
#include <ArduinoJson.h>
#include <GnTimeSync.h>
#include "TmrSwStatusApi.h"
#include "DeviceType.inc.h"
#include "Libs/Common/EspUpdate/EspSignedArtifact.h"
#include <algorithm>
#include <cstring>
#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <nvs.h>

/*
#include "TmrSwConfig.h"
#include "TmrSwChannels.h"
#include "TmrSwHw.h"
#include "SwitchTimes.h"
#include "TmrSwWifi.h"
*/

//****************************************************************
// Sw Configuration
//****************************************************************

//****************************************************************
// Vars
//****************************************************************

//****************************************************************
// Functions
//****************************************************************

//****************************************************************
// TTmrSwWifi
//****************************************************************

namespace
 {
constexpr char RescueNvsNamespace[] = "AqRescue";
constexpr char RescueWlanKey[] = "Wlan";
constexpr size_t RescueSignedFixedPrefixSize = 64 + 48;
constexpr size_t RescueMaximumSignatureSize = 512;
constexpr uint32_t RescuePartitionSize = 0xD0000;

struct TRescueUploadState
 {
  uint8_t Prefix[RescueSignedFixedPrefixSize + RescueMaximumSignatureSize];
  size_t PrefixSize = 0;
  uint16_t SignatureSize = 0;
  bool SignatureSizeKnown = false;
  bool PrefixVerified = false;
  bool WriteStarted = false;
  size_t PayloadWritten = 0;
  esp_partition_t const * Partition = nullptr;
  TEspSignedArtifactInfo Info = {};
  TEspSignedArtifactHash Hash;
  String Error;

  void Reset(void)
   {
    PrefixSize = 0;
    SignatureSize = 0;
    SignatureSizeKnown = false;
    PrefixVerified = false;
    WriteStarted = false;
    PayloadWritten = 0;
    Partition = nullptr;
    Info = {};
    Error.clear();
   }
 };

TRescueUploadState RescueUpload;

void SetRescueUploadError( String const & Error )
 {
  if( RescueUpload.Error.length() == 0 )
    RescueUpload.Error = Error;
 }

bool VerifyRescueTarget(void)
 {
  RescueUpload.Partition = esp_partition_find_first(
    ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "safeboot" );
  esp_partition_t const * Running = esp_ota_get_running_partition();
  if( !RescueUpload.Partition || RescueUpload.Partition->address != 0x10000 ||
      RescueUpload.Partition->size != RescuePartitionSize )
   {
    SetRescueUploadError( F( "Rescue partition layout mismatch" ) );
    return false;
   }
  if( !Running || Running->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0 )
   {
    SetRescueUploadError( F( "Rescue can only be updated while Main/app0 is running" ) );
    return false;
   }
  return true;
 }

bool ProcessRescueUploadData( uint8_t const * Data, size_t Size )
 {
  while( Size && RescueUpload.Error.length() == 0 )
   {
    if( !RescueUpload.PrefixVerified )
     {
      size_t Required = RescueUpload.SignatureSizeKnown ?
        EspSignedArtifactPrefixSize( RescueUpload.SignatureSize ) :
        RescueSignedFixedPrefixSize;
      const size_t CopySize = std::min( Size, Required - RescueUpload.PrefixSize );
      memcpy( RescueUpload.Prefix + RescueUpload.PrefixSize, Data, CopySize );
      RescueUpload.PrefixSize += CopySize;
      Data += CopySize;
      Size -= CopySize;

      if( !RescueUpload.SignatureSizeKnown &&
          RescueUpload.PrefixSize == RescueSignedFixedPrefixSize )
       {
        String Error;
        if( !EspSignedArtifactReadSignatureSize( RescueUpload.Prefix,
             RescueUpload.PrefixSize, RescueUpload.SignatureSize, Error ) ||
            RescueUpload.SignatureSize > RescueMaximumSignatureSize )
         {
          SetRescueUploadError( Error.length() ? Error :
            F( "RSA signature is too large" ) );
          return false;
         }
        RescueUpload.SignatureSizeKnown = true;
        Required = EspSignedArtifactPrefixSize( RescueUpload.SignatureSize );
       }

      if( RescueUpload.SignatureSizeKnown &&
          RescueUpload.PrefixSize == EspSignedArtifactPrefixSize( RescueUpload.SignatureSize ) )
       {
        String Error;
        if( !EspSignedArtifactVerifyPrefix( RescueUpload.Prefix,
             RescueUpload.PrefixSize, TEspSignedArtifactType::Rescue,
             ESP_UPDATE_DEVICE_TYPE, 0, RescuePartitionSize,
             RescueUpload.Info, Error ) || !VerifyRescueTarget() )
         {
          if( RescueUpload.Error.length() == 0 )
            SetRescueUploadError( Error );
          return false;
         }
        RescueUpload.PrefixVerified = true;
       }
      continue;
     }

    if( !RescueUpload.WriteStarted )
     {
      if( Data[0] != 0xE9 )
       {
        SetRescueUploadError( F( "Rescue payload is not an ESP32 application image" ) );
        return false;
       }
      if( esp_partition_erase_range( RescueUpload.Partition, 0,
           RescueUpload.Partition->size ) != ESP_OK )
       {
        SetRescueUploadError( F( "Could not erase Rescue partition" ) );
        return false;
       }
      RescueUpload.Hash.Begin();
      RescueUpload.WriteStarted = true;
     }

    const size_t Remaining = RescueUpload.Info.PayloadSize - RescueUpload.PayloadWritten;
    if( !Remaining )
     {
      SetRescueUploadError( F( "Unexpected data after Rescue payload" ) );
      return false;
     }
    const size_t WriteSize = std::min( Size, Remaining );
    RescueUpload.Hash.Add( Data, WriteSize );
    if( esp_partition_write( RescueUpload.Partition, RescueUpload.PayloadWritten,
         Data, WriteSize ) != ESP_OK )
     {
      SetRescueUploadError( F( "Could not write Rescue partition" ) );
      return false;
     }
    RescueUpload.PayloadWritten += WriteSize;
    Data += WriteSize;
    Size -= WriteSize;
   }
  return RescueUpload.Error.length() == 0;
 }

bool VerifyWrittenRescue(void)
 {
  if( !RescueUpload.PrefixVerified || !RescueUpload.WriteStarted ||
      RescueUpload.PayloadWritten != RescueUpload.Info.PayloadSize )
   {
    SetRescueUploadError( F( "Rescue upload is incomplete" ) );
    return false;
   }
  if( !RescueUpload.Hash.EndAndCompare( RescueUpload.Info.PayloadHash ) )
   {
    SetRescueUploadError( F( "Rescue payload hash mismatch" ) );
    return false;
   }

  TEspSignedArtifactHash ReadBackHash;
  ReadBackHash.Begin();
  uint8_t Buffer[1024];
  size_t Offset = 0;
  while( Offset < RescueUpload.Info.PayloadSize )
   {
    const size_t ReadSize = std::min( sizeof( Buffer ),
      size_t( RescueUpload.Info.PayloadSize - Offset ) );
    if( esp_partition_read( RescueUpload.Partition, Offset, Buffer, ReadSize ) != ESP_OK )
     {
      SetRescueUploadError( F( "Could not verify written Rescue partition" ) );
      return false;
     }
    ReadBackHash.Add( Buffer, ReadSize );
    Offset += ReadSize;
   }
  if( !ReadBackHash.EndAndCompare( RescueUpload.Info.PayloadHash ) )
   {
    SetRescueUploadError( F( "Written Rescue verification failed" ) );
    return false;
   }

  esp_app_desc_t Description = {};
  if( esp_ota_get_partition_description( RescueUpload.Partition, &Description ) != ESP_OK )
   {
    SetRescueUploadError( F( "Written Rescue image is not a valid ESP32 application" ) );
    return false;
   }
  return true;
 }

void StoreRescueWlanPreference(void)
 {
  if( WiFi.status() != WL_CONNECTED )
    return;

  const String CurrentSsid = WiFi.SSID();
  uint8_t Index;
  if( CurrentSsid == TmrSwConfig.WlanSsid )
    Index = 0;
  else if( CurrentSsid == TmrSwConfig.WlanSsid2 )
    Index = 1;
  else
    return;

  nvs_handle_t Handle;
  if( nvs_open( RescueNvsNamespace, NVS_READWRITE, &Handle ) != ESP_OK )
    return;
  if( nvs_set_u8( Handle, RescueWlanKey, Index ) == ESP_OK )
    nvs_commit( Handle );
  nvs_close( Handle );
 }
 }

TTmrSwWifi::TTmrSwWifi( void )
: FSsidNr( 0 )
, F500msConnectTicker( 0 )
, FFirstTimeValid( false )
, FSwitchingEnabled( false )
, FHttpServer( 80 )
, FLastMinuteOfWeek( FInvalidMinuteOfWeek )
 {
 }

time_t TTmrSwWifi::GetTime(void)
 {
#if 0 // Debug
  static time_t debugTime = 0;
  const time_t timeNow = debugTime++;
#else
  const time_t timeNow = time( nullptr );
#endif
  localtime_r( &timeNow, &FTm );
  return timeNow;
 }

/*virtual*/ void TTmrSwWifi::Loop(void)
 {
  FHttpServer.handleClient();
  GetTime();
  const uint16_t currentMinuteOfWeek = GnMinuteOfWeek( FTm );
  if( !FSwitchingEnabled || currentMinuteOfWeek == FLastMinuteOfWeek )
    return;

  // A regular one-minute step only needs to check the cached next event.
  // Clock corrections, DST changes and longer gaps require a full state rebuild.
  const bool consecutiveMinute = FLastMinuteOfWeek != FInvalidMinuteOfWeek &&
    currentMinuteOfWeek == ( FLastMinuteOfWeek + 1 ) % FMinutesPerWeek;
  FLastMinuteOfWeek = currentMinuteOfWeek;
  SwitchTimes.DoEvents( FTm, /* useNextMow = */ consecutiveMinute );
 }

struct tm TTmrSwWifi::GetTm(void)
 {
  GetTime();
  return FTm;
 }

#define Max500msForConnect 20
void TTmrSwWifi::Every500ms( uint8_t tick )
 {
  if( !FTimeIsValid && time( nullptr ) >= 1700000000 )
    STimeIsSet_cb();

  if( WiFi.status() != WL_CONNECTED )
   {
    // not connected: Blink-Period: 1sec
    TmrSwHw.WiFiLedToggle();
    if( ++F500msConnectTicker > Max500msForConnect )
     {
      if( FSsidNr == 1 && TmrSwConfig.WlanSsid2.length() > 0 )
        StartSTA( TmrSwConfig.WlanSsid2, TmrSwConfig.WlanPw2 );
       else
        StartSTA( TmrSwConfig.WlanSsid, TmrSwConfig.WlanPw );
      FSsidNr = 1 - FSsidNr;
     }
   }
   else if( !TimeIsValid() )
   {
    // connected, but no time: Blink-Period: 2sec
    FSwitchingEnabled = false;
    if( tick & 1 )
      TmrSwHw.WiFiLedToggle();
   }
   else if( !FFirstTimeValid )
   {
    Debug( "Connected, AND FirstTimeValid\n" ); //D

    // Enable switching only after the first valid time synchronization.
    FFirstTimeValid = true;
    GetTime();
    FLastMinuteOfWeek = GnMinuteOfWeek( FTm );
    FSwitchingEnabled = true;
    SwitchTimes.DoEvents( FTm /* , useNextMow = false */ );

    // In normal operation the status LED follows the relay state.
    TmrSwHw.RelayStatus() ? TmrSwHw.WiFiLedOn() : TmrSwHw.WiFiLedOff();
   }
   else
   {
    // WLAN and time are valid: indicate the physical relay state.
    TmrSwHw.RelayStatus() ? TmrSwHw.WiFiLedOn() : TmrSwHw.WiFiLedOff();
   }
 }

bool TTmrSwWifi::Setup( void )
 {
  return SetupNetwork() && SetupWeb();
 }

bool TTmrSwWifi::SetupNetwork( void )
 {
  return SetupWifi() && SetupNTP();
 }

bool TTmrSwWifi::SetupWeb( void )
 {
  return SetupHttpServer();
 }

bool TTmrSwWifi::SelectRescueBoot(void)
 {
  const esp_partition_t * Rescue = esp_partition_find_first(
    ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "safeboot" );
  if( !Rescue || Rescue->address != 0x10000 || Rescue->size != 0xD0000 )
   {
    Debug( "Rescue partition not found or invalid\n" );
    return false;
   }
  esp_app_desc_t Description;
  if( esp_ota_get_partition_description( Rescue, &Description ) != ESP_OK )
   {
    Debug( "Rescue partition does not contain a valid application\n" );
    return false;
   }
  const esp_err_t Error = esp_ota_set_boot_partition( Rescue );
  if( Error != ESP_OK )
   {
    Debug( "Can't select Rescue boot partition: %s\n", esp_err_to_name( Error ) );
    return false;
   }
  return true;
 }

bool TTmrSwWifi::StartRescueFromStartup(void)
 {
  if( !SelectRescueBoot() )
    return false;
  delay( 200 );
  ESP.restart();
  return true;
 }

bool TTmrSwWifi::SetupRecovery( char const * )
 {
  return StartRescueFromStartup();
 }

bool TTmrSwWifi::SetupRecoveryNetwork( char const * )
 {
  return StartRescueFromStartup();
 }

bool TTmrSwWifi::SetupStartupError( char const *, String const & Error )
 {
  Debug( "Startup error -> Rescue: %s\n", Error.c_str() );
  return StartRescueFromStartup();
 }

//****************************************************************
// TTmrSwWifi (Wifi)
//****************************************************************

void TTmrSwWifi::StartSTA( String const & ssid, String const & pw )
 {
  WiFi.disconnect( false, false );
  delay( 100 );
  const wl_status_t Status = WiFi.begin( ssid, pw );
  Debug( "StartSTA: Connecting to WiFi, SSID: %s\n", ssid.c_str() ); //D
  if( Status == WL_CONNECT_FAILED )
    Debug( "StartSTA: WiFi.begin failed\n" ); //D
  F500msConnectTicker = 0;
 }

bool TTmrSwWifi::SetupWifi( void )
 {
  // Set WiFi to configured mode and disconnect from an AP if it was previously connected
  WiFi.disconnect( true );
  delay( 100 );
  WiFi.mode( TmrSwConfig.WifiMode );

  if( TmrSwConfig.WifiMode & WIFI_STA )
   {
    WiFi.setAutoReconnect( false );
    WiFi.setHostname( TmrSwConfig.Hostname.c_str() );
    FSsidNr = 1;
    StartSTA( TmrSwConfig.WlanSsid, TmrSwConfig.WlanPw );
   }
   else
    WiFi.setAutoReconnect( false );

  if( TmrSwConfig.WifiMode & WIFI_AP )
   {
    //!_WiFi.softAPConfig( /*ip*/IPAddress( 192,168,4,1 ), /*gateway*/IPAddress( 192,168,4,200 ), /*subnet*/IPAddress( 255,255,255,0 ) );
    /*bool apOk =*/ WiFi.softAP( TmrSwConfig.ApSsid, TmrSwConfig.ApPw );
    //Log( "Starting soft-AP (Pw): %s\n", apOk ? "Ready" : "Failed!" );
    //Log( "Soft-AP IP address: %s\n", WiFi.softAPIP().toString().c_str() );
   }

  return true;
 }

//****************************************************************
// TTmrSwWifi (NTP)
//****************************************************************

bool TTmrSwWifi::FTimeIsValid = false;

void TTmrSwWifi::STimeIsSet_cb( void )
 {
  //D Debug( "TTmrSwWifi::STimeIsSet_cb\n" );
  if( TTmrSwWifi::FTimeIsValid )
    return;
  TTmrSwWifi::FTimeIsValid = true;
 }

bool TTmrSwWifi::SetupNTP( void )
 {
  //if( TmrSwConfig.WifiMode & WIFI_STA )
   {
    // Setp Ntp
    // install callback - called when settimeofday is called (by SNTP or us)
    // once enabled (by DHCP), SNTP is updated every hour
    configTzTime( TmrSwConfig.TZ.c_str(), TmrSwConfig.NtpServerAdr.c_str() );
   }

  return true;
 }

#if 0
// OPTIONAL: change SNTP startup delay
// a weak function is already defined and returns 0 (RFC violation)
// it can be redefined:
//uint32_t sntp_startup_delay_MS_rfc_not_less_than_60000 ()
//{
//    //info_sntp_startup_delay_MS_rfc_not_less_than_60000_has_been_called = true;
//    return 60000; // 60s (or lwIP's original default: (random() % 5000))
//}

// OPTIONAL: change SNTP update delay
// a weak function is already defined and returns 1 hour
// it can be redefined:
uint32_t sntp_update_delay_MS_rfc_not_less_than_15000( void )
 {
  //info_sntp_update_delay_MS_rfc_not_less_than_15000_has_been_called = true;
  // return 15 * 1000; // 15s
  return 15 * 60 * 1000; // 15min
 }
#endif

//****************************************************************
// HttpServer Template-Code
//****************************************************************

#if 0
void TTmrSwWifi::SHandle....( void )
 {
  #if 0 //D Debug/language
  if( HttpServer.hasHeader( "accept-language" ) )
   {
    // de-DE,de;q=0.9,en-US;q=0.8,en;q=0.7
    String lang( HttpServer.header( "accept-language" ) );
    int s = lang.indexOf( ',' );
    int e = lang.indexOf( ';' );
    Debug( "SHandleRoot: accept-language: %s\n", HttpServer.header( "accept-language" ).c_str() );
   }
  #endif

  HttpServer.send( 200, "text/html", ReadFile( F( "/Index.html" ) ) );
 }
#endif

//****************************************************************
// TTmrSwWifi (HttpServer & FwUpdater)
//****************************************************************

#define HttpServer TmrSwWifi.FHttpServer

void TTmrSwWifi::SHandleNotFound( void )
 {
  String msg = "File Not Found\n";
  msg += "\nURI: " + HttpServer.uri();
  msg += "\nMethod: " + ( HttpServer.method() == HTTP_GET ) ? "GET" : "POST";
  msg += "\nArguments: " + HttpServer.args();
  msg += "\n";
  for( uint8_t i = 0;  i < HttpServer.args();  ++i )
    msg += " " + HttpServer.argName( i ) + ": " + HttpServer.arg( i )  + "\n";
  HttpServer.send( 404, "text/plain", msg );
 }

void TTmrSwWifi::SSendJson( TGNString const & Json )
 {
  HttpServer.sendHeader( "Access-Control-Allow-Origin", "*" );
  HttpServer.sendHeader( "Cache-Control", "no-store" );
  HttpServer.send( 200, "application/json", Json.c_str() );
 }

void TTmrSwWifi::SHandleApiOptions(void)
 {
  HttpServer.sendHeader( "Access-Control-Allow-Origin", "*" );
  HttpServer.sendHeader( "Access-Control-Allow-Methods", "GET, POST, OPTIONS" );
  HttpServer.sendHeader( "Access-Control-Allow-Headers", "Content-Type" );
  HttpServer.send( 204, "text/plain", "" );
 }

void TTmrSwWifi::SHandleDeviceApi(void)
 {
  SSendJson( TmrSwStatusApi.DeviceJson() );
 }

void TTmrSwWifi::SHandleStatusApi(void)
 {
  SSendJson( TmrSwStatusApi.StatusJson() );
 }

void TTmrSwWifi::SHandlePowerCalibrationApi(void)
 {
  String Json = F( "{\"VoltageCal\":" );
  Json += TmrSwA8TPowerMeter.VoltageCal();
  Json += F( ",\"CurrentCal\":" );
  Json += TmrSwA8TPowerMeter.CurrentCal();
  Json += F( ",\"PowerCal\":" );
  Json += TmrSwA8TPowerMeter.PowerCal();
  Json += '}';
  HttpServer.send( 200, "application/json", Json );
 }

void TTmrSwWifi::SHandleSetPowerCalibrationApi(void)
 {
  if( !SChkPost( "PowerCalibration" ) ) return;

  JsonDocument Document;
  if( deserializeJson( Document, HttpServer.arg( "plain" ) ) )
   {
    HttpServer.send( 400, "text/plain", "Invalid calibration JSON" );
    return;
   }

  JsonVariantConst Voltage = Document["VoltageCal"];
  JsonVariantConst Current = Document["CurrentCal"];
  JsonVariantConst Power = Document["PowerCal"];
  if( !Voltage.is<uint32_t>() || !Current.is<uint32_t>() || !Power.is<uint32_t>() )
   {
    HttpServer.send( 400, "text/plain", "Invalid calibration values" );
    return;
   }

  SSendPostResponce( TmrSwA8TPowerMeter.SetCalValues( Voltage.as<uint32_t>(),
    Current.as<uint32_t>(), Power.as<uint32_t>() ), false );
 }


void TTmrSwWifi::SHandleShortButtonApi(void)
 {
  HttpServer.sendHeader( "Access-Control-Allow-Origin", "*" );
  HttpServer.sendHeader( "Cache-Control", "no-store" );
  const bool Ok = TmrSwHw.SimulateShortButtonPress();
  HttpServer.send( Ok ? 200 : 409, "application/json", Ok ? "{\"Ok\":true}" : "{\"Ok\":false}" );
 }

void TTmrSwWifi::SHandleCalibrationOverrideApi(void)
 {
  if( !SChkPost( "CalibrationOverride" ) ) return;
  JsonDocument Document;
  if( deserializeJson( Document, HttpServer.arg( "plain" ) ) )
   {
    HttpServer.send( 400, "text/plain", "Invalid calibration override JSON" );
    return;
   }
  JsonVariantConst Active = Document["Active"];
  JsonVariantConst State = Document["State"];
  if( !Active.is<bool>() || !State.is<bool>() )
   {
    HttpServer.send( 400, "text/plain", "Calibration override requires boolean Active and State" );
    return;
   }
  const bool Ok = SwitchTimes.SetCalibrationOverride( Active.as<bool>(), State.as<bool>() );
  HttpServer.send( Ok ? 200 : 409, "text/plain", Ok ? "1" : "Calibration override could not be applied" );
 }

void TTmrSwWifi::SSendPostResponce( String const & Response, bool Reboot )
 {
  const bool Ok = Response == "1";
  if( Reboot && Ok )
    HttpServer.client().setNoDelay( true );
  HttpServer.send( 200, "text/plain", Response );
  if( !Reboot || !Ok )
    return;
  delay( 500 );
  HttpServer.client().stop();
  ESP.restart();
 }

void TTmrSwWifi::SSendPostResponce( bool Ok, bool Reboot )
 {
  SSendPostResponce( String( Ok ? "1" : "0" ), Reboot );
 }

bool TTmrSwWifi::SChkPost( String postName )
 {
  if( HttpServer.hasArg( "plain" ) ) return true;
  Log( "%s: no body\n", postName.c_str() );
  return false;
 }

void TTmrSwWifi::SHandleSetSwitchTimes( void )
 {
  if( !SChkPost( "SetSwitchTimes" ) ) return;
  //D Debug( "SwitchTimes: plain: %s\n", HttpServer.arg( "plain" ).c_str() );
  const String Result = SwitchTimes.Set( HttpServer.arg( "plain" ) );
  if( Result == "1" )
    GnTimeSync.LocalChanged();
  SSendPostResponce( Result, false );
 }

void TTmrSwWifi::SHandleTimesApi( void )
 {
  if( !SChkPost( "TimesApi" ) ) return;
  TGNString Response;
  const int Status = GnTimeSync.HandleIncoming( HttpServer.arg( "plain" ).c_str(), Response );
  HttpServer.send( Status, "application/json", Response.c_str() );
 }

void TTmrSwWifi::SHandleSetConfig( void )
 {
  if( !SChkPost( "Config" ) ) return;
  //D Debug( "Config: plain: %s\n", HttpServer.arg( "plain" ).c_str() );
  SSendPostResponce( TmrSwConfigInstance.Set( HttpServer.arg( "plain" ) ), true );
 }

void TTmrSwWifi::SHandleSetChannels( void )
 {
  if( !SChkPost( "Channels" ) ) return;
  const String Result = TmrSwChannels.Set( HttpServer.arg( "plain" ) );
  if( Result == "1" ) GnTimeSync.LocalChanged();
  SSendPostResponce( Result, true );
 }

void TTmrSwWifi::SHandleResetDevice( void )
 {
  SSendPostResponce( true, true );
 }

void TTmrSwWifi::SHandleUpdateRescueUpload(void)
 {
  HTTPUpload & Upload = TmrSwWifi.FHttpServer.upload();
  switch( Upload.status )
   {
    case UPLOAD_FILE_START:
      RescueUpload.Reset();
      break;
    case UPLOAD_FILE_WRITE:
      ProcessRescueUploadData( Upload.buf, Upload.currentSize );
      break;
    case UPLOAD_FILE_END:
      if( RescueUpload.Error.length() == 0 )
        VerifyWrittenRescue();
      break;
    case UPLOAD_FILE_ABORTED:
      SetRescueUploadError( F( "Rescue upload aborted" ) );
      break;
    default:
      break;
   }
 }

void TTmrSwWifi::SHandleUpdateRescue(void)
 {
  if( RescueUpload.Error.length() )
   {
    HttpServer.send( 400, "text/plain", RescueUpload.Error );
    return;
   }
  if( !RescueUpload.PrefixVerified ||
      RescueUpload.PayloadWritten != RescueUpload.Info.PayloadSize )
   {
    HttpServer.send( 400, "text/plain", "No complete Rescue artifact uploaded" );
    return;
   }
  HttpServer.send( 200, "text/plain", "Rescue updated and verified" );
 }

void TTmrSwWifi::SHandleStartRescue(void)
 {
  StoreRescueWlanPreference();
  if( !TmrSwWifi.SelectRescueBoot() )
   {
    HttpServer.send( 500, "text/plain", "0" );
    return;
   }
  HttpServer.client().setNoDelay( true );
  HttpServer.send( 200, "text/plain", "1" );
  delay( 500 );
  HttpServer.client().stop();
  ESP.restart();
 }

#if 0
void TTmrSwWifi::SHandleIndex( void )
 {
  HttpServer.send( 200, "text/html", ReadFile( F( "/Index.html" ) ) );
 }
#endif

bool TTmrSwWifi::SetupHttpServer( void )
 {
  if( ReadFile( F( "/Index.html" ) ).length() == 0 )
    return false;

  FHttpServer.onNotFound( SHandleNotFound );

  // Dashboard API
  FHttpServer.on( "/api/v1/device", HTTP_GET, SHandleDeviceApi );
  FHttpServer.on( "/api/v1/sources", HTTP_GET, []()
   {
    JsonDocument Result, Identity;
    deserializeJson( Identity, GnDeviceIdentity.ReportJson().c_str() );
    Result["Sources"].set( Identity["Sources"] );
    Result["Editable"] = !GnDashboardAvailable();
    TGNString Text; serializeJson( Result, Text );
    SSendJson( Text );
   } );
  FHttpServer.on( "/api/v1/sources", HTTP_POST, []()
   {
    if( !SChkPost( "Sources" ) ) return;
    const bool Locked = GnDashboardAvailable();
    const bool Ok = GnDeviceIdentity.SetSources( HttpServer.arg( "plain" ).c_str(), Locked );
    if( Ok ) GnTimeSync.RequestSync();
    HttpServer.send( Ok ? 200 : Locked ? 409 : 400, "application/json",
      Ok ? "{\"Ok\":true}" : Locked ? "{\"Ok\":false,\"Error\":\"UseDashboard\"}" : "{\"Ok\":false,\"Error\":\"InvalidSourcesOrStorage\"}" );
   } );
  FHttpServer.on( "/api/v1/sources", HTTP_OPTIONS, SHandleApiOptions );
  FHttpServer.on( "/api/v1/device", HTTP_OPTIONS, SHandleApiOptions );
  FHttpServer.on( "/api/v1/status", HTTP_GET, SHandleStatusApi );
  FHttpServer.on( "/api/v1/status", HTTP_OPTIONS, SHandleApiOptions );
  FHttpServer.on( "/api/v1/power/calibration", HTTP_GET, SHandlePowerCalibrationApi );
  FHttpServer.on( "/api/v1/power/calibration", HTTP_POST, SHandleSetPowerCalibrationApi );
  FHttpServer.on( "/api/v1/power/calibration", HTTP_OPTIONS, SHandleApiOptions );
  FHttpServer.on( "/api/v1/times", HTTP_POST, SHandleTimesApi );
  FHttpServer.on( "/api/v1/times", HTTP_OPTIONS, SHandleApiOptions );
  FHttpServer.on( "/api/v1/button/0/short", HTTP_POST, SHandleShortButtonApi );
  FHttpServer.on( "/api/v1/button/0/short", HTTP_OPTIONS, SHandleApiOptions );
  FHttpServer.on( "/api/v1/calibration/override", HTTP_POST, SHandleCalibrationOverrideApi );
  FHttpServer.on( "/api/v1/calibration/override", HTTP_OPTIONS, SHandleApiOptions );

  // Setup a serveStatic for / as alias for Index.html
  FHttpServer.serveStatic( "/", LittleFS, "/Index.html" );
  // Setup a serveStatic for every file inside the dir '/'
  File Root = LittleFS.open( "/" );
  for( File Entry = Root.openNextFile(); Entry; Entry = Root.openNextFile() )
    if( !Entry.isDirectory() )
     {
      String fname = Entry.name();
      if( fname.charAt( 0 ) != '/' )
        fname = "/" + fname;
      FHttpServer.serveStatic( fname.c_str(), LittleFS, fname.c_str() );
     }

  // Setup a server.on for dynamic generated answers
  FHttpServer.on( "/SetSwitchTimes/", HTTP_POST, SHandleSetSwitchTimes );
  FHttpServer.on( "/SetConfig/", HTTP_POST, SHandleSetConfig );
  FHttpServer.on( "/SetChannels/", HTTP_POST, SHandleSetChannels );
  FHttpServer.on( "/ResetDevice/", HTTP_POST, SHandleResetDevice );
  FHttpServer.on( "/StartRescue/", HTTP_POST, SHandleStartRescue );
  FHttpServer.on( "/UpdateRescue/", HTTP_POST, SHandleUpdateRescue, SHandleUpdateRescueUpload );

  // For getting accept-language (only when language select by this...
  //FHttpServer.collectHeaders( "accept-language" );

  FHttpServer.begin();

  MDNS.begin( TmrSwConfig.Hostname.c_str() );
  MDNS.addService( "http", "tcp", 80 );

  return true;
 }
#undef HttpServer
