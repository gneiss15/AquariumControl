// FeederWifi.cpp, Version: 1.22
//****************************************************************
// Includes
//****************************************************************

#include "FeederWifi.h"
#include "HwConfig.FeederSimple.h"

#include "FeedTimes.h"
#include <GnTimeSync.h>
#include "FeederDrive.h"
#include "FeederConfig.h"
#include "FeederStatusApi.h"
#include "FeederTemperature.h"
#include "Libs/Common/Feeder/OneWireDiagnosticPage.h"

#ifdef ESP32
  #include <WiFi.h>
 #else
  #include <ESP8266WiFi.h>
  #include <ESP8266mDNS.h>
  #include <coredecls.h>                  // settimeofday_cb()
  #include "EspUpdateSigning.h"
#endif
#include <LittleFS.h>

//****************************************************************
// Sw Configuration
//****************************************************************

//****************************************************************
// Vars
//****************************************************************


#if defined( FeederSimpleDallasTest )
namespace
 {

constexpr uint16_t DallasConversionMs = 1000;

const char DallasTestPage[] PROGMEM = R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FeederSimple Dallas Test</title>
<style>
body{font-family:sans-serif;margin:1rem;max-width:52rem}table{border-collapse:collapse;margin:.4rem 0 1rem}th,td{padding:.35rem .7rem;text-align:left;border-bottom:1px solid #ccc}button{margin:.15rem;padding:.45rem .8rem}pre{white-space:pre-wrap;background:#f3f3f3;padding:.7rem}.Mono{font-family:monospace}
</style>
</head>
<body>
<h1>FeederSimple Dallas / OneWire Test</h1>
<p><a href="/OneWireDiag/">Produktivzyklus-Log</a></p>
<table>
<tr><th>DallasPresent</th><td id="Present">--</td></tr>
<tr><th>Pin</th><td id="Pin">--</td></tr>
<tr><th>Bus idle</th><td id="Idle">--</td></tr>
</table>
<p>
<button id="Run20" onclick="StartTest(20)">20 Tests starten</button>
<button id="RunContinuous" onclick="StartTest(0)">Dauertest starten</button>
<button id="Stop" onclick="StopTest()" disabled>Stop</button>
</p>
<pre id="Result">Noch kein Test.</pre>
<script>
let StopRequested=false;
let Running=false;

function YesNo(Value){return Value?'ja':'nein';}
function HighLow(Value){return Value?'HIGH':'LOW';}
function Sleep(Milliseconds){return new Promise(Resolve=>setTimeout(Resolve,Milliseconds));}

async function FetchJson(Url,Options,TimeoutMs){
 const Controller=new AbortController();
 const Timer=setTimeout(()=>Controller.abort(),TimeoutMs);
 try{
  Options=Options||{};
  Options.signal=Controller.signal;
  const Response=await fetch(Url,Options);
  if(!Response.ok)throw new Error('HTTP '+Response.status);
  return await Response.json();
 }catch(Error){
  if(Error.name==='AbortError')throw new Error('Timeout > '+TimeoutMs+' ms');
  throw Error;
 }finally{clearTimeout(Timer);}
}

async function Status(){
 try{
  const Data=await FetchJson('/DallasTestStatus/?t='+Date.now(),{cache:'no-store'},5000);
  document.getElementById('Present').textContent=YesNo(!!Data.Present);
  document.getElementById('Pin').textContent='GPIO'+Data.Pin+(Data.PinConflict?' (PIN-KONFLIKT)':'');
  document.getElementById('Idle').textContent=Data.PinValid?HighLow(!!Data.IdleHigh):'ungültiger Pin';
 }catch(Error){document.getElementById('Result').textContent='Statusfehler: '+Error.message;}
}

function SetButtons(IsRunning){
 document.getElementById('Run20').disabled=IsRunning;
 document.getElementById('RunContinuous').disabled=IsRunning;
 document.getElementById('Stop').disabled=!IsRunning;
}

function NewState(){
 return {
  Attempts:0,Errors:0,Presence:0,Search:0,Read:0,Agree:0,SkipTemp:0,MatchTemp:0,
  SearchRom:'',ReadRom:'',LastSkipTemp:null,LastMatchTemp:null,History:[]
 };
}

function Render(State,Mode,FinishedText){
 const Box=document.getElementById('Result');
 let Text=(Mode==='continuous'?'Dauertest':'20er-Test')+', Versuche: '+State.Attempts+'\n'
  +'Fehler/Timeouts:            '+State.Errors+'/'+State.Attempts+'\n'
  +'Presence:                   '+State.Presence+'/'+State.Attempts+'\n'
  +'SEARCH ROM + CRC:           '+State.Search+'/'+State.Attempts+'\n'
  +'READ ROM + CRC:             '+State.Read+'/'+State.Attempts+'\n'
  +'SEARCH/READ identisch:      '+State.Agree+'/'+State.Attempts+'\n'
  +'SKIP ROM + Temperatur:      '+State.SkipTemp+'/'+State.Attempts+'\n'
  +'MATCH ROM + Temperatur:     '+State.MatchTemp+'/'+State.Attempts
  +(State.SearchRom?'\nSEARCH ROM: '+State.SearchRom:'')
  +(State.ReadRom?'\nREAD ROM:   '+State.ReadRom:'')
  +(State.LastSkipTemp!==null?'\nLetzte SKIP-Temperatur:  '+State.LastSkipTemp.toFixed(2)+' °C':'')
  +(State.LastMatchTemp!==null?'\nLetzte MATCH-Temperatur: '+State.LastMatchTemp.toFixed(2)+' °C':'')
  +'\n\nLetzte Einzelversuche (max. 100):\n'+State.History.join('\n');
 if(FinishedText)Text+='\n\n'+FinishedText;
 Box.textContent=Text;
}

function StopTest(){
 StopRequested=true;
}

async function StartTest(MaxAttempts){
 if(Running)return;
 Running=true;
 StopRequested=false;
 SetButtons(true);
 const Mode=MaxAttempts===0?'continuous':'fixed';
 const State=NewState();

 while(!StopRequested && (MaxAttempts===0 || State.Attempts<MaxAttempts)){
  State.Attempts++;
  const Index=State.Attempts;
  const BrowserStart=performance.now();
  try{
   const Data=await FetchJson('/DallasTestAttempt/?t='+Date.now(),{method:'POST',cache:'no-store'},5000);
   const BrowserMs=Math.round(performance.now()-BrowserStart);
   if(Data.Presence)State.Presence++;
   if(Data.SearchRomOk){State.Search++;if(Data.SearchRom)State.SearchRom=Data.SearchRom;}
   if(Data.ReadRomOk){State.Read++;if(Data.ReadRom)State.ReadRom=Data.ReadRom;}
   if(Data.RomAgree)State.Agree++;
   if(Data.SkipTemperatureOk){State.SkipTemp++;State.LastSkipTemp=Data.SkipTemperature;}
   if(Data.MatchTemperatureOk){State.MatchTemp++;State.LastMatchTemp=Data.MatchTemperature;}
   let Temp='--';
   if(Data.MatchTemperatureOk)Temp=Data.MatchTemperature.toFixed(2)+' °C';
   else if(Data.SkipTemperatureOk)Temp=Data.SkipTemperature.toFixed(2)+' °C';
   State.History.push(String(Index).padStart(5,'0')+': Browser='+BrowserMs+' ms, Server='+Data.ServerMs+' ms, RSSI='+Data.Rssi+' dBm, Heap='+Data.FreeHeap+' B, Temp='+Temp);
  }catch(Error){
   const BrowserMs=Math.round(performance.now()-BrowserStart);
   State.Errors++;
   State.History.push(String(Index).padStart(5,'0')+': FEHLER nach '+BrowserMs+' ms: '+Error.message);
  }
  if(State.History.length>100)State.History.shift();
  Render(State,Mode,'');
  if(!StopRequested && (MaxAttempts===0 || State.Attempts<MaxAttempts))
    await Sleep(200);
 }

 const FinishedText=StopRequested?'Abgebrochen.':'Fertig.';
 Render(State,Mode,FinishedText);
 Running=false;
 SetButtons(false);
 Status();
}

Status();
</script>
</body>
</html>)HTML";

template <typename TString>
void AppendDallasRom( TString & Json, uint8_t const * Address )
 {
  static const char Hex[] = "0123456789ABCDEF";
  Json += '"';
  for( uint8_t Index = 0; Index < 8; ++Index )
   {
    if( Index != 0 )
      Json += '-';
    Json += Hex[( Address[Index] >> 4 ) & 0x0F];
    Json += Hex[Address[Index] & 0x0F];
   }
  Json += '"';
 }

template <typename TString>
void AppendInteger( TString & Output, long long Value )
 {
  char Text[32];
  snprintf( Text, sizeof( Text ), "%lld", Value );
  Output += Text;
 }

template <typename TString>
void AppendFloat2( TString & Output, float Value )
 {
  char Text[32];
  dtostrf( Value, 0, 2, Text );
  Output += Text;
 }

bool DallasRomValid( uint8_t const * Address )
 {
  return OneWire::crc8( Address, 7 ) == Address[7];
 }

bool DallasScratchpadValid( uint8_t const * Scratchpad )
 {
  return OneWire::crc8( Scratchpad, 8 ) == Scratchpad[8];
 }

bool DallasTemperature( uint8_t Family, uint8_t const * Scratchpad, float & Temperature )
 {
  const int16_t Raw = static_cast<int16_t>( static_cast<uint16_t>( Scratchpad[0] )
                      | ( static_cast<uint16_t>( Scratchpad[1] ) << 8 ) );
  if( Family == 0x10 )
    Temperature = static_cast<float>( Raw ) / 2.0F;
   else
    Temperature = static_cast<float>( Raw ) / 16.0F;
  return Temperature >= -55.0F && Temperature <= 125.0F;
 }

bool DallasReadScratchpadSkip( OneWire & Bus, uint8_t * Scratchpad )
 {
  if( Bus.reset() == 0 )
    return false;
  Bus.skip();
  Bus.write( 0xBE );
  for( uint8_t Index = 0; Index < 9; ++Index )
    Scratchpad[Index] = Bus.read();
  return DallasScratchpadValid( Scratchpad );
 }

bool DallasReadScratchpadMatch( OneWire & Bus, uint8_t const * Address, uint8_t * Scratchpad )
 {
  if( Bus.reset() == 0 )
    return false;
  Bus.select( Address );
  Bus.write( 0xBE );
  for( uint8_t Index = 0; Index < 9; ++Index )
    Scratchpad[Index] = Bus.read();
  return DallasScratchpadValid( Scratchpad );
 }

TGNString DallasTestStatusJson(void)
 {
  const bool PinValid = FeederConfig.DallasPin >= 0 && FeederConfig.DallasPin <= 15;
  const bool PinConflict = PinValid && FeederDrive.PinUsed( FeederConfig.DallasPin );
  const bool IdleHigh = PinValid && digitalRead( FeederConfig.DallasPin ) == HIGH;
  TGNString Json;
  const auto Build = [&]( auto & Output )
   {
    Output += "{\"Present\":";
    Output += FeederConfig.DallasPresent ? "true" : "false";
    Output += ",\"Pin\":";
    AppendInteger( Output, FeederConfig.DallasPin );
    Output += ",\"PinValid\":";
    Output += PinValid ? "true" : "false";
    Output += ",\"PinConflict\":";
    Output += PinConflict ? "true" : "false";
    Output += ",\"IdleHigh\":";
    Output += IdleHigh ? "true" : "false";
    Output += '}';
   };
  if( !GnBuildStringExact( Json, Build ) )
    return TGNString();
  return Json;
 }

TGNString DallasTestAttemptJson(void)
 {
  static constexpr TGnCoopTimer_t DiagnosticPauseMilliseconds = 3000;
  FeederTemperature.PauseForDiagnostic( DiagnosticPauseMilliseconds );
  TGnElapsedTimerMs ServerTimer;
  const bool PinValid = FeederConfig.DallasPin >= 0 && FeederConfig.DallasPin <= 15;
  if( !PinValid || FeederDrive.PinUsed( FeederConfig.DallasPin ) )
    return "{\"Presence\":false,\"SearchRomOk\":false,\"ReadRomOk\":false,\"RomAgree\":false,\"SkipTemperatureOk\":false,\"MatchTemperatureOk\":false}";

  const uint8_t Pin = static_cast<uint8_t>( FeederConfig.DallasPin );
  pinMode( Pin, INPUT );
  delay( 2 );
  OneWire Bus( Pin );

  const bool Presence = Bus.reset() != 0;

  uint8_t SearchAddress[8] = {};
  Bus.reset_search();
  const bool SearchRomOk = Bus.search( SearchAddress ) && DallasRomValid( SearchAddress );

  uint8_t ReadAddress[8] = {};
  bool ReadRomOk = false;
  if( Bus.reset() != 0 )
   {
    Bus.write( 0x33 );
    for( uint8_t Index = 0; Index < sizeof( ReadAddress ); ++Index )
      ReadAddress[Index] = Bus.read();
    ReadRomOk = DallasRomValid( ReadAddress );
   }

  bool RomAgree = SearchRomOk && ReadRomOk;
  if( RomAgree )
   {
    for( uint8_t Index = 0; Index < sizeof( SearchAddress ); ++Index )
      if( SearchAddress[Index] != ReadAddress[Index] )
       {
        RomAgree = false;
        break;
       }
   }

  bool ConversionStarted = false;
  if( Bus.reset() != 0 )
   {
    Bus.skip();
    Bus.write( 0x44 );
    ConversionStarted = true;
    delay( DallasConversionMs );
    Bus.depower();
   }

  uint8_t SkipScratchpad[9] = {};
  float SkipTemperature = 0.0F;
  bool SkipTemperatureOk = false;
  if( ConversionStarted && DallasReadScratchpadSkip( Bus, SkipScratchpad ) )
   {
    const uint8_t Family = ReadRomOk ? ReadAddress[0] : ( SearchRomOk ? SearchAddress[0] : 0x28 );
    SkipTemperatureOk = DallasTemperature( Family, SkipScratchpad, SkipTemperature );
   }

  uint8_t const * MatchAddress = ReadRomOk ? ReadAddress : ( SearchRomOk ? SearchAddress : nullptr );
  uint8_t MatchScratchpad[9] = {};
  float MatchTemperature = 0.0F;
  bool MatchTemperatureOk = false;
  if( ConversionStarted && MatchAddress != nullptr
      && DallasReadScratchpadMatch( Bus, MatchAddress, MatchScratchpad ) )
    MatchTemperatureOk = DallasTemperature( MatchAddress[0], MatchScratchpad, MatchTemperature );

  pinMode( Pin, INPUT );

  const TGnCoopTimer_t ServerMs = ServerTimer.Elapsed();
  const long Rssi = WiFi.RSSI();
  const uint32_t FreeHeap = ESP.getFreeHeap();
  TGNString Json;
  const auto Build = [&]( auto & Json )
   {
    Json += "{\"Presence\":";
  Json += Presence ? "true" : "false";
  Json += ",\"SearchRomOk\":";
  Json += SearchRomOk ? "true" : "false";
  Json += ",\"ReadRomOk\":";
  Json += ReadRomOk ? "true" : "false";
  Json += ",\"RomAgree\":";
  Json += RomAgree ? "true" : "false";
  Json += ",\"SkipTemperatureOk\":";
  Json += SkipTemperatureOk ? "true" : "false";
  Json += ",\"MatchTemperatureOk\":";
  Json += MatchTemperatureOk ? "true" : "false";
  Json += ",\"SearchRom\":";
  if( SearchRomOk )
    AppendDallasRom( Json, SearchAddress );
   else
    Json += "null";
  Json += ",\"ReadRom\":";
  if( ReadRomOk )
    AppendDallasRom( Json, ReadAddress );
   else
    Json += "null";
  Json += ",\"SkipTemperature\":";
  if( SkipTemperatureOk )
    AppendFloat2( Json, SkipTemperature );
   else
    Json += "null";
  Json += ",\"MatchTemperature\":";
  if( MatchTemperatureOk )
    AppendFloat2( Json, MatchTemperature );
   else
    Json += "null";
    Json += ",\"ServerMs\":";
    AppendInteger( Json, ServerMs );
    Json += ",\"Rssi\":";
    AppendInteger( Json, Rssi );
    Json += ",\"FreeHeap\":";
    AppendInteger( Json, FreeHeap );
    Json += '}';
   };
  if( !GnBuildStringExact( Json, Build ) )
    return TGNString();
  return Json;
 }

} // namespace
#endif


