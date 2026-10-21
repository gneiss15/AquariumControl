// SpecialLog.cpp, Version: 1.03

#include <GnPlatform.h>

#if defined( GNPlatformESP32 )
#include "SpecialLog.h"
#include "SpecialPrint.h"

#define PATH_A "/logA.bin"
#define PATH_B "/logB.bin"

static inline size_t MinSize( size_t a, size_t b ) { return a < b ? a : b; }

static volatile bool GTimeSyncPending = false;
void OnTimeSync( struct timeval * tv )
 {
  GTimeSyncPending = true;
 }

TSpecialLog::TSpecialLog(void)
#if SPECIALLOG_ENABLE_FLASH
: FFlushTimer( Closure( this, &TSpecialLog::FlushTimerCb ), 60000, false )
#endif
 {
 }

bool TSpecialLog::Begin
 (
  size_t maxTotalBytes
  #if SPECIALLOG_ENABLE_FLASH
    ,
    TFlushMode mode,
    TGnCoopTimer_t intervalMs
  #endif
 )
 {
  #if SPECIALLOG_ENABLE_FLASH
    FMode        = mode;
    FIntervalMs  = intervalMs;
    FFlushTimer.Interval( FlushIntervalMs() );

    FLastFlushedTotal = 0;  
    FFlushRequest = false;
    FFlushTimerDue = false;
  #endif

  #if SPECIALLOG_ENABLE_BUFFER
    FBufA.reserve( FMaxPerFile + 512 );
    FBufB.reserve( FMaxPerFile + 512 );
    FSizeA = FSizeB = 0;
  
    FActiveBuf  = &FBufA;
    FOtherBuf   = &FBufB;
    FActiveSize = &FSizeA;
    FOtherSize  = &FSizeB;

    FMaxPerFile  = ( maxTotalBytes / 2 );
    if( FMaxPerFile < 1024 )
      FMaxPerFile = 1024;
  #endif
  #if SPECIALLOG_ENABLE_FLASH
    LittleFS.begin();
    LoadFromFlash();
    FStarted = true;
    FFlushTimer.Start();
  #endif
  return true;
 }

void TSpecialLog::SpLog( const char * msg )
 {
  const size_t len = strlen( msg );

  #if SPECIALLOG_ENABLE_FLASH
    EnsureMutex();
    xSemaphoreTakeRecursive( SLogMutex, portMAX_DELAY );
  #endif
  
  #if SPECIALLOG_ENABLE_BUFFER
    RamAppend( *FActiveBuf, *FActiveSize, (const uint8_t *)msg, len );

  // Oversize? → erst nach dem Schreiben rotieren
    if( *FActiveSize > FMaxPerFile )
      RotateAfterWrite();
  #endif

  #if SPECIALLOG_ENABLE_FLASH
    xSemaphoreGiveRecursive( SLogMutex );
    if( FMode == TFlushMode::Direct )
      FlushReq();
  #endif
 }

void TSpecialLog::SpLogf( const char * fmt, ... )
 {
  char str[ 256 ];
  va_list ap;
  va_start( ap, fmt );
  int n = vsnprintf( str, sizeof( str ), fmt, ap );
  va_end( ap );

  if( n < 0 ) return;
  if( (size_t)n < sizeof( str ) )
   {
    SpLog( str );
    return;
   }

  va_start( ap, fmt );
  String dyn = VsnprintfGrow( fmt, ap );
  va_end( ap );
  SpLog( dyn.c_str() );
 }

