// SpecialPrint.h, Version: 1.02
#pragma once

#include <GnPlatform.h>

#if defined( GNPlatformESP32 )

#ifndef SpecialPrintIncluded	// #pragma once funktioniert hier nicht !?!
#define SpecialPrintIncluded
//****************************************************************
// Includes
//****************************************************************

#include <GnBasics.h>
#include <ESPAsyncWebServer.h>
#include <time.h>
#include <FS.h>
#include <LittleFS.h>

//****************************************************************
// TSpecialPrint Print to Serial & HttpSocket
//****************************************************************

//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wunused-parameter" 
extern void InitSpecialPrint(void);

class TSpecialPrint : public TSingleton<TSpecialPrint>
 {
  friend class TSingleton<TSpecialPrint>;
 private:
                       TSpecialPrint(void) : FHttpSocket( nullptr ) {}
 public:
  void                 HttpSocket( AsyncWebSocket * httpSocket ) { FHttpSocket = httpSocket; }
  void                 printf( char const * fmt, ... )  __attribute__ ((format (printf, 2, 3)));
  void                 tprintf( char const * fmt, ... )  __attribute__ ((format (printf, 2, 3)));
  void                 lprintf( char const * fmt, ... )  __attribute__ ((format (printf, 2, 3)));
 private:
  void                 iprintf( unsigned int prtTyp, char const * fmt, va_list ap );
 private:
  static char          FBuffer[ 1000 ];
  AsyncWebSocket       *FHttpSocket;
 };
// print to Serial & HttpSocket
#define SPrintf( fmt, ... )               TSpecialPrint::Instance().printf( fmt, ## __VA_ARGS__ )
// print to Serial & HttpSocket with Timestamp
#define TSPrintf( fmt, ... )              TSpecialPrint::Instance().tprintf( fmt, ## __VA_ARGS__ )
// print to Serial, HttpSocket & Log-File with Timestamp
#define LSPrintf( fmt, ... )              TSpecialPrint::Instance().lprintf( fmt, ## __VA_ARGS__ )

#define SetSPrintfWebSocket( httpSocket ) TSpecialPrint::Instance().HttpSocket( httpSocket ) 

//#pragma GCC diagnostic pop


#endif // sentry

#endif // ESP32