void TFeederWifi::SRecoveryBlinkTimerCb( TGnCoopTimerBase *, uint16_t )
 {
  static bool RecoveryLedOn = false;
  RecoveryLedOn = !RecoveryLedOn;
  digitalWrite( StatusLedPin, RecoveryLedOn == bool( StatusLedActiveHigh ) ? HIGH : LOW );
 }

//****************************************************************
// Functions
//****************************************************************

//****************************************************************
// TFeederWifi
//****************************************************************

TFeederWifi::TFeederWifi(void)
: FSsidNr( 0 )
, F500msConnectTicker( 0 )
, FWifiCheckTimer( Closure( this, &TFeederWifi::WifiCheckTimerCb ), 500, false )
, FRecoveryBlinkTimer( SRecoveryBlinkTimerCb, 150, false )
, FStatusTick( 0 )
, FRecoveryMode( false )
, FRecoveryFallbackApStarted( false )
, FStartupErrorMode( false )
, FTemporaryAp( false )
, FHttpServer( 80 )
 {
 }

/*virtual*/ void TFeederWifi::Loop(void)
 {
  FHttpServer.handleClient();
  if( FRecoveryMode )
    return;

  #if !defined( ESP32 )
    MDNS.update();
  #endif

 }

bool TFeederWifi::Setup(void) 
 { 
  FStartupErrorMode = false;
  return SetupNetwork() && SetupWeb();
 }