#if SPECIALLOG_ENABLE_FLASH
  TGnCoopTimer_t TSpecialLog::FlushIntervalMs(void) const
   {
    const TGnCoopTimer_t Interval =
      FMode == TFlushMode::Interval && FIntervalMs < 60000UL ? FIntervalMs : 60000UL;
    return Interval == 0 ? 1 : Interval;
   }

  void TSpecialLog::Mode( TFlushMode mode )
   {
    FMode = mode;
    if( FStarted && !FFlushTimerDue )
      FFlushTimer.Interval( FlushIntervalMs() );
   }

  void TSpecialLog::IntervalMs( TGnCoopTimer_t ms )
   {
    FIntervalMs = ms;
    if( FStarted && !FFlushTimerDue )
      FFlushTimer.Interval( FlushIntervalMs() );
   }

  void TSpecialLog::FlushTimerCb( TGnCoopTimerBase * Tmr, uint16_t )
   {
    Tmr->Stop();
    FFlushTimerDue = true;
   }

  void TSpecialLog::FlushNow(void)
   {
    FFlushRequest = false;
    EnsureMutex();
    xSemaphoreTakeRecursive( SLogMutex, portMAX_DELAY );
    // Wir kopieren keine Daten; FlushToFlashUnlocked liest synchron unter Lock
    const size_t total = *FOtherSize + *FActiveSize;
    if( total != FLastFlushedTotal )
     {
      FlushToFlashUnlocked();
      FLastFlushedTotal = total;
      FFlushTimerDue = false;
      FFlushTimer.Interval( FlushIntervalMs() );
      FFlushTimer.Start();
     }
    xSemaphoreGiveRecursive( SLogMutex );
   }
#endif

#if SPECIALLOG_ENABLE_BUFFER
  void TSpecialLog::DumpTo( Print & out )
   {
    out.write( FOtherBuf->data(), *FOtherSize );
    out.write( FActiveBuf->data(), *FActiveSize );
   }
#endif

#if SPECIALLOG_ENABLE_HTTP
  void TSpecialLog::RegisterHttp( AsyncWebServer & server, const char * path )
   {
    server.on
     (
      path, HTTP_GET, [this](AsyncWebServerRequest * req )
       {
        // 404, wenn aktuell keine Daten
        if( ( ActiveSize() + OtherSize() ) == 0 )
         {
          req->send( 404, "text/plain", "no log" );
          return;
         }
        req->send( MakeLogsResponse( req ) );
       }
     );
   }
#endif

void TSpecialLog::Loop(void)
 {
  if( GTimeSyncPending )
   {
    GTimeSyncPending = false;
    LSPrintf( "[TIME] sync ok\n" );     // jetzt im loopTask: Serial/WS/FS sind ok
   }

  #if SPECIALLOG_ENABLE_FLASH
    if( FFlushRequest || FFlushTimerDue )
      FlushNow();
  #endif
 }

#if SPECIALLOG_ENABLE_FLASH
  void TSpecialLog::FlushToFlashUnlocked(void)
   {
    // schreibt die beiden RAM-Dateien so, wie sie aktuell sind
    // Reihenfolge: ältere(other) → active (für spätere Re-Loads egal)

    const char * pathOther  = ActiveIsA() ? PATH_B : PATH_A;
    const char * pathActive = ActiveIsA() ? PATH_A : PATH_B;

    // Older
    File fo = LittleFS.open( pathOther, "w" );
    if( fo )
     {
      if( FOtherSize )
        fo.write( FOtherBuf->data(), *FOtherSize );
      fo.close();
     }
    // Active
    if( LittleFS.exists( pathActive ) )
      LittleFS.remove( pathActive );
    File fa = LittleFS.open( pathActive, "w" );
    if( fa )
     {
      if( FActiveSize )
        fa.write( FActiveBuf->data(), *FActiveSize );
      fa.close();
     }
   }

  void TSpecialLog::LoadOneFile( const char * path, TBufType & buf, size_t & size )
   {
    size = 0;
    File f = LittleFS.open( path, "r" );
    if( !f ) return;
    size_t fs = f.size();
     buf.resize( std::max( fs, FMaxPerFile ) );
     int r = f.read( buf.data(), fs );
     if( r > 0 )
       size = (size_t)r;
     f.close();
   }

  void TSpecialLog::LoadFromFlash(void)
   {
     // Dateien von Flash holen
     LoadOneFile( PATH_A, FBufA, FSizeA );
     LoadOneFile( PATH_B, FBufB, FSizeB );

     // kleinere wird aktiv
     if( FSizeA <= FSizeB )
      {
       FActiveBuf = &FBufA;
       FOtherBuf = &FBufB;
       FActiveSize = &FSizeA;
       FOtherSize = &FSizeB;
      }
      else
      {
       FActiveBuf = &FBufB;
       FOtherBuf = &FBufA;
       FActiveSize = &FSizeB;
       FOtherSize = &FSizeA;
      }
   }
#endif

