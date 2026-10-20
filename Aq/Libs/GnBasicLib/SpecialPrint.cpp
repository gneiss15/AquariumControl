// SpecialPrint.cpp, Version: 1.01

#include <GnPlatform.h>

#if defined( GNPlatformESP32 )
//****************************************************************
// Includes
//****************************************************************

#include "SpecialPrint.h"
#include "esp_sntp.h"
#include "SpecialLog.h"

//****************************************************************
// Init / NTP
//****************************************************************
void InitSpecialPrint(void)
 {
  configTzTime( "CET-1CEST,M3.5.0,M10.5.0/3", "192.168.99.1", "pool.ntp.org", "time.nist.gov" );
  sntp_set_time_sync_notification_cb( OnTimeSync );

  // SpecialLog RAM-Doppellog – z. B. 20 KiB total
  SpecialLog.Begin
   (
    #ifdef SPECIALLOG_MAX_TOTAL_BYTES
      SPECIALLOG_MAX_TOTAL_BYTES
     #else
      /*maxTotalBytes=*/20*1024
    #endif
    #if SPECIALLOG_ENABLE_FLASH
      ,
      /*mode=*/TSpecialLog::TFlushMode::OnDemand, // nur bei Bedarf in Flash spiegeln
      /*intervalMs=*/2000
    #endif
   );
 }

//****************************************************************
// TSpecialPrt Print to Serial & HttpSocket
//****************************************************************

char TSpecialPrint::FBuffer[ 1000 ];

static inline void MakeTimestamp( char * out, size_t outSize )
 {
  time_t now;
  time( &now );
  struct tm tm;
  if( localtime_r( &now, &tm ) )
    strftime( out, outSize, "%Y-%m-%d %H:%M:%S", &tm );
   else
   {
    // Fallback, falls Zeit noch nicht synchron
    strncpy( out, "1970-01-01 00:00:00", outSize );
    out[ outSize ? outSize-1 : 0 ] = '\0';
   }
 }

void TSpecialPrint::iprintf( unsigned int prtTyp, char const * fmt, va_list ap )
 {
  int n = 0;
  size_t rem = sizeof( FBuffer );
  if( prtTyp > 1 )
   {
    char ts[ 24 ];
    MakeTimestamp( ts, sizeof( ts ) );
    
    n = snprintf( FBuffer, sizeof( FBuffer ), "[%s] ", ts );
    if( n < 0 ) n = 0;
    rem = ( n < (int)sizeof( FBuffer ) ) ? ( sizeof( FBuffer ) - (size_t)n ) : 0;
   }

  if( rem > 0 )
    vsnprintf( FBuffer + n, rem, fmt, ap );

  Serial.printf( "%s", FBuffer );
  if( FHttpSocket )
    FHttpSocket->textAll( FBuffer );  // an alle HttpSocket-Clients senden

  if( prtTyp > 2 )
    SpecialLog.SpLog( FBuffer );
 }

void TSpecialPrint::printf( char const * fmt, ... )
 {
  va_list ap;
  va_start( ap, fmt );
  iprintf( 1, fmt, ap );
  va_end( ap );
 }

void TSpecialPrint::tprintf( const char* fmt, ... )
 {
  va_list ap;
  va_start( ap, fmt );
  iprintf( 2, fmt, ap );
  va_end( ap );
 }

void TSpecialPrint::lprintf( const char* fmt, ... )
 {
  va_list ap;
  va_start( ap, fmt );
  iprintf( 3, fmt, ap );
  va_end( ap );
 }

#endif // ESP32