bool TFeederWifi::SetupNetwork(void)
 {
  return SetupWifi() && SetupNTP();
 }

bool TFeederWifi::SetupWeb(void)
 {
  return SetupHttpServer();
 }

#if !defined( ESP32 )
bool TFeederWifi::SetupRecovery( char const * ProjectName )
 {
  FRecoveryMode = true;
  FRecoveryFallbackApStarted = true;
  FStartupErrorMode = false;
  digitalWrite( StatusLedPin, StatusLedActiveHigh ? LOW : HIGH );
  pinMode( StatusLedPin, OUTPUT );
  FRecoveryBlinkTimer.Start();

  WiFi.disconnect( true );
  delay( 100 );
  WiFi.mode( WIFI_AP );
  WiFi.setAutoConnect( false );

  String ApSsid( ProjectName );
  ApSsid += F( "-Rescue" );
  if( !WiFi.softAP( ApSsid.c_str() ) )
    return false;

  FWebUpdate.SetupRecovery( FHttpServer,
    &EspUpdateSigning::Hash, &EspUpdateSigning::Verifier, ProjectName );
  FHttpServer.begin();
  return true;
 }

bool TFeederWifi::SetupRecoveryNetwork( char const * ProjectName )
 {
  FRecoveryBlinkTimer.Stop();
  FRecoveryMode = false;
  FRecoveryFallbackApStarted = false;
  FStartupErrorMode = true;
  if( !SetupWifi() )
    return false;

  FRecoveryFallbackApStarted = ( FeederConfig.WifiMode & WIFI_AP ) != 0;
  if( FeederConfig.WifiMode == WIFI_OFF && !StartRecoveryFallbackAp() )
    return false;

  FHttpServer.onNotFound( SHandleNotFound );
  FWebUpdate.SetupRecovery( FHttpServer,
    &EspUpdateSigning::Hash, &EspUpdateSigning::Verifier, ProjectName );
  FHttpServer.begin();

  MDNS.begin( FeederConfig.Hostname );
  MDNS.addService( "http", "tcp", 80 );
  return true;
 }

