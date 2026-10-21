// GnBasics.cpp, Version: 1.09
//****************************************************************
// Includes
//****************************************************************

#include <GnBasics.h>
#include <algorithm>

#if defined( GNPlatformESP )
  #include <GNPlatformESP.h>
  #include <LittleFS.h>
  #if defined( GNPlatformESP32 )
    #include <esp_timer.h>
  #endif
#else
  #include <cstdlib>
  #include <fstream>
  #include <sstream>
  #include <thread>
#endif

//****************************************************************
// Configuration
//****************************************************************

//****************************************************************
// typedefs
//****************************************************************

//****************************************************************
// Global Vars
//****************************************************************

//****************************************************************
// GnAssert
//****************************************************************

noreturn_ void failed_
 (
  char const * file,
  char const * line,
  char const * msg
 )
 {
  static char const * file_ __attribute__(( unused )) = NULL;
  static char const * line_ __attribute__(( unused )) = NULL;
  static char const * msg_ __attribute__(( unused )) = NULL;

  file_ = file;
  line_ = line;
  msg_ = msg;

  #if defined( GNPlatformESP )
    Serial.begin( 115200 );
    Serial.println();
    Serial.print( F( "Assertion failed: File: " ) );
    Serial.print( file );
    Serial.print( F( ", Line: " ) );
    Serial.print( line );
    Serial.print( F( ", Msg: " ) );
    Serial.println( msg );

    while( 1 );
  #else
    fprintf( stderr, "Assertion failed: File: %s, Line: %s, Msg: %s\n", file, line, msg );
    abort();
  #endif
 }

//****************************************************************
// Functions (Time)
//****************************************************************

#if !defined( GNPlatformAVR )
uint64_t GnMillis64(void)
 {
  #if defined( GNPlatformESP32 )
    return static_cast<uint64_t>( esp_timer_get_time() ) / 1000;
  #elif defined( GNPlatformESP8266 )
    static uint32_t LastMillis = millis();
    static uint64_t MillisHigh = 0;
    const uint32_t CurrentMillis = millis();
    if( CurrentMillis < LastMillis )
      MillisHigh += 0x100000000ULL;
    LastMillis = CurrentMillis;
    return MillisHigh + CurrentMillis;
   #else
    return static_cast<uint64_t>( std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch() ).count() );
  #endif
 }
#endif

//****************************************************************
// Functions (Delay)
//****************************************************************

void DelayUs( uint32_t Us )
 {
  #if defined( GNPlatformESP )
    while ( Us >= 10000 )
     {
      WdtDelay( 10 );
      Us -= 10000;
     }
    if ( Us > 0 )
     {
      delayMicroseconds( Us );
     }
   #else
    std::this_thread::sleep_for( std::chrono::microseconds( Us ) );
  #endif
 }

void DelayMs( uint32_t Ms )
 {
  #if defined( GNPlatformESP )
    WdtDelay( Ms );
   #else
    std::this_thread::sleep_for( std::chrono::milliseconds( Ms ) );
  #endif
 }

void DelayS( uint32_t Seconds )
 {
  #if defined( GNPlatformESP )
    while ( Seconds > 0 )
     {
      WdtDelay( 1000 );
      --Seconds;
     }
   #else
    std::this_thread::sleep_for( std::chrono::seconds( Seconds ) );
  #endif
 }

//****************************************************************
// Functions (others)
//****************************************************************

// StringFormat
TGNString StringFormat( const char * fmt, ... )
 {
  va_list args;
  va_start( args, fmt );

  // Erst die benötigte Länge ermitteln
  int len = vsnprintf( nullptr, 0, fmt, args );
  va_end( args );

  if( len <= 0 )
    return TGNString();

  // Puffer exakt groß genug anlegen (+1 für '\0')
  char * buf = new char[ len + 1 ];

  va_start( args, fmt );
  vsnprintf( buf, len + 1, fmt, args );
  va_end( args );

  TGNString result( buf );
  delete[] buf;
  return result;
 }