#if SPECIALLOG_ENABLE_BUFFER
  SemaphoreHandle_t TSpecialLog::SLogMutex = nullptr;

  void TSpecialLog::AppendLine( const char * line )
   {
    SpLog( line ); // Alias – historische API falls du das Interface so haben wolltest
   }

  void TSpecialLog::RotateAfterWrite(void)
   {
    // „Andere“ RAM-Datei wird gelöscht, dann Umschalten.
    RamClear( *FOtherBuf, *FOtherSize );
    std::swap( FActiveBuf, FOtherBuf );
    std::swap( FActiveSize, FOtherSize );
   }

  void TSpecialLog::RamAppend( TBufType & buf, size_t & size,
                               const uint8_t * data, size_t len )
   {
    if( len == 0 ) return;
    if( size + len > buf.size() )
      buf.resize( size + len );
    memcpy( buf.data() + size, data, len );
    size += len;
   }

  void TSpecialLog::RamClear( TBufType & buf, size_t & size )
   {
    size = 0; // Inhalt gilt als gelöscht; Kapazität bleibt für Performance
    if( buf.size() > FMaxPerFile )
      buf.resize( FMaxPerFile );
   }
#endif

#if SPECIALLOG_ENABLE_HTTP
  AsyncWebServerResponse * TSpecialLog::MakeLogsResponse( AsyncWebServerRequest * /*req*/ )
   {
    const bool activeIsA = ActiveIsA();

    auto * resp = new AsyncChunkedResponse
     (
      "text/plain; charset=utf-8",
      [this]( uint8_t * out, size_t maxLen, size_t index ) -> size_t
       {
        // pro Chunk kurz den Log-Mutex nehmen → konsistente Pointer/Größen
        EnsureMutex();
        xSemaphoreTakeRecursive( SLogMutex, portMAX_DELAY );

        const TBufType * olderBuf   = FOtherBuf;
        const size_t     olderSize  = *FOtherSize;
        const TBufType * activeBuf  = FActiveBuf;
        const size_t     activeSize = *FActiveSize;
        const size_t     total      = olderSize + activeSize;

        if( index >= total )
         {
          xSemaphoreGiveRecursive( SLogMutex );
          return 0;
         }

        size_t copied = 0;

        // zuerst aus older
        if( index < olderSize )
         {
          const size_t n = std::min( maxLen, olderSize - index );
          if( n )
           {
            memcpy( out, olderBuf->data() + index, n );
            copied += n;
            index += n;
            out += n;
            maxLen -= n;
           }
         }

        // dann aus active
        if( maxLen > 0 && index >= olderSize )
		 {
          const size_t off = index - olderSize;
          if( off < activeSize )
		   {
			const size_t n = std::min( maxLen, activeSize - off );
            if( n )
			 {
              memcpy( out, activeBuf->data() + off, n );
              copied += n;
             }
           }
         }

        xSemaphoreGiveRecursive( SLogMutex );
        return copied;
       }
     );

    resp->addHeader( "Cache-Control", "no-store" );
    resp->addHeader( "X-Log-Active", activeIsA ? "A" : "B");

    // Größen-Header informativ (aktueller Stand)
    EnsureMutex();
    xSemaphoreTakeRecursive( SLogMutex, portMAX_DELAY );
    const size_t sizeOld = *FOtherSize;
    const size_t sizeAct = *FActiveSize;
    xSemaphoreGiveRecursive( SLogMutex );

    resp->addHeader( "X-Log-Size-Old",    String( (uint32_t)sizeOld ) );
    resp->addHeader( "X-Log-Size-Active", String( (uint32_t)sizeAct ) );
    resp->addHeader( "X-Log-Size-Total",  String( (uint32_t)( sizeOld + sizeAct ) ) );

    return resp;
  }
#endif

String TSpecialLog::VsnprintfGrow( const char * fmt, va_list ap )
 {
  va_list ap2;
  va_copy( ap2, ap );
  int need = vsnprintf( nullptr, 0, fmt, ap2 );
  va_end( ap2 );
  if( need <= 0 )
    return String();

  std::vector<char> buf( (size_t)need + 1 );
  vsnprintf( buf.data(), buf.size(), fmt, ap );
  return String( buf.data() );
 }

#endif // ESP32