bool TFeederWifi::SetupStartupError( char const * ProjectName, String const & Error )
 {
  FRecoveryBlinkTimer.Stop();
  FRecoveryMode = false;
  FRecoveryFallbackApStarted = false;
  FStartupErrorMode = true;
  if( !SetupWifi() )
    return false;

  FRecoveryFallbackApStarted = ( FeederConfig.WifiMode & WIFI_AP ) != 0;
  if( FeederConfig.WifiMode == WIFI_OFF && !StartRecoveryFallbackAp() )
    return false;

  FHttpServer.onNotFound( SHandleNotFound );
  FWebUpdate.SetupStartupError( FHttpServer,
    &EspUpdateSigning::Hash, &EspUpdateSigning::Verifier, ProjectName, Error );
  FHttpServer.begin();

  MDNS.begin( FeederConfig.Hostname );
  MDNS.addService( "http", "tcp", 80 );
  return true;
 }
#endif

//****************************************************************
// TFeederWifi (Wifi)
//****************************************************************

void TFeederWifi::StartSTA( String const & Ssid, String const & Password )
 {
  WiFi.begin( Ssid, Password );
  Debug( "StartSTA: Connecting to WiFi, SSID: %s\n", Ssid.c_str() );
  F500msConnectTicker = 0;
 }