#if defined( GNPlatformESP )
bool GetLittleFSInfo( TFSInfo & Info )
 {
  Info = {};
#if defined( GNPlatformESP8266 )
  FSInfo NativeInfo;
  if( !LittleFS.info( NativeInfo ) )
    return false;

  Info.totalBytes = NativeInfo.totalBytes;
  Info.usedBytes = NativeInfo.usedBytes;
  Info.blockSize = NativeInfo.blockSize;
  Info.pageSize = NativeInfo.pageSize;
  Info.maxOpenFiles = NativeInfo.maxOpenFiles;
  Info.maxPathLength = NativeInfo.maxPathLength;
  return true;
#elif defined( GNPlatformESP32 )
  // Arduino-ESP32 exposes total/used bytes, but not the ESP8266 detail fields.
  Info.totalBytes = LittleFS.totalBytes();
  Info.usedBytes = LittleFS.usedBytes();
  return Info.totalBytes != 0 && Info.usedBytes <= Info.totalBytes;
#endif
 }
#endif

#if defined( GNPlatformESP )
namespace
 {
TGNString LittleFSTemporaryName( char const * FileName )
 {
  return TGNString( FileName ) + ".tmp";
 }

TGNString LittleFSBackupName( char const * FileName )
 {
  return TGNString( FileName ) + ".bak";
 }
} // namespace

void DiscardLittleFSTemporary( char const * FileName )
 {
  const TGNString TemporaryName = LittleFSTemporaryName( FileName );
  if( LittleFS.exists( TemporaryName.c_str() ) )
    LittleFS.remove( TemporaryName.c_str() );
 }

bool WriteLittleFSTemporary( char const * FileName, TGNString const & Data, TGNString & Error )
 {
  const TGNString TemporaryName = LittleFSTemporaryName( FileName );
  DiscardLittleFSTemporary( FileName );

  File Target = LittleFS.open( TemporaryName.c_str(), "w" );
  if( !Target )
   {
    Error = TGNString( "Cannot open temporary file: " ) + TemporaryName;
    return false;
   }

  const size_t Written = Target.write(
    reinterpret_cast<uint8_t const *>( Data.c_str() ), Data.size() );
  Target.close();
  if( Written != Data.size() )
   {
    LittleFS.remove( TemporaryName.c_str() );
    Error = TGNString( "Incomplete temporary file write: " ) + TemporaryName;
    return false;
   }

  File Verify = LittleFS.open( TemporaryName.c_str(), "r" );
  if( !Verify || Verify.size() != Data.size() )
   {
    if( Verify )
      Verify.close();
    LittleFS.remove( TemporaryName.c_str() );
    Error = TGNString( "Temporary file size verification failed: " ) + TemporaryName;
    return false;
   }

  uint8_t Buffer[128];
  size_t Offset = 0;
  while( Offset < Data.size() )
   {
    const size_t Requested = std::min( sizeof( Buffer ), Data.size() - Offset );
    const size_t Read = Verify.read( Buffer, Requested );
    if( Read != Requested || memcmp( Buffer, Data.c_str() + Offset, Read ) != 0 )
     {
      Verify.close();
      LittleFS.remove( TemporaryName.c_str() );
      Error = TGNString( "Temporary file content verification failed: " ) + TemporaryName;
      return false;
     }
    Offset += Read;
   }
  Verify.close();
  Error.clear();
  return true;
 }

