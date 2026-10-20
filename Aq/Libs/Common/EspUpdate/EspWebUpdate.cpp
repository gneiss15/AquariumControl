// EspWebUpdate.cpp, Version: 1.28
#include "EspWebUpdate.h"
#include "DeviceType.inc.h"

#include <ArduinoJson.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace
 {
String GetUpdaterErrorString()
 {
#if defined( ARDUINO_ARCH_ESP32 )
  return String( Update.errorString() );
#elif defined( ARDUINO_ARCH_ESP8266 )
  return Update.getErrorString();
#else
  #error Unsupported Arduino architecture for EspWebUpdate
#endif
 }
 } // namespace

namespace
 {
  char const UpdateResponsePage[] PROGMEM = R"ESPUPDATE(<!DOCTYPE html>
<html>
<head>
<meta http-equiv="content-type" content="text/html;charset=UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
body {font-family:sans-serif;text-align:center;padding:2em;}
h1.Ok {color:green;}
h1.Error {color:red;}
button {font-size:1.2em;font-weight:bold;background-color:#04AA6D;border-radius:6px;padding:5px 20px;}
</style>
</head>
<body>
<h1 class="%CLASS%">%RESULT%</h1>
<p>%DETAIL%</p>
<button onclick="%ACTION%">%BUTTON%</button>
</body>
</html>)ESPUPDATE";

  char const RecoveryPage[] PROGMEM = R"ESPRECOVERY(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>%PROJECT% Recovery</title>
<style>
body{font-family:sans-serif;max-width:680px;margin:2em auto;padding:0 1em}
.Status{margin-bottom:1.5em;padding:.8em;background:#eee}
.Warning{margin-bottom:1.5em;padding:.8em;background:#fff3cd;border:1px solid #e0c36c}
form{margin:1.2em 0;padding:1em;border:1px solid #ccc}
input[type=file]{width:100%;margin:.5em 0 1em}
input[type=submit]{font-size:1.05em;font-weight:bold;padding:.35em 1.2em}
input[type=submit]:disabled{opacity:.65}
</style>
<script>
function PrepareRecoveryCombined(Form){
  const Firmware=Form.querySelector('input[name="firmware"]');
  const Filesystem=Form.querySelector('input[name="filesystem"]');
  if(!Firmware.files.length&&!Filesystem.files.length){
    alert("Select firmware and/or LittleFS.");return false;}
  if(Firmware.files.length){
    const Name=Firmware.files[0].name;
    if(!Name.endsWith(".bin.signed")&&!Name.endsWith(".bin.gz.signed")){
      alert("Select typed signed firmware (*.bin.signed or *.bin.gz.signed).");return false;}
    Form.action="/CombinedUpdate/?_EspFirmwareSize="+Firmware.files[0].size;}
  else Firmware.disabled=true;
  if(Filesystem.files.length){
    if(!Filesystem.files[0].name.endsWith(".littlefs.signed")){
      alert("Select typed signed LittleFS (*.littlefs.signed).");return false;}}
  else Filesystem.disabled=true;
  Form.querySelector('input[type="submit"]').disabled=true;
  return true;}
function PrepareRecoveryConfig(Form){
  const Input=Form.querySelector('input[name="config"]');
  if(!Input.files.length)return false;
  if(!Input.files[0].name.endsWith(".Config.json.update.signed")){
    alert("Select typed signed Config recovery artifact (*.Config.json.update.signed).");return false;}
  Form.action="/FwUpdate/?_EspConfigSize="+Input.files[0].size;
  Form.querySelector('input[type="submit"]').disabled=true;
  return true;}
</script>
</head>
<body>
<h1>%PROJECT% Recovery</h1>
<div class="Status">%NETWORK%</div>
<div class="Warning">
Startup recovery mode. Only DeviceType-matching recovery artifacts are accepted.<br>
Firmware and LittleFS can be recovered together and cause only one restart.
</div>
<form method="POST" enctype="multipart/form-data" action="/CombinedUpdate/" onsubmit="return PrepareRecoveryCombined(this)">
<b>Firmware + LittleFS</b><br>
Firmware:<br><input type="file" accept=".bin.signed,.bin.gz.signed" name="firmware"><br>
LittleFS:<br><input type="file" accept=".littlefs.signed" name="filesystem"><br>
<input type="submit" value="Recover firmware / LittleFS"></form>
<form method="POST" enctype="multipart/form-data" action="/FwUpdate/" onsubmit="return PrepareRecoveryConfig(this)">
<b>Config.json</b><br><input type="file" accept=".update.signed" name="config" required><br>
<input type="submit" value="Recover Config.json"></form>
</body>
</html>)ESPRECOVERY";

  char const StartupErrorPage[] PROGMEM = R"ESPSTARTUPERROR(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>%PROJECT% Startup error</title>
<style>
body{font-family:sans-serif;max-width:680px;margin:2em auto;padding:0 1em}
.Warning{margin-bottom:1.5em;padding:.8em;background:#fff3cd;border:1px solid #e0c36c}
table{border-collapse:collapse;width:100%;margin-bottom:1.5em}
th,td{padding:.4em;text-align:left;vertical-align:top}
input[type=file]{width:100%}
input[type=submit]{font-size:1.05em;font-weight:bold;padding:.35em 1.2em}
</style>
<script>
function PrepareFirmwareUpdate(Form){
  const Firmware=Form.querySelector('input[name="firmware"]');
  const Filesystem=Form.querySelector('input[name="filesystem"]');
  if(!Firmware.files.length&&!Filesystem.files.length){
    alert("Select firmware and/or LittleFS.");return false;}
  if(Firmware&&Firmware.files.length){
    const Name=Firmware.files[0].name;
    if(!Name.endsWith(".bin.signed")&&!Name.endsWith(".bin.gz.signed")){
      alert("Invalid firmware file.");return false;}
    Form.action="/CombinedUpdate/?_EspFirmwareSize="+Firmware.files[0].size;}
  else Firmware.disabled=true;
  if(!Filesystem.files.length)Filesystem.disabled=true;
  return true;}
</script>
</head>
<body>
<h1>%PROJECT% Startup error</h1>
<div class="Warning">
Config.json and Index.html were validated successfully, but initialization failed afterwards.<br>
Only update functions are available.<br><br>
<b>%ERROR%</b>
</div>
<form method="POST" enctype="multipart/form-data" action="/CombinedUpdate/">
<table>
<tr><th colspan="2">Update package</th></tr>
<tr><th>Package</th><td><input type="file" accept=".update.signed" name="update" required></td></tr>
<tr><th colspan="2"><input type="submit" value="Update"></th></tr>
</table>
</form>
<form method="POST" enctype="multipart/form-data" action="/CombinedUpdate/" onsubmit="return PrepareFirmwareUpdate(this)">
<table>
<tr><th colspan="2">Update Firmware and/or Filesystem</th></tr>
<tr><th>Firmware</th><td><input type="file" accept=".bin.signed,.bin.gz.signed" name="firmware"></td></tr>
<tr><th>LittleFS</th><td><input type="file" accept=".littlefs.signed" name="filesystem"></td></tr>
<tr><th colspan="2"><input type="submit" value="Update"></th></tr>
</table>
</form>
</body>
</html>)ESPSTARTUPERROR";

  uint8_t const DeviceTypeMagic[8] = { 'E', 'S', 'P', 'D', 'T', '0', '1', 0 };
  constexpr size_t DeviceTypeHeaderSize = 64;
  constexpr uint8_t DeviceTypeFormatVersion = 1;
  constexpr uint8_t DeviceTypeFirmwareArtifact = 1;
  constexpr uint8_t DeviceTypeFileSystemArtifact = 2;
  constexpr uint8_t DeviceTypeCombinedArtifact = 3;
  constexpr uint8_t DeviceTypeConfigArtifact = 4;
  constexpr size_t MaximumConfigPayloadSize = 32768;
  uint8_t const CombinedPackageMagic[8] = { 'E', 'S', 'P', 'U', 'P', 'D', '1', 0 };
  uint8_t const PackageMagic[8] = { 'L', 'F', 'S', 'U', 'P', 'D', '3', 0 };
  char const PackageTemporarySuffix[] = ".espupd";
  constexpr uint16_t PackageMaximumFileCount = 64;
  constexpr size_t PackageRamSafetyReserve = 4096;
  constexpr size_t PackageRamChunkSize = 1024;
  constexpr size_t PackageCompareChunkSize = 128;

  uint16_t ReadUInt16Le( uint8_t const * Data )
   {
    return uint16_t( Data[0] ) |
      uint16_t( Data[1] ) << 8;
   }

  uint32_t ReadUInt32Le( uint8_t const * Data )
   {
    return uint32_t( Data[0] ) |
      uint32_t( Data[1] ) << 8 |
      uint32_t( Data[2] ) << 16 |
      uint32_t( Data[3] ) << 24;
   }
 }

TEspWebUpdate::TEspWebUpdate(void)
 : FHttpServer( nullptr )
#ifdef ESP8266
 , FSigningHash( nullptr )
 , FSigningVerifier( nullptr )
#endif
 , FCurrentPart( TPart::None )
 , FPackageState( TPackageState::Header )
 , FCombinedPackageState( TCombinedPackageState::Header )
 , FCombinedRequest( false )
 , FCombinedPackageUpload( false )
 , FCombinedPackageVerificationActive( false )
 , FCombinedPackageVerified( false )
 , FConfigCompleted( false )
 , FConfigVerificationActive( false )
 , FFileSystemCompleted( false )
 , FFileSystemUnmounted( false )
 , FFileSystemVerificationActive( false )
 , FFirmwareVerificationActive( false )
 , FFirmwareCompleted( false )
 , FFirmwareStaged( false )
 , FProgressResponseActive( false )
 , FRequestActive( false )
 , FRecoveryMode( false )
 , FSignatureTailCapacity( 0 )
 , FSignatureTailSize( 0 )
 , FFirmwarePayloadSize( 0 )
 , FFirmwareExpectedPayloadSize( 0 )
 , FConfigExpectedPayloadSize( 0 )
 , FDeviceTypeHeaderSize( 0 )
 , FDeviceTypePayloadSize( 0 )
 , FDeviceTypePayloadRemaining( 0 )
 , FDeviceTypeHeaderVerified( false )
 , FUpdatePayloadStarted( false )
 , FUploadedFileCount( 0 )
 , FCombinedPackageHeaderSize( 0 )
 , FCombinedFirmwareRemaining( 0 )
 , FPackageHeaderSize( 0 )
 , FPackageFileHeaderSize( 0 )
 , FPackagePathLength( 0 )
 , FPackagePathSize( 0 )
 , FPackageTotalDataLength( 0 )
 , FPackageReceivedDataLength( 0 )
 , FPackageFileCount( 0 )
 , FPackageTableIndex( 0 )
 , FPackageDataIndex( 0 )
 , FPackageChangedFileCount( 0 )
 , FPackageDeletedFileCount( 0 )
 , FPackageCommittedFileCount( 0 )
 , FPackageRamDataSize( 0 )
 {
 }

#ifdef ESP8266
void TEspWebUpdate::Setup( TEspUpdateHttpServer & HttpServer,
  UpdaterHashClass * SigningHash, UpdaterVerifyClass * SigningVerifier )
#else
void TEspWebUpdate::Setup( TEspUpdateHttpServer & HttpServer )
#endif
 {
  FHttpServer = &HttpServer;
  FRecoveryMode = false;
  FRescueProjectName.clear();
  FStartupError.clear();

#ifdef ESP8266
  FSigningHash = SigningHash;
  FSigningVerifier = SigningVerifier;
  if( FSigningHash && FSigningVerifier )
    Update.installSignature( FSigningHash, FSigningVerifier );
#endif

  // A reset during an earlier staging upload can leave only temporary files.
  // Remove them before exposing the update routes. A request repeats this
  // cleanup and reports an error if removal is no longer possible.
  CleanupTemporaryPackageFiles();
  FError.clear();

  FHttpServer->on( "/FwUpdate/", HTTP_POST,
    [this](){ CompleteRequest( false ); },
    [this](){ HandleUpload( false ); } );
  FHttpServer->on( "/CombinedUpdate/", HTTP_POST,
    [this](){ CompleteRequest( true ); },
    [this](){ HandleUpload( true ); } );
 }

#ifdef ESP8266
void TEspWebUpdate::SetupRecovery( TEspUpdateHttpServer & HttpServer,
  UpdaterHashClass * SigningHash, UpdaterVerifyClass * SigningVerifier,
  char const * ProjectName )
 {
  FHttpServer = &HttpServer;
  FSigningHash = SigningHash;
  FSigningVerifier = SigningVerifier;
  FRecoveryMode = true;
  FRescueProjectName = ProjectName ? ProjectName : "ESP";
  FStartupError.clear();
  FError.clear();

  if( FSigningHash && FSigningVerifier )
    Update.installSignature( FSigningHash, FSigningVerifier );

  FHttpServer->on( "/", HTTP_GET, [this](){ SendRecoveryPage(); } );
  FHttpServer->on( "/Index.html", HTTP_GET, [this](){ SendRecoveryPage(); } );
  FHttpServer->on( "/FwUpdate/", HTTP_POST,
    [this](){ CompleteRequest( false ); },
    [this](){ HandleUpload( false ); } );
  FHttpServer->on( "/CombinedUpdate/", HTTP_POST,
    [this](){ CompleteRequest( true ); },
    [this](){ HandleUpload( true ); } );
 }

void TEspWebUpdate::SetupStartupError( TEspUpdateHttpServer & HttpServer,
  UpdaterHashClass * SigningHash, UpdaterVerifyClass * SigningVerifier,
  char const * ProjectName, String const & Error )
 {
  Setup( HttpServer, SigningHash, SigningVerifier );
  FRescueProjectName = ProjectName ? ProjectName : "ESP";
  FStartupError = Error;
  FHttpServer->on( "/", HTTP_GET, [this](){ SendStartupErrorPage(); } );
  FHttpServer->on( "/Index.html", HTTP_GET, [this](){ SendStartupErrorPage(); } );
 }
#else
void TEspWebUpdate::SetupRecovery( TEspUpdateHttpServer & HttpServer,
  char const * ProjectName )
 {
  FHttpServer = &HttpServer;
  FRecoveryMode = true;
  FRescueProjectName = ProjectName ? ProjectName : "ESP";
  FStartupError.clear();
  FError.clear();

  FHttpServer->on( "/", HTTP_GET, [this](){ SendRecoveryPage(); } );
  FHttpServer->on( "/Index.html", HTTP_GET, [this](){ SendRecoveryPage(); } );
  FHttpServer->on( "/FwUpdate/", HTTP_POST,
    [this](){ CompleteRequest( false ); },
    [this](){ HandleUpload( false ); } );
  FHttpServer->on( "/CombinedUpdate/", HTTP_POST,
    [this](){ CompleteRequest( true ); },
    [this](){ HandleUpload( true ); } );
 }

void TEspWebUpdate::SetupStartupError( TEspUpdateHttpServer & HttpServer,
  char const * ProjectName, String const & Error )
 {
  Setup( HttpServer );
  FRescueProjectName = ProjectName ? ProjectName : "ESP";
  FStartupError = Error;
  FHttpServer->on( "/", HTTP_GET, [this](){ SendStartupErrorPage(); } );
  FHttpServer->on( "/Index.html", HTTP_GET, [this](){ SendStartupErrorPage(); } );
 }
#endif

void TEspWebUpdate::AbortCurrentUpdate(void)
 {
#ifdef ESP8266
  if( FConfigVerificationActive )
   {
    Update.installSignature( FSigningHash, FSigningVerifier );
    ResetConfigVerification();
    DiscardLittleFSTemporary( "/Config.json" );
   }
   else if( FCombinedPackageVerificationActive )
   {
    if( Update.isRunning() )
      Update.end();
    Update.clearError();
    Update.installSignature( FSigningHash, FSigningVerifier );
    ResetCombinedPackageVerification();
    CleanupTemporaryPackageFiles();
   }
   else if( FFileSystemVerificationActive )
   {
    String const SavedError( FError );
    Update.installSignature( FSigningHash, FSigningVerifier );
    if( Update.isRunning() )
      Update.end( true );
    Update.clearError();
    FError = SavedError;
    ResetFileSystemVerification();
   }
   else if( FFirmwareVerificationActive )
   {
    String const SavedError( FError );
    if( Update.isRunning() )
      Update.end();
    Update.clearError();
    Update.installSignature( FSigningHash, FSigningVerifier );
    FError = SavedError;
    ResetFirmwareVerification();
   }
   else if( Update.isRunning() )
    Update.end();
#else
  if( FConfigVerificationActive )
   {
    ResetConfigVerification();
    DiscardLittleFSTemporary( "/Config.json" );
   }
  if( Update.isRunning() )
    Update.end();
#endif
 }

bool TEspWebUpdate::BeginCombinedPackageUpload(void)
 {
#ifdef ESP8266
  if( !FSigningHash || !FSigningVerifier )
   {
    SetError( F( "Signed combined update package is unavailable in this build" ) );
    return false;
   }

  const size_t SignatureLength = FSigningVerifier->length();
  if( !SignatureLength )
   {
    SetError( F( "Combined update signature verifier has zero length" ) );
    return false;
   }

  FSignatureTailCapacity = SignatureLength + sizeof( uint32_t );
  FSignatureTail.reset( new(std::nothrow) uint8_t[FSignatureTailCapacity] );
  if( !FSignatureTail )
   {
    SetError( F( "Not enough memory for combined update signature verification" ) );
    return false;
   }

  FSignatureTailSize = 0;
  FCombinedPackageVerificationActive = true;
  FCombinedPackageVerified = false;
  FCombinedPackageState = TCombinedPackageState::Header;
  FCombinedPackageHeaderSize = 0;
  FCombinedFirmwareRemaining = 0;
  ResetPackageParser();
  FSigningHash->begin();
  ResetDeviceTypeHeader();
  Update.installSignature( nullptr, nullptr );
  return true;
#else
  SetError( F( "Signed combined update package is unavailable in this build" ) );
  return false;
#endif
 }

bool TEspWebUpdate::BeginConfigUpload( size_t SignedFileSize )
 {
  FConfigUploadData.clear();
  FConfigExpectedPayloadSize = 0;
#ifdef ESP8266
  if( !FSigningHash || !FSigningVerifier )
   {
    SetError( F( "Signed Config recovery upload is unavailable in this build" ) );
    return false;
   }
  const size_t SignatureLength = FSigningVerifier->length();
  if( !SignatureLength )
   {
    SetError( F( "Config signature verifier has zero length" ) );
    return false;
   }
  FSignatureTailCapacity = SignatureLength + sizeof( uint32_t );
  if( SignedFileSize <= FSignatureTailCapacity + DeviceTypeHeaderSize )
   {
    SetError( F( "Config signed file is too short for DeviceType metadata" ) );
    return false;
   }
  FConfigExpectedPayloadSize =
    SignedFileSize - FSignatureTailCapacity - DeviceTypeHeaderSize;
  if( !FConfigExpectedPayloadSize || FConfigExpectedPayloadSize > MaximumConfigPayloadSize )
   {
    SetError( F( "Config payload size is invalid" ) );
    return false;
   }
  FSignatureTail.reset( new(std::nothrow) uint8_t[FSignatureTailCapacity] );
  if( !FSignatureTail )
   {
    SetError( F( "Not enough memory for Config signature verification" ) );
    return false;
   }
  FSignatureTailSize = 0;
  FSigningHash->begin();
#else
  (void)SignedFileSize;
#endif
  FConfigVerificationActive = true;
  ResetDeviceTypeHeader();
  return true;
 }

bool TEspWebUpdate::BeginFileSystemUpload(void)
 {
#ifdef ESP8266
  if( !FSigningHash || !FSigningVerifier )
   {
    SetError( F( "Signed LittleFS upload is unavailable in this build" ) );
    return false;
   }

  const size_t SignatureLength = FSigningVerifier->length();
  if( !SignatureLength )
   {
    SetError( F( "LittleFS signature verifier has zero length" ) );
    return false;
   }

  FSignatureTailCapacity = SignatureLength + sizeof( uint32_t );
  FSignatureTail.reset( new(std::nothrow) uint8_t[FSignatureTailCapacity] );
  if( !FSignatureTail )
   {
    SetError( F( "Not enough memory for LittleFS signature verification" ) );
    return false;
   }

  FSignatureTailSize = 0;
  FFileSystemVerificationActive = true;
  FSigningHash->begin();
  ResetDeviceTypeHeader();
  return true;
#else
  FFileSystemVerificationActive = true;
  ResetDeviceTypeHeader();
  return true;
#endif
 }

bool TEspWebUpdate::BeginFileSystemPayloadUpload(void)
 {
#ifdef ESP8266
  if( FDeviceTypePayloadSize != GetMaximumSize( TPart::FileSystem ) )
   {
    SetError( F( "LittleFS DeviceType payload has the wrong partition size" ) );
    return false;
   }

  LittleFS.end();
  FFileSystemUnmounted = true;
  Update.installSignature( nullptr, nullptr );
  if( !Update.begin( GetMaximumSize( TPart::FileSystem ), U_FS ) )
   {
    SetUpdaterError( F( "LittleFS update start failed" ) );
    AbortCurrentUpdate();
    return false;
   }
  FUpdatePayloadStarted = true;
  return true;
#else
  if( !FDeviceTypePayloadSize )
   {
    SetError( F( "LittleFS DeviceType payload is empty" ) );
    return false;
   }
  LittleFS.end();
  FFileSystemUnmounted = true;
  if( !Update.begin( FDeviceTypePayloadSize, U_FLASHFS ) )
   {
    SetUpdaterError( F( "LittleFS update start failed" ) );
    AbortCurrentUpdate();
    return false;
   }
  FUpdatePayloadStarted = true;
  return true;
#endif
 }

bool TEspWebUpdate::BeginFirmwareUpload( size_t SignedFileSize )
 {
#ifdef ESP8266
  if( !FSigningHash || !FSigningVerifier )
   {
    SetError( F( "Signed firmware upload is unavailable in this build" ) );
    return false;
   }

  const size_t SignatureLength = FSigningVerifier->length();
  if( !SignatureLength )
   {
    SetError( F( "Firmware signature verifier has zero length" ) );
    return false;
   }

  FSignatureTailCapacity = SignatureLength + sizeof( uint32_t );
  if( SignedFileSize <= FSignatureTailCapacity + DeviceTypeHeaderSize )
   {
    SetError( F( "Firmware signed file is too short for DeviceType metadata" ) );
    return false;
   }

  FFirmwareExpectedPayloadSize =
    SignedFileSize - FSignatureTailCapacity - DeviceTypeHeaderSize;
  FSignatureTail.reset( new(std::nothrow) uint8_t[FSignatureTailCapacity] );
  if( !FSignatureTail )
   {
    SetError( F( "Not enough memory for firmware signature verification" ) );
    return false;
   }

  FSignatureTailSize = 0;
  FFirmwarePayloadSize = 0;
  FFirmwareVerificationActive = true;
  FSigningHash->begin();
#else
  (void)SignedFileSize;
  FFirmwarePayloadSize = 0;
  FFirmwareExpectedPayloadSize = 0;
#endif
  ResetDeviceTypeHeader();
  return true;
 }

bool TEspWebUpdate::BeginFirmwarePayloadUpload( size_t PayloadSize )
 {
#ifdef ESP8266
  if( PayloadSize != FFirmwareExpectedPayloadSize )
   {
    SetError( F( "Firmware DeviceType payload size does not match selected file" ) );
    return false;
   }

  const size_t MaximumSize = GetMaximumSize( TPart::Firmware );
  if( PayloadSize >= MaximumSize )
   {
    SetError( F( "Firmware does not fit into available OTA space. Update via Rescue firmware." ) );
    return false;
   }

  Update.installSignature( nullptr, nullptr );
  // Keep one byte deliberately unwritten so an interrupted or unverified
  // upload cannot be staged accidentally.
  if( !Update.begin( PayloadSize + 1, U_FLASH ) )
#else
  if( !PayloadSize )
   {
    SetError( F( "Firmware DeviceType payload is empty" ) );
    return false;
   }
  if( !Update.begin( PayloadSize, U_FLASH ) )
#endif
   {
    SetUpdaterError( F( "Firmware update start failed" ) );
    AbortCurrentUpdate();
    return false;
   }
  FUpdatePayloadStarted = true;
  return true;
 }

void TEspWebUpdate::BeginProgressResponse(void)
 {
  FProgressResponseActive = true;
  FHttpServer->client().setNoDelay( true );
  FHttpServer->sendHeader( "Cache-Control", "no-store" );
  FHttpServer->setContentLength( CONTENT_LENGTH_UNKNOWN );
  FHttpServer->send( 200, "text/plain", "" );
 }

void TEspWebUpdate::BeginRequest( bool CombinedRequest )
 {
  if( FRequestActive )
   {
    if( FCombinedRequest != CombinedRequest )
      SetError( F( "Upload route changed during request" ) );
    return;
   }

  ResetRequest();
  if( !FRecoveryMode && !CleanupTemporaryPackageFiles() )
    return;

  FRequestActive = true;
  FCombinedRequest = CombinedRequest;
#ifdef ESP8266
  WiFiClient::stopAllExcept( &FHttpServer->client() );
  WiFiUDP::stopAll();
  delay( 0 );
#endif
 }

bool TEspWebUpdate::AllocatePackageRam( size_t Size )
 {
#ifdef ESP8266
  ResetPackageRam();
  if( !Size )
    return true;

  const size_t FreeHeap = ESP.getFreeHeap();
  if( FreeHeap <= PackageRamSafetyReserve ||
      Size > FreeHeap - PackageRamSafetyReserve )
   {
    SetError( String( F( "Not enough RAM after LittleFS staging: need " ) ) +
      String( unsigned( Size ) ) + " B, free " + String( unsigned( FreeHeap ) ) +
      " B, max block " + String( unsigned( ESP.getMaxFreeBlockSize() ) ) +
      " B. Update via Rescue firmware." );
    return false;
   }

  const size_t ChunkCount = ( Size + PackageRamChunkSize - 1 ) / PackageRamChunkSize;
  FPackageRamChunks.reserve( ChunkCount );
  size_t Remaining = Size;
  while( Remaining )
   {
    const size_t ChunkSize = std::min( Remaining, PackageRamChunkSize );
    std::unique_ptr<uint8_t[]> Chunk( new(std::nothrow) uint8_t[ChunkSize] );
    if( !Chunk )
     {
      SetError( String( F( "Could not allocate chunked RAM for compact LittleFS update staging: need " ) ) +
        String( unsigned( Size ) ) + " B, free " + String( unsigned( ESP.getFreeHeap() ) ) +
        " B, max block " + String( unsigned( ESP.getMaxFreeBlockSize() ) ) +
        " B. Update via Rescue firmware." );
      ResetPackageRam();
      return false;
     }
    FPackageRamChunks.push_back( std::move( Chunk ) );
    Remaining -= ChunkSize;
   }

  FPackageRamDataSize = Size;
  return true;
#else
  (void)Size;
  SetError( F( "Compact combined update RAM staging is unavailable on this platform" ) );
  return false;
#endif
 }

bool TEspWebUpdate::CopyToPackageRam( size_t Offset, uint8_t const * Data, size_t Size )
 {
  if( Offset > FPackageRamDataSize || Size > FPackageRamDataSize - Offset )
   {
    SetError( F( "Compact LittleFS RAM staging write exceeds allocated space" ) );
    return false;
   }

  while( Size )
   {
    const size_t ChunkIndex = Offset / PackageRamChunkSize;
    const size_t ChunkOffset = Offset % PackageRamChunkSize;
    if( ChunkIndex >= FPackageRamChunks.size() || !FPackageRamChunks[ChunkIndex] )
     {
      SetError( F( "Compact LittleFS RAM staging chunk is missing" ) );
      return false;
     }

    const size_t CopySize = std::min( Size, PackageRamChunkSize - ChunkOffset );
    memcpy( FPackageRamChunks[ChunkIndex].get() + ChunkOffset, Data, CopySize );
    Offset += CopySize;
    Data += CopySize;
    Size -= CopySize;
   }
  return true;
 }

bool TEspWebUpdate::BuildPackageStoragePlan(void)
 {
#ifdef ESP8266
  TFSInfo FsInfo;
  if( !GetLittleFSInfo( FsInfo ) )
   {
    SetError( F( "Could not read LittleFS information" ) );
    return false;
   }

  const size_t BlockSize = FsInfo.blockSize ? FsInfo.blockSize : 4096;
  const size_t FreeBytes = FsInfo.totalBytes > FsInfo.usedBytes
    ? FsInfo.totalBytes - FsInfo.usedBytes
    : 0;
  size_t TemporaryBudget = FreeBytes > BlockSize
    ? ( ( FreeBytes - BlockSize ) / BlockSize ) * BlockSize
    : 0;

  std::vector<size_t> EntryOrder;
  EntryOrder.reserve( FPackageEntries.size() );
  for( size_t Index = 0;  Index < FPackageEntries.size();  ++Index )
   {
    TPackageEntry & Entry = FPackageEntries[Index];
    Entry.TemporaryDataSize = 0;
    Entry.RamOffset = 0;
    if( !Entry.Identical )
      EntryOrder.push_back( Index );
   }

  std::sort( EntryOrder.begin(), EntryOrder.end(),
    [this]( size_t Left, size_t Right )
     {
      if( FPackageEntries[Left].DataSize != FPackageEntries[Right].DataSize )
        return FPackageEntries[Left].DataSize > FPackageEntries[Right].DataSize;
      return strcmp( FPackageEntries[Left].Name.c_str(), FPackageEntries[Right].Name.c_str() ) < 0;
     } );

  size_t RamSize = 0;
  for( size_t Index : EntryOrder )
   {
    TPackageEntry & Entry = FPackageEntries[Index];
    Entry.RamOffset = RamSize;

    const size_t DataSize = size_t( Entry.DataSize );
    if( DataSize && TemporaryBudget )
     {
      const size_t AllocatedSize =
        ( ( DataSize + BlockSize - 1 ) / BlockSize ) * BlockSize;
      if( AllocatedSize <= TemporaryBudget )
       {
        Entry.TemporaryDataSize = DataSize;
        TemporaryBudget -= AllocatedSize;
       }
       else
       {
        Entry.TemporaryDataSize = TemporaryBudget;
        TemporaryBudget = 0;
       }
     }

    RamSize += DataSize - Entry.TemporaryDataSize;
   }

  return AllocatePackageRam( RamSize );
#else
  SetError( F( "Compact combined update staging is unavailable on this platform" ) );
  return false;
#endif
 }

bool TEspWebUpdate::CleanupTemporaryPackageFiles(void)
 {
  std::vector<String> TemporaryFiles;
#ifdef ESP32
  File Directory = LittleFS.open( "/" );
  for( File Entry = Directory.openNextFile(); Entry; Entry = Directory.openNextFile() )
   {
    if( Entry.isDirectory() )
      continue;
    String FileName = Entry.name();
    if( !FileName.startsWith( "/" ) )
      FileName = "/" + FileName;
    if( FileName.endsWith( PackageTemporarySuffix ) )
      TemporaryFiles.push_back( FileName );
   }
#else
  Dir Directory = LittleFS.openDir( "/" );
  while( Directory.next() )
   {
    if( Directory.isDirectory() )
      continue;
    String FileName = Directory.fileName();
    if( !FileName.startsWith( "/" ) )
      FileName = "/" + FileName;
    if( FileName.endsWith( PackageTemporarySuffix ) )
      TemporaryFiles.push_back( FileName );
   }
#endif

  for( String const & FileName : TemporaryFiles )
    if( !LittleFS.remove( FileName.c_str() ) )
     {
      SetError( String( F( "Could not remove temporary LittleFS update file: " ) ) + FileName );
      return false;
     }
  return true;
 }

bool TEspWebUpdate::CompletePackageComparePass(void)
 {
  if( FPackageTargetFile )
    FPackageTargetFile.close();

  if( FPackageReceivedDataLength != FPackageTotalDataLength )
   {
    SetError( F( "Compact LittleFS compare pass ended before all file data was received" ) );
    return false;
   }

  FPackageChangedFileCount = 0;
  for( TPackageEntry const & Entry : FPackageEntries )
    if( !Entry.Identical )
      ++FPackageChangedFileCount;

  FPackageDeletedFileCount = 0;
#ifdef ESP32
  File Directory = LittleFS.open( "/" );
  for( File Entry = Directory.openNextFile(); Entry; Entry = Directory.openNextFile() )
   {
    if( Entry.isDirectory() )
      continue;
    String FileName = Entry.name();
    if( !FileName.startsWith( "/" ) )
      FileName = "/" + FileName;
    if( FileName.endsWith( PackageTemporarySuffix ) )
      continue;
    if( !IsPackageFileName( FileName ) )
      ++FPackageDeletedFileCount;
   }
#else
  Dir Directory = LittleFS.openDir( "/" );
  while( Directory.next() )
   {
    if( Directory.isDirectory() )
      continue;
    String FileName = Directory.fileName();
    if( !FileName.startsWith( "/" ) )
      FileName = "/" + FileName;
    if( FileName.endsWith( PackageTemporarySuffix ) )
      continue;
    if( !IsPackageFileName( FileName ) )
      ++FPackageDeletedFileCount;
   }
#endif

  if( !BuildPackageStoragePlan() )
    return false;

  FPackageReceivedDataLength = 0;
  FPackageDataIndex = 0;
  FPackageState = TPackageState::StageData;
  return StartPackageStageEntry();
 }

bool TEspWebUpdate::CommitPackageFiles(void)
 {
  if( FPackageTargetFile )
    FPackageTargetFile.close();

  String CompareLine( F( "Files changed: " ) );
  CompareLine += String( unsigned( FPackageChangedFileCount ) );
  CompareLine += F( ", deleted: " );
  CompareLine += String( unsigned( FPackageDeletedFileCount ) );
  CompareLine += F( ", total: " );
  CompareLine += String( unsigned( FPackageEntries.size() ) );
  SendProgressLine( CompareLine );

  FPackageCommittedFileCount = 0;
  for( TPackageEntry const & Entry : FPackageEntries )
    if( Entry.Identical )
      ++FPackageCommittedFileCount;
  SendProgressLine( String( F( "FILES " ) ) +
    String( unsigned( FPackageCommittedFileCount ) ) + " " +
    String( unsigned( FPackageEntries.size() ) ) );

  if( !CommitTemporaryPackageFiles() )
    return false;

  // After all temporary files have been renamed, no current .espupd file
  // remains. Remove obsolete old files now so RAM-staged files get all space
  // that the final filesystem layout is allowed to use.
  if( !DeleteObsoletePackageFiles() )
    return false;

  if( !CommitRamPackageFiles() )
    return false;

  ResetPackageRam();
  return true;
 }

bool TEspWebUpdate::CommitRamPackageFiles(void)
 {
  for( TPackageEntry const & Entry : FPackageEntries )
   {
    if( Entry.Identical || Entry.TemporaryDataSize )
      continue;

    if( LittleFS.exists( Entry.Name.c_str() ) && !LittleFS.remove( Entry.Name.c_str() ) )
     {
      SetError( String( F( "Could not replace LittleFS file: " ) ) + Entry.Name );
      return false;
     }

    File TargetFile = LittleFS.open( Entry.Name.c_str(), "w" );
    if( !TargetFile )
     {
      SetError( String( F( "Could not create LittleFS file: " ) ) + Entry.Name );
      return false;
     }

    if( Entry.DataSize &&
        !WritePackageRamToFile( TargetFile, Entry.RamOffset, Entry.DataSize ) )
     {
      TargetFile.close();
      SetError( String( F( "Could not write LittleFS file: " ) ) + Entry.Name );
      return false;
     }
    TargetFile.close();
    ReportPackageFileCommitted();
   }
  return true;
 }

bool TEspWebUpdate::CommitTemporaryPackageFiles(void)
 {
  for( TPackageEntry const & Entry : FPackageEntries )
   {
    if( Entry.Identical || !Entry.TemporaryDataSize )
      continue;

    String const TemporaryName = GetTemporaryPackageFileName( Entry.Name );
    if( !LittleFS.exists( TemporaryName.c_str() ) )
     {
      SetError( String( F( "Temporary LittleFS update file is missing: " ) ) + TemporaryName );
      return false;
     }
    if( LittleFS.exists( Entry.Name.c_str() ) && !LittleFS.remove( Entry.Name.c_str() ) )
     {
      SetError( String( F( "Could not replace LittleFS file: " ) ) + Entry.Name );
      return false;
     }

    const size_t RamDataSize = size_t( Entry.DataSize ) - Entry.TemporaryDataSize;
    if( RamDataSize )
     {
      File TemporaryFile = LittleFS.open( TemporaryName.c_str(), "a" );
      if( !TemporaryFile )
       {
        SetError( String( F( "Could not append temporary LittleFS update file: " ) ) + Entry.Name );
        return false;
       }
      if( !WritePackageRamToFile( TemporaryFile, Entry.RamOffset, RamDataSize ) )
       {
        TemporaryFile.close();
        SetError( String( F( "Could not complete temporary LittleFS update file: " ) ) + Entry.Name );
        return false;
       }
      TemporaryFile.close();
     }

    if( !LittleFS.rename( TemporaryName.c_str(), Entry.Name.c_str() ) )
     {
      SetError( String( F( "Could not activate LittleFS update file: " ) ) + Entry.Name );
      return false;
     }
    ReportPackageFileCommitted();
   }
  return true;
 }

void TEspWebUpdate::CancelPendingFirmware(void)
 {
  if( !FFirmwareStaged )
    return;
#ifdef ESP8266
  eboot_command_clear();
#endif
  FFirmwareStaged = false;
 }

void TEspWebUpdate::ResetConfigVerification(void)
 {
  FConfigVerificationActive = false;
  FConfigExpectedPayloadSize = 0;
  FConfigUploadData.clear();
  ResetSignatureTail();
 }

void TEspWebUpdate::ResetDeviceTypeHeader(void)
 {
  FDeviceTypeHeaderSize = 0;
  FDeviceTypePayloadSize = 0;
  FDeviceTypePayloadRemaining = 0;
  FDeviceTypeHeaderVerified = false;
  FUpdatePayloadStarted = false;
 }

bool TEspWebUpdate::ConsumeDeviceTypeHeader( TPart Part,
  uint8_t const * & Data, size_t & Size )
 {
  if( FDeviceTypeHeaderVerified )
    return true;

  const size_t CopySize = std::min( Size,
    DeviceTypeHeaderSize - FDeviceTypeHeaderSize );
  memcpy( FDeviceTypeHeader + FDeviceTypeHeaderSize, Data, CopySize );
  FDeviceTypeHeaderSize += CopySize;
  Data += CopySize;
  Size -= CopySize;

  if( FDeviceTypeHeaderSize != DeviceTypeHeaderSize )
    return true;

  if( memcmp( FDeviceTypeHeader, DeviceTypeMagic, sizeof( DeviceTypeMagic ) ) != 0 ||
      ReadUInt16Le( FDeviceTypeHeader + 8 ) != DeviceTypeHeaderSize ||
      FDeviceTypeHeader[10] != DeviceTypeFormatVersion )
   {
    SetError( F( "Update rejected: invalid DeviceType header" ) );
    return false;
   }

  uint8_t ExpectedArtifact = 0;
  if( Part == TPart::Firmware )
    ExpectedArtifact = DeviceTypeFirmwareArtifact;
   else if( Part == TPart::FileSystem )
    ExpectedArtifact = DeviceTypeFileSystemArtifact;
   else if( Part == TPart::CombinedPackage )
    ExpectedArtifact = DeviceTypeCombinedArtifact;
   else if( Part == TPart::Config )
    ExpectedArtifact = DeviceTypeConfigArtifact;
  if( !ExpectedArtifact || FDeviceTypeHeader[11] != ExpectedArtifact )
   {
    SetError( F( "Update rejected: DeviceType artifact type mismatch" ) );
    return false;
   }

  char DeviceType[33];
  memcpy( DeviceType, FDeviceTypeHeader + 16, 32 );
  DeviceType[32] = 0;
  char * Terminator = static_cast<char *>( memchr( DeviceType, 0, 32 ) );
  if( !Terminator || Terminator == DeviceType )
   {
    SetError( F( "Update rejected: invalid DeviceType id" ) );
    return false;
   }

  for( uint8_t const * Reserved = FDeviceTypeHeader + 48;
       Reserved != FDeviceTypeHeader + DeviceTypeHeaderSize; ++Reserved )
    if( *Reserved )
     {
      SetError( F( "Update rejected: unsupported DeviceType header extension" ) );
      return false;
     }

  if( strcmp( DeviceType, ESP_UPDATE_DEVICE_TYPE ) != 0 )
   {
    String Error = F( "Update rejected: DeviceType mismatch; device=" );
    Error += ESP_UPDATE_DEVICE_TYPE;
    Error += F( ", artifact=" );
    Error += DeviceType;
    SetError( Error );
    return false;
   }

  FDeviceTypePayloadSize = ReadUInt32Le( FDeviceTypeHeader + 12 );
  if( !FDeviceTypePayloadSize )
   {
    SetError( F( "Update rejected: DeviceType payload is empty" ) );
    return false;
   }

  FDeviceTypePayloadRemaining = FDeviceTypePayloadSize;
  FDeviceTypeHeaderVerified = true;
  return true;
 }

bool TEspWebUpdate::CheckDeviceTypePayloadComplete( TPart Part )
 {
  if( !FDeviceTypeHeaderVerified )
   {
    SetError( F( "Update rejected: DeviceType header is missing or incomplete" ) );
    return false;
   }
  if( FDeviceTypePayloadRemaining )
   {
    String Error = F( "Update rejected: incomplete DeviceType payload for " );
    if( Part == TPart::Firmware )
      Error += F( "firmware" );
     else if( Part == TPart::FileSystem )
      Error += F( "filesystem" );
     else if( Part == TPart::Config )
      Error += F( "config" );
     else
      Error += F( "combined update" );
    SetError( Error );
    return false;
   }
  return true;
 }

bool TEspWebUpdate::CheckFileName( String const & FileName, TPart Part ) const
 {
  if( Part == TPart::Firmware )
   {
#ifdef ESP8266
    return FileName.endsWith( ".bin.signed" ) ||
      FileName.endsWith( ".bin.gz.signed" );
#else
    String Name( FileName );
    Name.toLowerCase();
    return Name.endsWith( ".bin" ) &&
      !Name.endsWith( ".bootloader.bin" ) &&
      !Name.endsWith( ".partitions.bin" ) &&
      !Name.endsWith( ".boot_app0.bin" ) &&
      !Name.endsWith( ".factory.bin" ) &&
      !Name.endsWith( ".merged.bin" );
#endif
   }
  if( Part == TPart::FileSystem )
   {
#ifdef ESP8266
    return FileName.endsWith( ".littlefs.signed" );
#else
    return FileName.endsWith( ".littlefs" );
#endif
   }
  if( Part == TPart::Config )
   {
#ifdef ESP8266
    return FileName.endsWith( ".Config.json.update.signed" );
#else
    return FileName.endsWith( ".Config.json.update" );
#endif
   }
  if( Part == TPart::CombinedPackage )
    return FileName.endsWith( ".update.signed" );
  return false;
 }

bool TEspWebUpdate::CompleteCombinedPackageUpdate(void)
 {
  if( !FCombinedPackageVerified )
   {
    SetError( F( "Combined update package has not been verified" ) );
    return false;
   }

  if( !CommitPackageFiles() )
   {
    AbortCurrentUpdate();
    return false;
   }

  SendProgressLine( "FIRMWARE" );
  if( !Update.end( true ) )
   {
    SetUpdaterError( F( "Firmware package completion failed" ) );
    AbortCurrentUpdate();
    return false;
   }

  FFirmwareCompleted = true;
  FFirmwareStaged = true;
  FFileSystemCompleted = true;
  FCombinedPackageState = TCombinedPackageState::Done;
  FCombinedPackageVerificationActive = false;
  FCombinedPackageVerified = false;
#ifdef ESP8266
  Update.installSignature( FSigningHash, FSigningVerifier );
#endif
  return true;
 }

void TEspWebUpdate::CompleteRequest( bool CombinedRequest )
 {
  if( !FRequestActive )
    BeginRequest( CombinedRequest );

  if( FCombinedRequest != CombinedRequest )
    SetError( F( "Upload route mismatch" ) );

  if( CombinedRequest )
   {
    if( FCombinedPackageUpload )
     {
      if( FUploadedFileCount != 1 )
        SetError( F( "Combined update package must be the only upload file" ) );
      if( !FCombinedPackageVerified )
        SetError( F( "Combined update package missing, incomplete or invalid" ) );
     }
     else
     {
      if( !FFirmwareCompleted && !FFileSystemCompleted )
        SetError( F( "Firmware or LittleFS file missing or incomplete" ) );
      const unsigned CompletedFileCount =
        unsigned( FFirmwareCompleted ) + unsigned( FFileSystemCompleted );
      if( FUploadedFileCount != CompletedFileCount )
        SetError( F( "Combined update contains duplicate or incomplete files" ) );
     }
   }
   else
   {
    const unsigned CompletedFileCount =
      unsigned( FFirmwareCompleted ) + unsigned( FFileSystemCompleted ) +
      unsigned( FConfigCompleted );
    if( FUploadedFileCount != 1 || CompletedFileCount != 1 )
      SetError( F( "Single update requires exactly one complete file" ) );
   }

  const bool ProgressRequested = FHttpServer->arg( "_EspUpdateProgress" ) == "1";
  bool Ok = FError.length() == 0;

  if( Ok && FCombinedPackageUpload )
   {
    if( ProgressRequested )
     {
      BeginProgressResponse();
      SendProgressLine( "PROCESS" );
     }
    Ok = CompleteCombinedPackageUpdate();
   }

  if( !Ok )
   {
    AbortCurrentUpdate();
    CancelPendingFirmware();
   }
  if( !Ok && FFileSystemUnmounted )
    LittleFS.begin();

  if( ProgressRequested )
   {
    if( !FProgressResponseActive )
      BeginProgressResponse();

    if( !Ok )
     {
      SendProgressError();
      EndProgressResponse();
      ResetRequest();
      return;
     }

    if( !FCombinedPackageUpload )
      SendProgressLine( "PROCESS" );
    SendProgressLine( "RESTART" );
    EndProgressResponse();
   }
   else
   {
    if( Ok )
      FHttpServer->client().setNoDelay( true );
    FHttpServer->send( 200, "text/html", CreateResponsePage( Ok, CombinedRequest ) );
   }

  if( !Ok )
   {
    ResetRequest();
    return;
   }

  delay( 500 );
  FHttpServer->client().stop();
  ESP.restart();
 }

String TEspWebUpdate::CreateResponsePage( bool Ok, bool CombinedRequest ) const
 {
  String Page( FPSTR( UpdateResponsePage ) );
  Page.replace( "%CLASS%", Ok ? "Ok" : "Error" );
  Page.replace( "%RESULT%", Ok ? "Update erfolgreich / successful" : "Update fehlgeschlagen / failed" );
  Page.replace( "%ACTION%", Ok ? "location='/Index.html'" : "history.back()" );
  Page.replace( "%BUTTON%", Ok ? "Reload / Neu laden" : "Back / Zurück" );

  String Detail;
  if( Ok )
   {
    if( FFirmwareCompleted && FFileSystemCompleted )
      Detail = F( "Firmware und LittleFS wurden aktualisiert. Neustart... / Firmware and LittleFS updated. Rebooting..." );
     else if( FFirmwareCompleted )
      Detail = F( "Firmware wurde aktualisiert. Neustart... / Firmware updated. Rebooting..." );
     else if( FFileSystemCompleted )
      Detail = F( "LittleFS wurde aktualisiert. Neustart... / LittleFS updated. Rebooting..." );
     else if( FConfigCompleted )
      Detail = F( "Config.json wurde wiederhergestellt. Neustart... / Config.json recovered. Rebooting..." );
     else
      Detail = CombinedRequest
        ? F( "Update package completed. Rebooting..." )
        : F( "Update completed. Rebooting..." );
   }
   else
    Detail = EscapeHtml( FError );
  Page.replace( "%DETAIL%", Detail );
  return Page;
 }

bool TEspWebUpdate::DeleteObsoletePackageFiles(void)
 {
  std::vector<String> ObsoleteFiles;
#ifdef ESP32
  File Directory = LittleFS.open( "/" );
  for( File Entry = Directory.openNextFile(); Entry; Entry = Directory.openNextFile() )
   {
    if( Entry.isDirectory() )
      continue;
    String FileName = Entry.name();
    if( !FileName.startsWith( "/" ) )
      FileName = "/" + FileName;
    if( !IsPackageFileName( FileName ) )
      ObsoleteFiles.push_back( FileName );
   }
#else
  Dir Directory = LittleFS.openDir( "/" );
  while( Directory.next() )
   {
    if( Directory.isDirectory() )
      continue;
    String FileName = Directory.fileName();
    if( !FileName.startsWith( "/" ) )
      FileName = "/" + FileName;
    if( !IsPackageFileName( FileName ) )
      ObsoleteFiles.push_back( FileName );
   }
#endif

  for( String const & FileName : ObsoleteFiles )
    if( !LittleFS.remove( FileName.c_str() ) )
     {
      SetError( String( F( "Could not remove obsolete LittleFS file: " ) ) + FileName );
      return false;
     }
  return true;
 }

bool TEspWebUpdate::EndCombinedPackageUpload(void)
 {
#ifdef ESP8266
  if( !CheckDeviceTypePayloadComplete( TPart::CombinedPackage ) )
   {
    AbortCurrentUpdate();
    return false;
   }
  if( !FCombinedPackageVerificationActive || !FSignatureTail )
   {
    SetError( F( "Combined update signature verification is not active" ) );
    return false;
   }

  if( FSignatureTailSize != FSignatureTailCapacity )
   {
    SetError( F( "Combined update signed file is too short" ) );
    AbortCurrentUpdate();
    return false;
   }
  if( FCombinedPackageState != TCombinedPackageState::FileSystem ||
      FPackageState != TPackageState::Done )
   {
    SetError( F( "Combined update package is incomplete" ) );
    AbortCurrentUpdate();
    return false;
   }

  if( !FSigningHash || !FSigningVerifier )
   {
    SetError( F( "Signed combined update package is unavailable in this build" ) );
    return false;
   }

  const size_t SignatureLength = FSigningVerifier->length();
  uint8_t const * LengthData = FSignatureTail.get() + SignatureLength;
  const uint32_t StoredSignatureLength = ReadUInt32Le( LengthData );
  if( StoredSignatureLength != SignatureLength )
   {
    SetError( F( "Combined update signature length is invalid" ) );
    AbortCurrentUpdate();
    return false;
   }

  FSigningHash->end();
  if( !FSigningVerifier->verify( FSigningHash, FSignatureTail.get(), SignatureLength ) )
   {
    SetError( F( "Combined update signature verification failed" ) );
    AbortCurrentUpdate();
    return false;
   }

  FCombinedPackageVerified = true;
  FCombinedPackageState = TCombinedPackageState::Done;
  ResetSignatureTail();
  return true;
#else
  SetError( F( "Signed combined update package is unavailable in this build" ) );
  return false;
#endif
 }

bool TEspWebUpdate::EndConfigUpload(void)
 {
  if( !CheckDeviceTypePayloadComplete( TPart::Config ) )
   {
    AbortCurrentUpdate();
    return false;
   }
  if( !FConfigVerificationActive )
   {
    SetError( F( "Config recovery verification is not active" ) );
    return false;
   }
  if( FConfigUploadData.length() != FDeviceTypePayloadSize )
   {
    SetError( F( "Config recovery payload size mismatch" ) );
    AbortCurrentUpdate();
    return false;
   }
#ifdef ESP8266
  if( !FSignatureTail || FSignatureTailSize != FSignatureTailCapacity )
   {
    SetError( F( "Config signed file is too short" ) );
    AbortCurrentUpdate();
    return false;
   }
  if( FConfigUploadData.length() != FConfigExpectedPayloadSize )
   {
    SetError( F( "Config upload size does not match selected file" ) );
    AbortCurrentUpdate();
    return false;
   }
  const size_t SignatureLength = FSigningVerifier->length();
  const uint32_t StoredSignatureLength =
    ReadUInt32Le( FSignatureTail.get() + SignatureLength );
  if( StoredSignatureLength != SignatureLength )
   {
    SetError( F( "Config signature length is invalid" ) );
    AbortCurrentUpdate();
    return false;
   }
  FSigningHash->end();
  if( !FSigningVerifier->verify( FSigningHash, FSignatureTail.get(), SignatureLength ) )
   {
    SetError( F( "Config signature verification failed" ) );
    AbortCurrentUpdate();
    return false;
   }
#endif

  JsonDocument Document;
  const DeserializationError JsonError = deserializeJson( Document, FConfigUploadData.c_str(), FConfigUploadData.length() );
  if( JsonError || !Document.is<JsonObject>() )
   {
    SetError( String( F( "Config recovery JSON is invalid: " ) ) + JsonError.c_str() );
    AbortCurrentUpdate();
    return false;
   }

  TGNString Error;
  if( !WriteLittleFSTemporary( "/Config.json", FConfigUploadData, Error ) ||
      !CommitLittleFSTemporary( "/Config.json", Error ) )
   {
    SetError( String( F( "Config recovery write failed: " ) ) + Error.c_str() );
    AbortCurrentUpdate();
    return false;
   }
#ifdef ESP8266
  Update.installSignature( FSigningHash, FSigningVerifier );
#endif
  ResetConfigVerification();
  return true;
 }

bool TEspWebUpdate::EndFileSystemUpload(void)
 {
#ifdef ESP8266
  if( !CheckDeviceTypePayloadComplete( TPart::FileSystem ) )
   {
    AbortCurrentUpdate();
    return false;
   }
  if( !FFileSystemVerificationActive || !FSignatureTail )
   {
    SetError( F( "LittleFS signature verification is not active" ) );
    return false;
   }

  if( FSignatureTailSize != FSignatureTailCapacity )
   {
    SetError( F( "LittleFS signed file is too short" ) );
    AbortCurrentUpdate();
    return false;
   }

  if( !FSigningHash || !FSigningVerifier )
   {
    SetError( F( "Signed LittleFS upload is unavailable in this build" ) );
    return false;
   }

  const size_t SignatureLength = FSigningVerifier->length();
  uint8_t const * LengthData = FSignatureTail.get() + SignatureLength;
  const uint32_t StoredSignatureLength = ReadUInt32Le( LengthData );
  if( StoredSignatureLength != SignatureLength )
   {
    SetError( F( "LittleFS signature length is invalid" ) );
    AbortCurrentUpdate();
    return false;
   }

  if( Update.progress() != GetMaximumSize( TPart::FileSystem ) )
   {
    SetError( F( "LittleFS image size does not match the configured partition" ) );
    AbortCurrentUpdate();
    return false;
   }

  FSigningHash->end();
  if( !FSigningVerifier->verify( FSigningHash, FSignatureTail.get(), SignatureLength ) )
   {
    SetError( F( "LittleFS signature verification failed" ) );
    AbortCurrentUpdate();
    return false;
   }

  if( !Update.end( true ) )
   {
    SetUpdaterError( F( "LittleFS update completion failed" ) );
    AbortCurrentUpdate();
    return false;
   }

  Update.installSignature( FSigningHash, FSigningVerifier );
  ResetFileSystemVerification();
  return true;
#else
  if( !CheckDeviceTypePayloadComplete( TPart::FileSystem ) )
   {
    AbortCurrentUpdate();
    return false;
   }
  if( !FFileSystemVerificationActive || !FUpdatePayloadStarted )
   {
    SetError( F( "LittleFS recovery upload is not active" ) );
    return false;
   }
  if( Update.progress() != FDeviceTypePayloadSize )
   {
    SetError( F( "LittleFS image size does not match DeviceType payload" ) );
    AbortCurrentUpdate();
    return false;
   }
  if( !Update.end( true ) )
   {
    SetUpdaterError( F( "LittleFS update completion failed" ) );
    AbortCurrentUpdate();
    return false;
   }
  ResetFileSystemVerification();
  return true;
#endif
 }

bool TEspWebUpdate::EndFirmwareUpload(void)
 {
  if( !CheckDeviceTypePayloadComplete( TPart::Firmware ) )
   {
    AbortCurrentUpdate();
    return false;
   }
#ifdef ESP8266
  if( !FFirmwareVerificationActive || !FSignatureTail )
   {
    SetError( F( "Firmware signature verification is not active" ) );
    return false;
   }

  if( FSignatureTailSize != FSignatureTailCapacity )
   {
    SetError( F( "Firmware signed file is too short" ) );
    AbortCurrentUpdate();
    return false;
   }

  if( !FSigningHash || !FSigningVerifier )
   {
    SetError( F( "Signed firmware upload is unavailable in this build" ) );
    return false;
   }

  const size_t SignatureLength = FSigningVerifier->length();
  uint8_t const * LengthData = FSignatureTail.get() + SignatureLength;
  const uint32_t StoredSignatureLength = ReadUInt32Le( LengthData );
  if( StoredSignatureLength != SignatureLength )
   {
    SetError( F( "Firmware signature length is invalid" ) );
    AbortCurrentUpdate();
    return false;
   }

  if( !FFirmwarePayloadSize )
   {
    SetError( F( "Firmware payload is empty" ) );
    AbortCurrentUpdate();
    return false;
   }

  if( FFirmwarePayloadSize != FFirmwareExpectedPayloadSize )
   {
    SetError( F( "Firmware upload size does not match selected file" ) );
    AbortCurrentUpdate();
    return false;
   }

  FSigningHash->end();
  if( !FSigningVerifier->verify( FSigningHash, FSignatureTail.get(), SignatureLength ) )
   {
    SetError( F( "Firmware signature verification failed" ) );
    AbortCurrentUpdate();
    return false;
   }
#endif

  if( !Update.end( true ) )
   {
    SetUpdaterError( F( "Firmware update completion failed" ) );
    AbortCurrentUpdate();
    return false;
   }

#ifdef ESP8266
  Update.installSignature( FSigningHash, FSigningVerifier );
  ResetFirmwareVerification();
#endif
  return true;
 }

void TEspWebUpdate::EndProgressResponse(void)
 {
  if( !FProgressResponseActive )
    return;

  FHttpServer->sendContent( "" );
  FProgressResponseActive = false;
 }

String TEspWebUpdate::EscapeHtml( String const & Text )
 {
  String Result( Text );
  Result.replace( "&", "&amp;" );
  Result.replace( "<", "&lt;" );
  Result.replace( ">", "&gt;" );
  Result.replace( "\"", "&quot;" );
  return Result;
 }

size_t TEspWebUpdate::GetMaximumSize( TPart Part ) const
 {
#ifdef ESP32
  (void)Part;
  return UPDATE_SIZE_UNKNOWN;
#else
  if( Part == TPart::FileSystem )
    return size_t( &_FS_end ) - size_t( &_FS_start );
  return ( ESP.getFreeSketchSpace() - 0x1000 ) & 0xFFFFF000;
#endif
 }

String TEspWebUpdate::GetTemporaryPackageFileName( String const & FileName ) const
 {
  return FileName + PackageTemporarySuffix;
 }

bool TEspWebUpdate::IsPackageFileName( String const & FileName ) const
 {
  for( TPackageEntry const & Entry : FPackageEntries )
    if( Entry.Name == FileName )
      return true;
  return false;
 }

void TEspWebUpdate::HandleUpload( bool CombinedRequest )
 {
  HTTPUpload & Upload = FHttpServer->upload();

  if( Upload.status == UPLOAD_FILE_START )
    StartUpload( Upload, CombinedRequest );
   else if( Upload.status == UPLOAD_FILE_WRITE && !FError.length() )
   {
    if( FCurrentPart == TPart::CombinedPackage )
      WriteCombinedPackageData( Upload.buf, Upload.currentSize );
     else if( FCurrentPart == TPart::FileSystem )
      WriteFileSystemData( Upload.buf, Upload.currentSize );
     else if( FCurrentPart == TPart::Config )
      WriteConfigData( Upload.buf, Upload.currentSize );
     else
      WriteFirmwareData( Upload.buf, Upload.currentSize );
   }
   else if( Upload.status == UPLOAD_FILE_END && !FError.length() )
   {
    if( FCurrentPart == TPart::CombinedPackage )
      EndCombinedPackageUpload();
     else if( FCurrentPart == TPart::FileSystem )
     {
      if( EndFileSystemUpload() )
        FFileSystemCompleted = true;
     }
     else if( FCurrentPart == TPart::Config )
     {
      if( EndConfigUpload() )
        FConfigCompleted = true;
     }
     else if( EndFirmwareUpload() )
     {
      FFirmwareCompleted = true;
      FFirmwareStaged = true;
     }
    FCurrentPart = TPart::None;
   }
   else if( Upload.status == UPLOAD_FILE_ABORTED )
   {
    SetError( F( "Upload aborted" ) );
    AbortCurrentUpdate();
    FCurrentPart = TPart::None;
   }
  delay( 0 );
 }

bool TEspWebUpdate::ProcessCombinedPackagePayload( uint8_t const * Data, size_t Size )
 {
  while( Size && !FError.length() )
   {
    if( FCombinedPackageState == TCombinedPackageState::Header )
     {
      const size_t CopySize = std::min( Size,
        sizeof( FCombinedPackageHeader ) - FCombinedPackageHeaderSize );
      memcpy( FCombinedPackageHeader + FCombinedPackageHeaderSize, Data, CopySize );
      FCombinedPackageHeaderSize += CopySize;
      Data += CopySize;
      Size -= CopySize;
      if( FCombinedPackageHeaderSize != sizeof( FCombinedPackageHeader ) )
        continue;

      if( memcmp( FCombinedPackageHeader, CombinedPackageMagic,
          sizeof( CombinedPackageMagic ) ) != 0 )
       {
        SetError( F( "Invalid combined update package header" ) );
        return false;
       }

      FCombinedFirmwareRemaining = ReadUInt32Le( FCombinedPackageHeader + 8 );
      const size_t MaximumFirmwareSize = GetMaximumSize( TPart::Firmware );
      if( !FCombinedFirmwareRemaining )
       {
        SetError( F( "Combined update package has an invalid firmware size" ) );
        return false;
       }
      if( FCombinedFirmwareRemaining >= MaximumFirmwareSize )
       {
        SetError( F( "Firmware package does not fit into available OTA space. Update via Rescue firmware." ) );
        return false;
       }

      // Keep one byte deliberately unwritten. Thus Update.end() without the
      // evenIfRemaining flag can only abort, never stage an unauthenticated
      // package firmware if the combined upload fails before signature check.
      if( !Update.begin( size_t( FCombinedFirmwareRemaining ) + 1, U_FLASH ) )
       {
        SetUpdaterError( F( "Firmware package update start failed" ) );
        return false;
       }
      FCombinedPackageState = TCombinedPackageState::Firmware;
      continue;
     }

    if( FCombinedPackageState == TCombinedPackageState::Firmware )
     {
      const size_t WriteSize = std::min( Size, size_t( FCombinedFirmwareRemaining ) );
      if( Update.write( const_cast<uint8_t *>( Data ), WriteSize ) != WriteSize )
       {
        SetUpdaterError( F( "Firmware package write failed" ) );
        return false;
       }
      Data += WriteSize;
      Size -= WriteSize;
      FCombinedFirmwareRemaining -= WriteSize;
      if( FCombinedFirmwareRemaining )
        continue;

      ResetPackageParser();
      FCombinedPackageState = TCombinedPackageState::FileSystem;
      continue;
     }

    if( FCombinedPackageState == TCombinedPackageState::FileSystem )
     {
      if( !ProcessPackageData( Data, Size ) )
        return false;
      Size = 0;
      continue;
     }

    SetError( F( "Unexpected trailing data in combined update package" ) );
    return false;
   }
  return !FError.length();
 }

bool TEspWebUpdate::ProcessPackageData( uint8_t const * Data, size_t Size )
 {
  while( Size && !FError.length() )
   {
    if( FPackageState == TPackageState::Header )
     {
      const size_t CopySize = std::min( Size, sizeof( FPackageHeader ) - FPackageHeaderSize );
      memcpy( FPackageHeader + FPackageHeaderSize, Data, CopySize );
      FPackageHeaderSize += CopySize;
      Data += CopySize;
      Size -= CopySize;
      if( FPackageHeaderSize != sizeof( FPackageHeader ) )
        continue;

      if( memcmp( FPackageHeader, PackageMagic, sizeof( PackageMagic ) ) != 0 )
       {
        SetError( F( "Invalid compact LittleFS update package header" ) );
        return false;
       }
      if( ReadUInt32Le( FPackageHeader + 8 ) != GetMaximumSize( TPart::FileSystem ) )
       {
        SetError( F( "Compact LittleFS update package has the wrong filesystem size" ) );
        return false;
       }

      FPackageFileCount = ReadUInt16Le( FPackageHeader + 12 );
      FPackageTotalDataLength = ReadUInt32Le( FPackageHeader + 14 );
      if( FPackageFileCount > PackageMaximumFileCount )
       {
        SetError( F( "Compact LittleFS update package contains too many files" ) );
        return false;
       }
      FPackageEntries.reserve( FPackageFileCount );
      FPackageState = FPackageFileCount
        ? TPackageState::TableFileHeader
        : TPackageState::CompareData;
      if( !FPackageFileCount )
       {
        if( FPackageTotalDataLength )
         {
          SetError( F( "Compact LittleFS update package has data without files" ) );
          return false;
         }
        FPackageDataIndex = 0;
        if( !StartPackageCompareEntry() )
          return false;
       }
      continue;
     }

    if( FPackageState == TPackageState::TableFileHeader )
     {
      const size_t CopySize = std::min( Size,
        sizeof( FPackageFileHeader ) - FPackageFileHeaderSize );
      memcpy( FPackageFileHeader + FPackageFileHeaderSize, Data, CopySize );
      FPackageFileHeaderSize += CopySize;
      Data += CopySize;
      Size -= CopySize;
      if( FPackageFileHeaderSize != sizeof( FPackageFileHeader ) )
        continue;

      FPackagePathLength = FPackageFileHeader[0];
      if( FPackagePathLength < 2 || FPackagePathLength >= sizeof( FPackagePath ) )
       {
        SetError( F( "Invalid path length in compact LittleFS update package" ) );
        return false;
       }
      FPackagePathSize = 0;
      FPackageState = TPackageState::TablePath;
      continue;
     }

    if( FPackageState == TPackageState::TablePath )
     {
      const size_t CopySize = std::min( Size, FPackagePathLength - FPackagePathSize );
      memcpy( FPackagePath + FPackagePathSize, Data, CopySize );
      FPackagePathSize += CopySize;
      Data += CopySize;
      Size -= CopySize;
      if( FPackagePathSize != FPackagePathLength )
        continue;

      if( memchr( FPackagePath, 0, FPackagePathLength ) )
       {
        SetError( F( "NUL character in compact LittleFS update package path" ) );
        return false;
       }
      FPackagePath[FPackagePathLength] = 0;
      String const FileName( FPackagePath );
      if( FPackagePath[0] != '/' || strchr( FPackagePath + 1, '/' ) )
       {
        SetError( String( F( "Invalid compact LittleFS update path: " ) ) + FileName );
        return false;
       }
      if( FileName.endsWith( PackageTemporarySuffix ) )
       {
        SetError( String( F( "Reserved compact LittleFS update path: " ) ) + FileName );
        return false;
       }
      if( FileName.length() + strlen( PackageTemporarySuffix ) >= sizeof( FPackagePath ) )
       {
        SetError( String( F( "Compact LittleFS update path is too long for staging: " ) ) + FileName );
        return false;
       }
      for( TPackageEntry const & Entry : FPackageEntries )
        if( Entry.Name == FileName )
         {
          SetError( String( F( "Duplicate compact LittleFS update path: " ) ) + FileName );
          return false;
         }

      TPackageEntry Entry;
      Entry.Name = FileName;
      Entry.DataSize = ReadUInt32Le( FPackageFileHeader + 1 );
      Entry.DataWritten = 0;
      Entry.RamOffset = 0;
      Entry.TemporaryDataSize = 0;
      Entry.Identical = false;
      FPackageEntries.push_back( Entry );

      ++FPackageTableIndex;
      FPackageFileHeaderSize = 0;
      FPackagePathLength = 0;
      FPackagePathSize = 0;
      if( FPackageTableIndex != FPackageFileCount )
       {
        FPackageState = TPackageState::TableFileHeader;
        continue;
       }

      uint64_t TableDataLength = 0;
      for( TPackageEntry const & PackageEntry : FPackageEntries )
        TableDataLength += PackageEntry.DataSize;
      if( TableDataLength != FPackageTotalDataLength )
       {
        SetError( F( "Compact LittleFS update data length does not match its file table" ) );
        return false;
       }

      FPackageDataIndex = 0;
      FPackageState = TPackageState::CompareData;
      if( !StartPackageCompareEntry() )
        return false;
      continue;
     }

    if( FPackageState == TPackageState::CompareData )
     {
      if( FPackageDataIndex >= FPackageEntries.size() )
       {
        SetError( F( "Compact LittleFS update data exceeds its file table" ) );
        return false;
       }

      TPackageEntry & Entry = FPackageEntries[FPackageDataIndex];
      const size_t Remaining = size_t( Entry.DataSize - Entry.DataWritten );
      const size_t CompareSize = std::min( Size, Remaining );
      if( Entry.Identical )
       {
        uint8_t CompareBuffer[PackageCompareChunkSize];
        size_t Compared = 0;
        while( Compared < CompareSize )
         {
          const size_t ChunkSize = std::min( CompareSize - Compared,
            sizeof( CompareBuffer ) );
          if( FPackageTargetFile.read( CompareBuffer, ChunkSize ) != ChunkSize ||
              memcmp( CompareBuffer, Data + Compared, ChunkSize ) != 0 )
           {
            Entry.Identical = false;
            FPackageTargetFile.close();
            break;
           }
          Compared += ChunkSize;
         }
       }

      Data += CompareSize;
      Size -= CompareSize;
      Entry.DataWritten += CompareSize;
      FPackageReceivedDataLength += CompareSize;
      if( Entry.DataWritten == Entry.DataSize )
       {
        if( FPackageTargetFile )
          FPackageTargetFile.close();
        ++FPackageDataIndex;
        if( !StartPackageCompareEntry() )
          return false;
       }
      continue;
     }

    if( FPackageState == TPackageState::StageData )
     {
      if( FPackageDataIndex >= FPackageEntries.size() )
       {
        SetError( F( "Compact LittleFS update data exceeds its file table" ) );
        return false;
       }

      TPackageEntry & Entry = FPackageEntries[FPackageDataIndex];
      const size_t Remaining = size_t( Entry.DataSize - Entry.DataWritten );
      const size_t WriteSize = std::min( Size, Remaining );
      size_t Consumed = 0;
      if( !Entry.Identical && Entry.DataWritten < Entry.TemporaryDataSize )
       {
        const size_t TemporaryWriteSize = std::min( WriteSize,
          Entry.TemporaryDataSize - size_t( Entry.DataWritten ) );
        if( FPackageTargetFile.write( Data, TemporaryWriteSize ) != TemporaryWriteSize )
         {
          SetError( String( F( "Temporary LittleFS update write failed: " ) ) + Entry.Name );
          return false;
         }
        Consumed = TemporaryWriteSize;
       }

      const size_t RamWriteSize = Entry.Identical ? 0 : WriteSize - Consumed;
      if( RamWriteSize )
       {
        const size_t RamEntryOffset = size_t( Entry.DataWritten ) + Consumed -
          Entry.TemporaryDataSize;
        if( !CopyToPackageRam( Entry.RamOffset + RamEntryOffset,
            Data + Consumed, RamWriteSize ) )
          return false;
       }

      Data += WriteSize;
      Size -= WriteSize;
      Entry.DataWritten += WriteSize;
      FPackageReceivedDataLength += WriteSize;
      if( Entry.DataWritten == Entry.DataSize )
       {
        if( FPackageTargetFile )
          FPackageTargetFile.close();
        ++FPackageDataIndex;
        if( !StartPackageStageEntry() )
          return false;
       }
      continue;
     }

    SetError( F( "Unexpected trailing data in compact LittleFS update package" ) );
    return false;
   }
  return !FError.length();
 }

void TEspWebUpdate::ReportPackageFileCommitted(void)
 {
  ++FPackageCommittedFileCount;
  SendProgressLine( String( F( "FILES " ) ) +
    String( unsigned( FPackageCommittedFileCount ) ) + " " +
    String( unsigned( FPackageEntries.size() ) ) );
 }

void TEspWebUpdate::ResetCombinedPackageVerification(void)
 {
  FCombinedPackageVerificationActive = false;
  FCombinedPackageVerified = false;
  FCombinedPackageState = TCombinedPackageState::Header;
  FCombinedPackageHeaderSize = 0;
  FCombinedFirmwareRemaining = 0;
  ResetSignatureTail();
  ResetPackageParser();
 }

void TEspWebUpdate::ResetFileSystemVerification(void)
 {
  FFileSystemVerificationActive = false;
  ResetSignatureTail();
 }

void TEspWebUpdate::ResetFirmwareVerification(void)
 {
  FFirmwareVerificationActive = false;
  FFirmwarePayloadSize = 0;
  FFirmwareExpectedPayloadSize = 0;
  ResetSignatureTail();
 }

void TEspWebUpdate::ResetPackageParser(void)
 {
  if( FPackageTargetFile )
    FPackageTargetFile.close();
  FPackageState = TPackageState::Header;
  FPackageHeaderSize = 0;
  FPackageFileHeaderSize = 0;
  FPackagePathLength = 0;
  FPackagePathSize = 0;
  FPackageTotalDataLength = 0;
  FPackageReceivedDataLength = 0;
  FPackageFileCount = 0;
  FPackageTableIndex = 0;
  FPackageDataIndex = 0;
  FPackageChangedFileCount = 0;
  FPackageDeletedFileCount = 0;
  FPackageCommittedFileCount = 0;
  FPackageEntries.clear();
  ResetPackageRam();
 }

void TEspWebUpdate::ResetPackageRam(void)
 {
  FPackageRamChunks.clear();
  FPackageRamDataSize = 0;
 }

bool TEspWebUpdate::WritePackageRamToFile( File & TargetFile, size_t Offset, size_t Size )
 {
  if( Offset > FPackageRamDataSize || Size > FPackageRamDataSize - Offset )
    return false;

  while( Size )
   {
    const size_t ChunkIndex = Offset / PackageRamChunkSize;
    const size_t ChunkOffset = Offset % PackageRamChunkSize;
    if( ChunkIndex >= FPackageRamChunks.size() || !FPackageRamChunks[ChunkIndex] )
      return false;

    const size_t WriteSize = std::min( Size, PackageRamChunkSize - ChunkOffset );
    if( TargetFile.write( FPackageRamChunks[ChunkIndex].get() + ChunkOffset, WriteSize ) != WriteSize )
      return false;
    Offset += WriteSize;
    Size -= WriteSize;
   }
  return true;
 }

void TEspWebUpdate::ResetRequest(void)
 {
  AbortCurrentUpdate();
  FError.clear();
  FCurrentPart = TPart::None;
  FCombinedRequest = false;
  FCombinedPackageUpload = false;
  FConfigCompleted = false;
  FFileSystemCompleted = false;
  FFileSystemUnmounted = false;
  FFirmwareCompleted = false;
  FFirmwareStaged = false;
  FProgressResponseActive = false;
  FRequestActive = false;
  FUploadedFileCount = 0;
  ResetCombinedPackageVerification();
  ResetConfigVerification();
  ResetDeviceTypeHeader();
 }

void TEspWebUpdate::ResetSignatureTail(void)
 {
  FSignatureTail.reset();
  FSignatureTailCapacity = 0;
  FSignatureTailSize = 0;
 }

void TEspWebUpdate::SetError( String const & Error )
 {
  if( !FError.length() )
    FError = Error;
 }

void TEspWebUpdate::SetError( __FlashStringHelper const * Error )
 {
  if( !FError.length() )
    FError = Error;
 }

void TEspWebUpdate::SetUpdaterError( String const & Prefix )
 {
  const uint8_t UpdateError = Update.getError();
  String Error = GetUpdaterErrorString();
  if( UpdateError == UPDATE_ERROR_SPACE )
    Error += F( ". Update via Rescue firmware." );
  String Message = Prefix;
  Message += F( ": " );
  Message += Error;
  SetError( Message );
 }

void TEspWebUpdate::SetUpdaterError( __FlashStringHelper const * Prefix )
 {
  SetUpdaterError( String( Prefix ) );
 }

void TEspWebUpdate::SendProgressError(void)
 {
  String Error = FError.length() ? FError : String( F( "Unknown update error" ) );
  Error.replace( "\r", " " );
  Error.replace( "\n", " " );
  SendProgressLine( String( F( "ERROR " ) ) + Error );
 }

void TEspWebUpdate::SendProgressLine( String const & Line )
 {
  if( !FProgressResponseActive || !FHttpServer->client().connected() )
    return;

  FHttpServer->sendContent( Line + "\n" );
  delay( 0 );
 }

void TEspWebUpdate::SendRecoveryPage(void)
 {
#ifdef ESP32
  String Page = F( "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
                   "<title>Recovery</title><script>function R(f){const a=f.querySelector('[name=firmware]'),b=f.querySelector('[name=filesystem]');"
                   "if(!a.files.length&&!b.files.length){alert('Select firmware and/or LittleFS.');return false;}"
                   "if(!a.files.length)a.disabled=true;if(!b.files.length)b.disabled=true;return true;}</script></head>"
                   "<body><h1>%PROJECT% Recovery</h1><div>%NETWORK%</div>"
                   "<p>Startup recovery mode. Only DeviceType-matching recovery artifacts are accepted. "
                   "Firmware and LittleFS can be recovered together and cause only one restart.</p>"
                   "<form method='POST' enctype='multipart/form-data' action='/CombinedUpdate/' onsubmit='return R(this)'>"
                   "<b>Firmware + LittleFS</b><br>Firmware:<br><input type='file' accept='.bin' name='firmware'><br>"
                   "LittleFS:<br><input type='file' accept='.littlefs' name='filesystem'><br>"
                   "<input type='submit' value='Recover firmware / LittleFS'></form><br>"
                   "<form method='POST' enctype='multipart/form-data' action='/FwUpdate/'><b>Config.json</b><br>"
                   "<input type='file' accept='.update' name='config' required><input type='submit' value='Recover Config.json'></form>"
                   "</body></html>" );
#else
  String Page( FPSTR( RecoveryPage ) );
#endif
  String Network;
  const WiFiMode_t Mode = WiFi.getMode();
  if( ( Mode == WIFI_STA || Mode == WIFI_AP_STA ) && WiFi.status() == WL_CONNECTED )
   {
    Network = F( "STA: " );
    Network += WiFi.localIP().toString();
   }
  if( Mode == WIFI_AP || Mode == WIFI_AP_STA )
   {
    if( Network.length() )
      Network += F( " &middot; " );
    Network += F( "AP: " );
    Network += WiFi.softAPIP().toString();
   }
  if( !Network.length() )
    Network = F( "WLAN connection is starting" );

  Page.replace( "%PROJECT%", EscapeHtml( FRescueProjectName ) );
  Page.replace( "%NETWORK%", Network );
  FHttpServer->send( 200, "text/html", Page );
 }

void TEspWebUpdate::SendStartupErrorPage(void)
 {
#ifdef ESP32
  String Page = F( "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
                   "<title>Startup error</title></head><body><h1>%PROJECT% Startup error</h1>"
                   "<p><b>%ERROR%</b></p><p>Upload an ESP32 application firmware. Bootloader and partition table remain unchanged.</p>"
                   "<form method='POST' enctype='multipart/form-data' action='/FwUpdate/'>"
                   "<input type='file' accept='.bin' name='firmware' required>"
                   "<input type='submit' value='Update firmware'></form></body></html>" );
#else
  String Page( FPSTR( StartupErrorPage ) );
#endif
  Page.replace( "%PROJECT%", EscapeHtml( FRescueProjectName ) );
  Page.replace( "%ERROR%", EscapeHtml( FStartupError ) );
  FHttpServer->send( 200, "text/html", Page );
 }

bool TEspWebUpdate::StartPackageCompareEntry(void)
 {
  while( FPackageDataIndex < FPackageEntries.size() )
   {
    TPackageEntry & Entry = FPackageEntries[FPackageDataIndex];
    Entry.DataWritten = 0;

    Entry.Identical = false;
    if( LittleFS.exists( Entry.Name.c_str() ) )
     {
      FPackageTargetFile = LittleFS.open( Entry.Name.c_str(), "r" );
      if( FPackageTargetFile && FPackageTargetFile.size() == Entry.DataSize )
        Entry.Identical = true;
       else if( FPackageTargetFile )
        FPackageTargetFile.close();
     }

    if( !Entry.DataSize )
     {
      if( FPackageTargetFile )
        FPackageTargetFile.close();
      ++FPackageDataIndex;
      continue;
     }

    FPackageState = TPackageState::CompareData;
    return true;
   }

  return CompletePackageComparePass();
 }

bool TEspWebUpdate::StartPackageStageEntry(void)
 {
  while( FPackageDataIndex < FPackageEntries.size() )
   {
    TPackageEntry & Entry = FPackageEntries[FPackageDataIndex];
    Entry.DataWritten = 0;
    if( !Entry.DataSize )
     {
      ++FPackageDataIndex;
      continue;
     }

    if( !Entry.Identical && Entry.TemporaryDataSize )
     {
      String const TemporaryName = GetTemporaryPackageFileName( Entry.Name );
      FPackageTargetFile = LittleFS.open( TemporaryName.c_str(), "w" );
      if( !FPackageTargetFile )
       {
        SetError( String( F( "Could not open temporary LittleFS update file: " ) ) + TemporaryName );
        return false;
       }
     }
    FPackageState = TPackageState::StageData;
    return true;
   }

  if( FPackageReceivedDataLength != FPackageTotalDataLength )
   {
    SetError( F( "Compact LittleFS update ended before all file data was received" ) );
    return false;
   }
  FPackageState = TPackageState::Done;
  return true;
 }

void TEspWebUpdate::StartUpload( HTTPUpload const & Upload, bool CombinedRequest )
 {
  BeginRequest( CombinedRequest );
  if( FError.length() )
    return;
  ++FUploadedFileCount;

  TPart Part = TPart::None;
  if( Upload.name == "update" )
    Part = TPart::CombinedPackage;
   else if( Upload.name == "firmware" )
    Part = TPart::Firmware;
   else if( Upload.name == "filesystem" )
    Part = TPart::FileSystem;
   else if( Upload.name == "config" )
    Part = TPart::Config;
   else
   {
    SetError( String( F( "Unknown upload field: " ) ) + Upload.name );
    return;
   }

  if( FRecoveryMode && CombinedRequest &&
      Part != TPart::Firmware && Part != TPart::FileSystem )
   {
    SetError( F( "Recovery combined update accepts only firmware and LittleFS" ) );
    return;
   }
  if( !FRecoveryMode && Part == TPart::Config )
   {
    SetError( F( "Config recovery upload is available only in startup recovery mode" ) );
    return;
   }

  if( !CheckFileName( Upload.filename, Part ) )
   {
    SetError( String( F( "Invalid file name: " ) ) + Upload.filename );
    return;
   }

  if( Part == TPart::CombinedPackage )
   {
    if( !CombinedRequest )
     {
      SetError( F( "Combined update package requires the combined-update route" ) );
      return;
     }
    if( FUploadedFileCount != 1 )
     {
      SetError( F( "Combined update package must be the only upload file" ) );
      return;
     }

    FCombinedPackageUpload = true;
    FCurrentPart = Part;
    BeginCombinedPackageUpload();
    return;
   }

  if( FCombinedPackageUpload )
   {
    SetError( F( "Combined update package cannot be mixed with separate files" ) );
    return;
   }

  if( CombinedRequest )
   {
    if( Part == TPart::Firmware )
     {
      if( FFirmwareCompleted || FFileSystemCompleted || FUploadedFileCount != 1 )
       {
        SetError( F( "Firmware must be the first combined-update file" ) );
        return;
       }
     }
     else if( Part == TPart::FileSystem )
     {
      if( FFileSystemCompleted || FUploadedFileCount > 2 ||
          ( FUploadedFileCount == 2 && !FFirmwareCompleted ) )
       {
        SetError( F( "LittleFS must be the only file or follow a completed firmware upload" ) );
        return;
       }
     }
   }

  FCurrentPart = Part;
  if( Part == TPart::FileSystem )
   {
    BeginFileSystemUpload();
    return;
   }
  if( Part == TPart::Config )
   {
#ifdef ESP8266
    if( !FHttpServer->hasArg( "_EspConfigSize" ) )
     {
      SetError( F( "Config upload size is missing" ) );
      return;
     }
    const String ConfigSizeText = FHttpServer->arg( "_EspConfigSize" );
    char * ConfigEnd = nullptr;
    const unsigned long ConfigSize = std::strtoul( ConfigSizeText.c_str(), &ConfigEnd, 10 );
    if( !ConfigSize || !ConfigEnd || *ConfigEnd )
     {
      SetError( F( "Config upload size is invalid" ) );
      return;
     }
    BeginConfigUpload( size_t( ConfigSize ) );
#else
    BeginConfigUpload( 0 );
#endif
    return;
   }

#ifdef ESP8266
  if( !FHttpServer->hasArg( "_EspFirmwareSize" ) )
   {
    SetError( F( "Firmware upload size is missing" ) );
    return;
   }

  const String FirmwareSizeText = FHttpServer->arg( "_EspFirmwareSize" );
  char * End = nullptr;
  const unsigned long FirmwareSize = std::strtoul( FirmwareSizeText.c_str(), &End, 10 );
  if( !FirmwareSize || !End || *End )
   {
    SetError( F( "Firmware upload size is invalid" ) );
    return;
   }
  BeginFirmwareUpload( size_t( FirmwareSize ) );
#else
  BeginFirmwareUpload( 0 );
#endif
 }

bool TEspWebUpdate::WriteConfigData( uint8_t const * Data, size_t Size )
 {
  if( !FConfigVerificationActive )
   {
    SetError( F( "Config recovery verification is not active" ) );
    return false;
   }
#ifdef ESP8266
  if( !FSignatureTail )
   {
    SetError( F( "Config signature verification is not active" ) );
    return false;
   }
  const size_t CombinedSize = FSignatureTailSize + Size;
  if( CombinedSize <= FSignatureTailCapacity )
   {
    memcpy( FSignatureTail.get() + FSignatureTailSize, Data, Size );
    FSignatureTailSize += Size;
    return true;
   }

  size_t FlushSize = CombinedSize - FSignatureTailCapacity;
  const size_t TailFlushSize = std::min( FlushSize, FSignatureTailSize );
  if( TailFlushSize && !WriteConfigPayload( FSignatureTail.get(), TailFlushSize ) )
    return false;
  if( TailFlushSize < FSignatureTailSize )
    memmove( FSignatureTail.get(), FSignatureTail.get() + TailFlushSize,
      FSignatureTailSize - TailFlushSize );
  FSignatureTailSize -= TailFlushSize;
  FlushSize -= TailFlushSize;

  if( FlushSize && !WriteConfigPayload( Data, FlushSize ) )
    return false;
  Data += FlushSize;
  Size -= FlushSize;
  memcpy( FSignatureTail.get() + FSignatureTailSize, Data, Size );
  FSignatureTailSize += Size;
  return true;
#else
  return WriteConfigPayload( Data, Size );
#endif
 }

bool TEspWebUpdate::WriteConfigPayload( uint8_t const * Data, size_t Size )
 {
#ifdef ESP8266
  if( FSigningHash )
    FSigningHash->add( Data, Size );
#endif
  if( !ConsumeDeviceTypeHeader( TPart::Config, Data, Size ) )
    return false;
  if( !FDeviceTypeHeaderVerified )
    return true;
  if( FDeviceTypePayloadSize > MaximumConfigPayloadSize )
   {
    SetError( F( "Config recovery payload is too large" ) );
    return false;
   }
  if( Size > FDeviceTypePayloadRemaining )
   {
    SetError( F( "Config recovery upload exceeds DeviceType payload size" ) );
    return false;
   }
  if( FConfigUploadData.length() == 0 && !GnReserveStringExact( FConfigUploadData, FDeviceTypePayloadSize ) )
   {
    SetError( F( "Not enough memory for Config recovery payload" ) );
    return false;
   }
  if( Size && !FConfigUploadData.concat( reinterpret_cast<char const *>( Data ), Size ) )
   {
    SetError( F( "Not enough memory for Config recovery payload" ) );
    return false;
   }
  FDeviceTypePayloadRemaining -= Size;
  return true;
 }

bool TEspWebUpdate::WriteCombinedPackageData( uint8_t const * Data, size_t Size )
 {
  if( !FCombinedPackageVerificationActive || !FSignatureTail )
   {
    SetError( F( "Combined update signature verification is not active" ) );
    return false;
   }

  const size_t CombinedSize = FSignatureTailSize + Size;
  if( CombinedSize <= FSignatureTailCapacity )
   {
    memcpy( FSignatureTail.get() + FSignatureTailSize, Data, Size );
    FSignatureTailSize += Size;
    return true;
   }

  size_t FlushSize = CombinedSize - FSignatureTailCapacity;
  const size_t TailFlushSize = std::min( FlushSize, FSignatureTailSize );
  if( TailFlushSize && !WriteCombinedPackagePayload( FSignatureTail.get(), TailFlushSize ) )
    return false;

  if( TailFlushSize < FSignatureTailSize )
    memmove( FSignatureTail.get(), FSignatureTail.get() + TailFlushSize,
      FSignatureTailSize - TailFlushSize );
  FSignatureTailSize -= TailFlushSize;
  FlushSize -= TailFlushSize;

  if( FlushSize && !WriteCombinedPackagePayload( Data, FlushSize ) )
    return false;
  Data += FlushSize;
  Size -= FlushSize;

  memcpy( FSignatureTail.get() + FSignatureTailSize, Data, Size );
  FSignatureTailSize += Size;
  return true;
 }

bool TEspWebUpdate::WriteCombinedPackagePayload( uint8_t const * Data, size_t Size )
 {
#ifdef ESP8266
  if( FSigningHash )
    FSigningHash->add( Data, Size );
#endif

  if( !ConsumeDeviceTypeHeader( TPart::CombinedPackage, Data, Size ) )
    return false;
  if( !FDeviceTypeHeaderVerified || !Size )
    return true;
  if( Size > FDeviceTypePayloadRemaining )
   {
    SetError( F( "Combined update exceeds DeviceType payload size" ) );
    return false;
   }

  if( !ProcessCombinedPackagePayload( Data, Size ) )
    return false;
  FDeviceTypePayloadRemaining -= Size;
  return true;
 }

bool TEspWebUpdate::WriteFileSystemData( uint8_t const * Data, size_t Size )
 {
#ifdef ESP8266
  if( !FFileSystemVerificationActive || !FSignatureTail )
   {
    SetError( F( "LittleFS signature verification is not active" ) );
    return false;
   }

  const size_t CombinedSize = FSignatureTailSize + Size;
  if( CombinedSize <= FSignatureTailCapacity )
   {
    memcpy( FSignatureTail.get() + FSignatureTailSize, Data, Size );
    FSignatureTailSize += Size;
    return true;
   }

  size_t FlushSize = CombinedSize - FSignatureTailCapacity;
  const size_t TailFlushSize = std::min( FlushSize, FSignatureTailSize );
  if( TailFlushSize && !WriteFileSystemPayload( FSignatureTail.get(), TailFlushSize ) )
    return false;

  if( TailFlushSize < FSignatureTailSize )
    memmove( FSignatureTail.get(), FSignatureTail.get() + TailFlushSize,
      FSignatureTailSize - TailFlushSize );
  FSignatureTailSize -= TailFlushSize;
  FlushSize -= TailFlushSize;

  if( FlushSize && !WriteFileSystemPayload( Data, FlushSize ) )
    return false;
  Data += FlushSize;
  Size -= FlushSize;

  memcpy( FSignatureTail.get() + FSignatureTailSize, Data, Size );
  FSignatureTailSize += Size;
  return true;
#else
  return WriteFileSystemPayload( Data, Size );
#endif
 }

bool TEspWebUpdate::WriteFileSystemPayload( uint8_t const * Data, size_t Size )
 {
#ifdef ESP8266
  if( FSigningHash )
    FSigningHash->add( Data, Size );
#endif

  if( !ConsumeDeviceTypeHeader( TPart::FileSystem, Data, Size ) )
    return false;
  if( !FDeviceTypeHeaderVerified )
    return true;
  if( Size > FDeviceTypePayloadRemaining )
   {
    SetError( F( "LittleFS update exceeds DeviceType payload size" ) );
    return false;
   }
  if( !Size )
    return true;
  if( !FUpdatePayloadStarted && !BeginFileSystemPayloadUpload() )
    return false;

  if( Update.write( const_cast<uint8_t *>( Data ), Size ) != Size )
   {
    SetUpdaterError( F( "LittleFS write failed" ) );
    return false;
   }
  FDeviceTypePayloadRemaining -= Size;
  return true;
 }

bool TEspWebUpdate::WriteFirmwareData( uint8_t const * Data, size_t Size )
 {
#ifdef ESP8266
  if( !FFirmwareVerificationActive || !FSignatureTail )
   {
    SetError( F( "Firmware signature verification is not active" ) );
    return false;
   }

  const size_t CombinedSize = FSignatureTailSize + Size;
  if( CombinedSize <= FSignatureTailCapacity )
   {
    memcpy( FSignatureTail.get() + FSignatureTailSize, Data, Size );
    FSignatureTailSize += Size;
    return true;
   }

  size_t FlushSize = CombinedSize - FSignatureTailCapacity;
  const size_t TailFlushSize = std::min( FlushSize, FSignatureTailSize );
  if( TailFlushSize && !WriteFirmwarePayload( FSignatureTail.get(), TailFlushSize ) )
    return false;

  if( TailFlushSize < FSignatureTailSize )
    memmove( FSignatureTail.get(), FSignatureTail.get() + TailFlushSize,
      FSignatureTailSize - TailFlushSize );
  FSignatureTailSize -= TailFlushSize;
  FlushSize -= TailFlushSize;

  if( FlushSize && !WriteFirmwarePayload( Data, FlushSize ) )
    return false;
  Data += FlushSize;
  Size -= FlushSize;

  memcpy( FSignatureTail.get() + FSignatureTailSize, Data, Size );
  FSignatureTailSize += Size;
  return true;
#else
  return WriteFirmwarePayload( Data, Size );
#endif
 }

bool TEspWebUpdate::WriteFirmwarePayload( uint8_t const * Data, size_t Size )
 {
#ifdef ESP8266
  if( FSigningHash )
    FSigningHash->add( Data, Size );
#endif

  if( !ConsumeDeviceTypeHeader( TPart::Firmware, Data, Size ) )
    return false;
  if( !FDeviceTypeHeaderVerified )
    return true;
  if( Size > FDeviceTypePayloadRemaining )
   {
    SetError( F( "Firmware update exceeds DeviceType payload size" ) );
    return false;
   }
  if( !Size )
    return true;
  if( !FUpdatePayloadStarted && !BeginFirmwarePayloadUpload( FDeviceTypePayloadSize ) )
    return false;

#ifdef ESP8266
  if( FFirmwarePayloadSize > FFirmwareExpectedPayloadSize ||
      Size > FFirmwareExpectedPayloadSize - FFirmwarePayloadSize )
   {
    SetError( F( "Firmware upload exceeds selected file size" ) );
    return false;
   }
#endif

  if( Update.write( const_cast<uint8_t *>( Data ), Size ) != Size )
   {
    SetUpdaterError( F( "Firmware write failed" ) );
    return false;
   }
  FDeviceTypePayloadRemaining -= Size;
  FFirmwarePayloadSize += Size;
  return true;
 }