bool TFeederWifi::StartRecoveryFallbackAp(void)
 {
  if( FRecoveryFallbackApStarted )
    return true;

  const WiFiMode_t Mode = WiFi.getMode();
  if( Mode == WIFI_STA )
    WiFi.mode( WIFI_AP_STA );
   else if( Mode == WIFI_OFF )
    WiFi.mode( WIFI_AP );

  if( !WiFi.softAP( FeederConfig.ApSsid, FeederConfig.ApPw ) )
    return false;
  FRecoveryFallbackApStarted = true;
  return true;
 }

void TFeederWifi::WifiCheckTimerCb( TGnCoopTimerBase *, uint16_t )
 {
  if( !FRecoveryMode )
    CheckWifiConnection();
 }

void TFeederWifi::CheckWifiConnection(void)
 {
  const bool StationRequired = FTemporaryAp || ( FeederConfig.WifiMode & WIFI_STA ) != 0;
  const bool StationConnected = !StationRequired || WiFi.status() == WL_CONNECTED;
  FeederDrive.SetNetworkStatus( StationConnected, TimeIsValid(), FStatusTick++ );

  if( !StationRequired )
    return;
  if( StationConnected )
   {
    if( FTemporaryAp )
     {
      // Only the explicitly requested rescue AP is temporary.
      WiFi.softAPdisconnect( true );
      WiFi.mode( FeederConfig.WifiMode );
      FTemporaryAp = false;
     }
    F500msConnectTicker = 0;
    return;
   }
  if( ++F500msConnectTicker <= 20 )
    return;

  if( FStartupErrorMode && !FRecoveryFallbackApStarted )
    StartRecoveryFallbackAp();

  if( FSsidNr == 1 && FeederConfig.WlanSsid2.length() > 0 )
    StartSTA( FeederConfig.WlanSsid2, FeederConfig.WlanPw2 );
   else
    StartSTA( FeederConfig.WlanSsid, FeederConfig.WlanPw );
  FSsidNr = 1 - FSsidNr;
 }

void TFeederWifi::Reconnect(void)
 {
  if( !FTemporaryAp && !( FeederConfig.WifiMode & WIFI_STA ) )
    return;

  WiFi.disconnect();
  FSsidNr = FeederConfig.WlanSsid2.length() > 0 ? 1 : 0;
  StartSTA( FeederConfig.WlanSsid, FeederConfig.WlanPw );
 }