bool CommitLittleFSTemporary( char const * FileName, TGNString & Error )
 {
  const TGNString TemporaryName = LittleFSTemporaryName( FileName );
  const TGNString BackupName = LittleFSBackupName( FileName );
  if( !LittleFS.exists( TemporaryName.c_str() ) )
   {
    Error = TGNString( "Temporary file missing: " ) + TemporaryName;
    return false;
   }

  if( LittleFS.exists( BackupName.c_str() ) && !LittleFS.remove( BackupName.c_str() ) )
   {
    Error = TGNString( "Cannot remove old backup: " ) + BackupName;
    return false;
   }

  const bool HadCurrent = LittleFS.exists( FileName );
  if( HadCurrent && !LittleFS.rename( FileName, BackupName.c_str() ) )
   {
    Error = TGNString( "Cannot create backup: " ) + BackupName;
    return false;
   }

  if( LittleFS.rename( TemporaryName.c_str(), FileName ) )
   {
    Error.clear();
    return true;
   }

  if( HadCurrent )
    LittleFS.rename( BackupName.c_str(), FileName );
  Error = TGNString( "Cannot activate temporary file: " ) + FileName;
  return false;
 }

bool RestoreLittleFSBackup( char const * FileName, TGNString & Error )
 {
  const TGNString BackupName = LittleFSBackupName( FileName );
  if( !LittleFS.exists( BackupName.c_str() ) )
   {
    Error = TGNString( "Backup file missing: " ) + BackupName;
    return false;
   }

  if( LittleFS.exists( FileName ) && !LittleFS.remove( FileName ) )
   {
    Error = TGNString( "Cannot remove invalid file: " ) + FileName;
    return false;
   }
  if( !LittleFS.rename( BackupName.c_str(), FileName ) )
   {
    Error = TGNString( "Cannot restore backup: " ) + BackupName;
    return false;
   }
  Error.clear();
  return true;
 }
#endif

// Read File from LittleFs into String
TGNString ReadFile( char const * fileName )
 {
  #if defined( GNPlatformESP )
    LittleFS.begin();
    File f = LittleFS.open( fileName, "r" );
    if( !f ) 
     {
      Dprintf( "Can't open file: %s\n", fileName );
      return "";
     }
    TGNString res( f.readString() );
    f.close();
    return res;
  #else
    std::ifstream f( fileName, std::ios::binary );
    if( !f )
     {
      Dprintf( "Can't open file: %s\n", fileName );
      return "";
     }
    std::ostringstream res;
    res << f.rdbuf();
    return res.str();
  #endif
 }
#if defined( GNPlatformESP )
TGNString ReadFile( __FlashStringHelper const * fileName )
 {
  LittleFS.begin();
  File f = LittleFS.open( fileName, "r" );
  if( !f ) 
   {
    Dprintf( "Can't open file: %s\n", TGNString( fileName ).c_str() );
    return "";
   }
  TGNString res( f.readString() );
  f.close();
  return res;
 }
#endif

//****************************************************************
// TLoopInstances List of Instances, that needs to be called inside loop
//****************************************************************

void TLoopInstances::Add( TLoopInstance * obj )
 {
  // Put the new LoopInstance at the beginning of the list
  obj->FNext = FList;
  FList = obj;
 }

void TLoopInstances::Remove( TLoopInstance * obj )
 {
  for( TLoopInstance ** p = &FList;  *p;  p = &( (*p)->FNext ) )
    if( *p == obj )
     {
      *p = obj->Next();
      return;
     }
 }

void TLoopInstances::Loop(void)
 {
  #if defined( GNPlatformESP8266 )
    (void)GnMillis64();
  #endif
  for( TLoopInstance * p = FList;  p;  p = p->FNext )
    p->Loop();
 }

//****************************************************************
// TLog
//****************************************************************
TLog::TLog(void)
 {
  #if defined( GNPlatformESP )
    Serial.begin( 115200 );
    while (!Serial);
    printf( "\nLog-Start:\n" );
    delay( 2 );
  #else
    printf( "\nLog-Start:\n" );
  #endif
 }

void TLog::LogPrintf( char const * fmt, ... )
 {
  va_list va;
  va_start( va, fmt );
  vprintf( fmt, va );
  va_end( va );
 }