bool TFeederWifi::StartTemporaryAp(void)
 {
  // Keep the saved mode unchanged; restart STA attempts alongside the rescue AP.
  if( !WiFi.mode( WIFI_AP_STA ) )
    return false;
  if( !WiFi.softAP( FeederConfig.ApSsid, FeederConfig.ApPw ) )
   {
    WiFi.mode( FeederConfig.WifiMode );
    return false;
   }
  FTemporaryAp = true;
  SetupNTP();
  Reconnect();
  return true;
 }

bool TFeederWifi::SetupWifi(void)
 {
  // Set WiFi to configured mode and disconnect from an AP if it was previously connected
  WiFi.disconnect( true );
  delay( 100 );
  WiFi.mode( FeederConfig.WifiMode );

  if( FeederConfig.WifiMode & WIFI_STA )
   {
    WiFi.setAutoConnect( true );
    WiFi.hostname( FeederConfig.Hostname );
    FSsidNr = FeederConfig.WlanSsid2.length() > 0 ? 1 : 0;
    StartSTA( FeederConfig.WlanSsid, FeederConfig.WlanPw );
   }
   else
    WiFi.setAutoConnect( false );

  if( FeederConfig.WifiMode & WIFI_AP )   
   {
    //!_WiFi.softAPConfig( /*ip*/IPAddress( 192,168,4,1 ), /*gateway*/IPAddress( 192,168,4,200 ), /*subnet*/IPAddress( 255,255,255,0 ) );
    /*bool apOk =*/ WiFi.softAP( FeederConfig.ApSsid, FeederConfig.ApPw );
    //Log( "Starting soft-AP (Pw): %s\n", apOk ? "Ready" : "Failed!" );
    //Log( "Soft-AP IP address: %s\n", WiFi.softAPIP().toString().c_str() );
   }

  FWifiCheckTimer.Start();
  return true;
 }

//****************************************************************
// TFeederWifi (NTP)
//****************************************************************

bool TFeederWifi::STimeIsValid = false;

void TFeederWifi::STimeIsSet_cb(void)
 {
  //D Debug( "TFeederWifi::STimeIsSet_cb\n" );
  if( TFeederWifi::STimeIsValid )
    return;
  TFeederWifi::STimeIsValid = true;
  FeederDrive.TimeSynchronized();
  //settimeofday_cb( NULL );
 }

bool TFeederWifi::SetupNTP(void)
 {
  if( FTemporaryAp || ( FeederConfig.WifiMode & WIFI_STA ) )
   {
    // Setp Ntp
    // install callback - called when settimeofday is called (by SNTP or us)
    // once enabled (by DHCP), SNTP is updated every hour
    settimeofday_cb( STimeIsSet_cb );
    configTime( FeederConfig.TZ.c_str(), FeederConfig.NtpServerAdr.c_str() );
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
uint32_t sntp_update_delay_MS_rfc_not_less_than_15000(void)
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
void TFeederWifi::SHandle....(void)
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
// TFeederWifi (HttpServer & FwUpdater)
//****************************************************************

#define HttpServer FeederWifi.FHttpServer

void TFeederWifi::SHandleNotFound(void)
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

void TFeederWifi::SSendJson( TGNString const & Json )
 {
  HttpServer.sendHeader( "Access-Control-Allow-Origin", "*" );
  HttpServer.sendHeader( "Cache-Control", "no-store" );
  HttpServer.send( 200, "application/json", Json.c_str() );
 }

void TFeederWifi::SHandleApiOptions(void)
 {
  HttpServer.sendHeader( "Access-Control-Allow-Origin", "*" );
  HttpServer.sendHeader( "Access-Control-Allow-Methods", "GET, OPTIONS" );
  HttpServer.sendHeader( "Access-Control-Allow-Headers", "Content-Type" );
  HttpServer.send( 204, "text/plain", "" );
 }

void TFeederWifi::SHandleDeviceApi(void)
 {
  SSendJson( FeederStatusApi.DeviceJson() );
 }

void TFeederWifi::SHandleStatusApi(void)
 {
  SSendJson( FeederStatusApi.StatusJson() );
 }


void TFeederWifi::SHandleOneWireDiag(void)
 {
  HttpServer.sendHeader( "Cache-Control", "no-store" );
  HttpServer.send_P( 200, "text/html; charset=UTF-8", OneWireDiagnosticPage );
 }

void TFeederWifi::SHandleOneWireDiagStats(void)
 {
  SSendJson( FeederTemperature.DiagnosticStatsJson() );
 }

void TFeederWifi::SHandleOneWireDiagLog(void)
 {
  HttpServer.client().setNoDelay( true );
  HttpServer.sendHeader( "Cache-Control", "no-store" );
  HttpServer.sendHeader( "Content-Disposition", "inline; filename=OneWireDiag.csv" );
  HttpServer.setContentLength( CONTENT_LENGTH_UNKNOWN );
  HttpServer.send( 200, "text/csv; charset=UTF-8", "" );
  HttpServer.sendContent( "Time,UnixTime,UptimeMs,Poll,State,Reason\n" );

  TFeederTemperature::TDiagnosticLogEntry Entries[16];
  size_t StartEntry = 0;
  bool OutputOk = true;
  while( OutputOk && HttpServer.client().connected() )
   {
    const size_t Count = FeederTemperature.ReadDiagnosticLog( StartEntry, Entries, TableSize( Entries ) );
    if( Count == 0 )
      break;
    for( size_t Index = 0; Index < Count; ++Index )
     {
      const TFeederTemperature::TDiagnosticLogEntry & Entry = Entries[Index];
      char TimeText[24] = {};
      if( Entry.UnixTime != 0 )
       {
        const time_t UnixTime = static_cast<time_t>( Entry.UnixTime );
        struct tm Time = {};
        gmtime_r( &UnixTime, &Time );
        strftime( TimeText, sizeof( TimeText ), "%Y-%m-%dT%H:%M:%SZ", &Time );
       }
      char Line[160];
      const int Length = snprintf( Line, sizeof( Line ), "%s,%lu,%llu,%lu,%s,%s\n",
        TimeText,
        static_cast<unsigned long>( Entry.UnixTime ),
        static_cast<unsigned long long>( Entry.UptimeMilliseconds ),
        static_cast<unsigned long>( Entry.Poll ),
        TFeederTemperature::DiagnosticStateName( Entry.State ),
        TFeederTemperature::DiagnosticReasonName( Entry.Reason ) );
      if( Length <= 0 || static_cast<size_t>( Length ) >= sizeof( Line ) )
       {
        OutputOk = false;
        break;
       }
      String CsvLine;
      if( !CsvLine.reserve( static_cast<unsigned int>( Length ) )
          || !CsvLine.concat( Line, static_cast<unsigned int>( Length ) ) )
       {
        OutputOk = false;
        break;
       }
      HttpServer.sendContent( CsvLine );
     }
    StartEntry += Count;
    delay( 0 );
   }
  HttpServer.sendContent( "" );
 }

void TFeederWifi::SHandleOneWireDiagClear(void)
 {
  SSendPostResponce( FeederTemperature.ClearDiagnostics(), false );
 }



void TFeederWifi::SSendPostResponce( String const & Response, bool Reboot )
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

void TFeederWifi::SSendPostResponce( bool Ok, bool Reboot )
 {
  SSendPostResponce( String( Ok ? "1" : "0" ), Reboot );
 }

bool TFeederWifi::SChkPost( String postName )
 {
  if( HttpServer.hasArg( "plain" ) ) return true;
  Log( "%s: no body\n", postName.c_str() );
  return false;
 }

void TFeederWifi::SHandleSetFeedTimes(void)
 {
  if( !SChkPost( "SetFeedTimes" ) ) return;
  //D Debug( "FeedTimes: plain: %s\n", HttpServer.arg( "plain" ).c_str() );
  const String Result = FeedTimes.Set( HttpServer.arg( "plain" ) );
  if( Result == "1" )
    GnTimeSync.LocalChanged();
  SSendPostResponce( Result, false );
 }

void TFeederWifi::SHandleTimesApi(void)
 {
  if( !SChkPost( "TimesApi" ) ) return;
  TGNString Response;
  const int Status = GnTimeSync.HandleIncoming( HttpServer.arg( "plain" ).c_str(), Response );
  HttpServer.send( Status, "application/json", Response.c_str() );
 }

void TFeederWifi::SHandleManualFeed(void)
 {
  if( !SChkPost( "ManualFeed" ) ) return;
  //D Debug( "ManualFeed\n" );
  SSendPostResponce( FeederDrive.Feed(), false );
 }

void TFeederWifi::SHandleStopFeed(void)
 {
  if( !SChkPost( "StopFeed" ) ) return;
  //D Debug( "StopFeed\n" );
  SSendPostResponce( FeederDrive.Stop(), false );
 }

void TFeederWifi::SHandleMFeedChk(void)
 {
  if( !SChkPost( "MFeedChk" ) ) return;
  //D Debug( "MFeedChk\n" );
  SSendPostResponce( !FeederDrive.Running(), false );
 }

void TFeederWifi::SHandleSetConfig(void)
 {
  if( !SChkPost( "Config" ) ) return;
  //D Debug( "Config: plain: %s\n", HttpServer.arg( "plain" ).c_str() );
  SSendPostResponce( FeederConfigInstance.Set( HttpServer.arg( "plain" ) ), true );
 }

void TFeederWifi::SHandleSetServoSteps(void)
 {
  if( !SChkPost( "SetServoSteps" ) ) return;
  //D Debug( "SetServoSteps: plain: %s\n", HttpServer.arg( "plain" ).c_str() );
  SSendPostResponce( FeederDrive.SetSettings( HttpServer.arg( "plain" ) ), false );
 }

void TFeederWifi::SHandleResetDevice(void)
 {
  SSendPostResponce( true, true );
 }

#if 0
void TFeederWifi::SHandleIndex(void)
 {
  HttpServer.send( 200, "text/html", ReadFile( F( "/Index.html" ) ) );
 }
#endif
 
bool TFeederWifi::SetupHttpServer(void)
 {
  if( ReadFile( F( "/Index.html" ) ).length() == 0 )
    return false;

  FHttpServer.onNotFound( SHandleNotFound );

  // Dashboard API
  FHttpServer.on( "/api/v1/device", HTTP_GET, SHandleDeviceApi );
  FHttpServer.on( "/api/v1/device", HTTP_OPTIONS, SHandleApiOptions );
  FHttpServer.on( "/api/v1/status", HTTP_GET, SHandleStatusApi );
  FHttpServer.on( "/api/v1/status", HTTP_OPTIONS, SHandleApiOptions );
  FHttpServer.on( "/api/v1/times", HTTP_POST, SHandleTimesApi );
  FHttpServer.on( "/api/v1/times", HTTP_OPTIONS, SHandleApiOptions );

  FHttpServer.on( "/OneWireDiag", HTTP_GET, SHandleOneWireDiag );
  FHttpServer.on( "/OneWireDiag/", HTTP_GET, SHandleOneWireDiag );
  FHttpServer.on( "/OneWireDiagStats/", HTTP_GET, SHandleOneWireDiagStats );
  FHttpServer.on( "/OneWireDiagLog.csv", HTTP_GET, SHandleOneWireDiagLog );
  FHttpServer.on( "/OneWireDiagClear/", HTTP_POST, SHandleOneWireDiagClear );

#if defined( FeederSimpleDallasTest )
  FHttpServer.on( "/DallasTestStatus/", HTTP_GET,
    [this]()
     {
      SSendJson( DallasTestStatusJson() );
     } );
  FHttpServer.on( "/DallasTestAttempt/", HTTP_POST,
    [this]()
     {
      SSendJson( DallasTestAttemptJson() );
     } );
  FHttpServer.on( "/Test", HTTP_GET,
    [this]()
     {
      FHttpServer.send_P( 200, "text/html; charset=UTF-8", DallasTestPage );
     } );
  FHttpServer.on( "/Test/", HTTP_GET,
    [this]()
     {
      FHttpServer.send_P( 200, "text/html; charset=UTF-8", DallasTestPage );
     } );
#endif

  // Use the project PNG also for browsers that request the conventional favicon path.
  FHttpServer.serveStatic( "/favicon.ico", LittleFS, "/fish.png" );

  // Setup a serveStatic for / as alias for Index.html
  FHttpServer.serveStatic( "/", LittleFS, "/Index.html" );
  // Setup a serveStatic for every file inside the dir '/'
  Dir dir = LittleFS.openDir( "/" );
  while( dir.next() )
    if( !dir.isDirectory() )
     {
      String fname = dir.fileName();
      if( fname.charAt( 0 ) != '/' )
        fname = "/" + fname;
      // if( fname != "/Index.html" )
      FHttpServer.serveStatic( fname.c_str(), LittleFS, fname.c_str() );
     }

  // Setup a server.on for dynamic generated answers
  FHttpServer.on( "/SetFeedTimes/", HTTP_POST, SHandleSetFeedTimes ); 
  FHttpServer.on( "/ManualFeed/", HTTP_POST, SHandleManualFeed ); 
  FHttpServer.on( "/StopFeed/", HTTP_POST, SHandleStopFeed ); 
  FHttpServer.on( "/MFeedChk/", HTTP_POST, SHandleMFeedChk ); 
  FHttpServer.on( "/SetConfig/", HTTP_POST, SHandleSetConfig ); 
  FHttpServer.on( "/SetServoSteps/", HTTP_POST, SHandleSetServoSteps ); 
  FHttpServer.on( "/ResetDevice/", HTTP_POST, SHandleResetDevice );
#ifdef ESP32
  FWebUpdate.Setup( FHttpServer );
#else
  FWebUpdate.Setup( FHttpServer, &EspUpdateSigning::Hash, &EspUpdateSigning::Verifier );
#endif

  // For getting accept-language (only when language select by this...
  //FHttpServer.collectHeaders( "accept-language" );

  FHttpServer.begin();

  MDNS.begin( FeederConfig.Hostname );
  MDNS.addService( "http", "tcp", 80 );

  return true;
 }
#undef HttpServer
