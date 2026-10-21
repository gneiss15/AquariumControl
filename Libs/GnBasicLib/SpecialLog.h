// SpecialLog.h, Version: 1.04
#pragma once

#include <GnPlatform.h>

#if defined( GNPlatformESP32 )

#include <GnBasics.h>
#include <GnCoopTimer.h>
#include <Arduino.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <vector>

// FreeRTOS mutex for safe RAM/FS access without disabling interrupts
extern "C"
 {
  #include <freertos/FreeRTOS.h>
  #include <freertos/semphr.h>
 }

// ---- Feature toggles (can be overridden in project Config.h before including this header) ----
#ifndef SPECIALLOG_ENABLE_BUFFER
  #define SPECIALLOG_ENABLE_BUFFER 0
#endif
#ifndef SPECIALLOG_ENABLE_FLASH
  #define SPECIALLOG_ENABLE_FLASH 0
#endif
#ifndef SPECIALLOG_ENABLE_HTTP
  #define SPECIALLOG_ENABLE_HTTP 0
#endif

#if SPECIALLOG_ENABLE_FLASH && !SPECIALLOG_ENABLE_BUFFER
  #error "If SPECIALLOG_ENABLE_FLASH is set, SPECIALLOG_ENABLE_BUFFER must also be enabled"
#endif
#if SPECIALLOG_ENABLE_HTTP && !SPECIALLOG_ENABLE_BUFFER
  #error "If SPECIALLOG_ENABLE_HTTP is set, SPECIALLOG_ENABLE_BUFFER must also be enabled"
#endif

extern void OnTimeSync( struct timeval * tv );

class TSpecialLog : public TLoopInstance, public TSingleton<TSpecialLog>
 {
  typedef std::vector<uint8_t> TBufType;
 public:
  enum class TFlushMode : uint8_t
   {
    Direct
	#if SPECIALLOG_ENABLE_FLASH
	 , Interval, OnDemand
	#endif
   };

                       TSpecialLog(void);

  // Init: MaxTotalBytes = RAM-Log-Gesamtgröße (A+B), nicht Flash!
  bool                 Begin
                        (
                         size_t maxTotalBytes
                         #if SPECIALLOG_ENABLE_FLASH
                           ,
                           TFlushMode mode = TFlushMode::OnDemand,
                           TGnCoopTimer_t intervalMs = 2000
                         #endif						   
                        );

  void                 End(void)                                               {}

  // Logging
  void                 SpLog( const char * msg );
  void                 SpLogln( const String & str )                           { SpLog( str.c_str() ); }
  void                 SpLogf( const char * fmt, ... )  __attribute__ ((format (printf, 2, 3)));

  // Flush Policy & Trigger
  #if SPECIALLOG_ENABLE_FLASH
    void               Mode( TFlushMode mode );
    void               IntervalMs( TGnCoopTimer_t ms );
    void               FlushReq(void)                                          { FFlushRequest = true; }
    void               FlushNow(void);                                         // Aufruf nur aus Loop usw. nicht via SpLog...
   #else // Dummys so every combination can use the same API
    void               FlushNow(void)                                          {}
  #endif

  #if SPECIALLOG_ENABLE_BUFFER
    // Info
    bool               ActiveIsA(void) const                                   { return FActiveBuf == &FBufA; }
    size_t             ActiveSize(void) const                                  { return *FActiveSize; }
    size_t             OtherSize(void)  const                                  { return *FOtherSize; }
    size_t             MaxPerFile(void) const                                  { return FMaxPerFile; }
    // Dump
    void               DumpTo( Print & out );
    void               DumpToSerial(void)                                      { DumpTo(Serial); }
   #else // Dummys so every combination can use the same API
    void               DumpTo( Print & )                                       {}
    void               DumpToSerial(void)                                      {}
  #endif

  #if SPECIALLOG_ENABLE_HTTP
    // HTTP: streamt RAM-Logs in Reihenfolge (older → active)
    // Optional: ?download=1 → Content-Disposition
    void               RegisterHttp( AsyncWebServer & server,
                                     const char * path = "/logs" );
   #else // Dummys so every combination can use the same API
    void               RegisterHttp( AsyncWebServer &, const char *)           {}
  #endif

 private:
  virtual void         Loop(void) override;

 private:
  #if SPECIALLOG_ENABLE_FLASH
    // Policy
    TFlushMode         FMode = TFlushMode::OnDemand;
    TGnCoopTimer_t     FIntervalMs = 2000;
    TGnCoopTimerMs     FFlushTimer;
    bool               FFlushTimerDue = false;
    bool               FStarted = false;
    size_t             FLastFlushedTotal = 0;
    // Flash I/O
    void               FlushToFlashUnlocked(void);        // ohne Lock (außer wo nötig)
    void               LoadOneFile( const char * path,
                                    TBufType & buf, size_t & size );
    void               LoadFromFlash(void);               // beim Start
    TGnCoopTimer_t     FlushIntervalMs(void) const;
    void               FlushTimerCb( TGnCoopTimerBase * Tmr, uint16_t IntervalsElapsed );
    // Request Delayed Flush (in Loop)
    bool               FFlushRequest = false;
  #endif

  #if SPECIALLOG_ENABLE_BUFFER
    // Mutex to serialize RAM and FS access (recursive)
    static SemaphoreHandle_t 
                       SLogMutex;
    static inline void EnsureMutex(void)                                        { if (!SLogMutex) SLogMutex = xSemaphoreCreateRecursiveMutex(); }

    // Virtuelle „Dateien“ (RAM)
    TBufType           FBufA;
    TBufType           FBufB;
    size_t             FSizeA = 0;
    size_t             FSizeB = 0;
  
    TBufType *         FActiveBuf = &FBufA;
    TBufType *         FOtherBuf = &FBufB;
    size_t *           FActiveSize = &FSizeA;
    size_t *           FOtherSize = &FSizeB;

    size_t             FMaxPerFile = 0;     // pro RAM-Datei (maxTotalBytes/2)

    // intern
    void               AppendLine( const char * line ); // schreibt in aktive RAM-Datei
    void               RotateAfterWrite(void);          // löscht andere RAM-Datei, schaltet um

    // RAM helpers
    void               RamAppend( TBufType & buf, size_t & size,
                                  const uint8_t * data, size_t len );
    void               RamClear( TBufType & buf, size_t & size );

  #endif

  #if SPECIALLOG_ENABLE_HTTP
    // HTTP
    AsyncWebServerResponse *
                       MakeLogsResponse( AsyncWebServerRequest * req );
  #endif

  // Format helper
  static String        VsnprintfGrow( const char * fmt, va_list ap );
 };
#define SpecialLog     TSpecialLog::Instance()

#endif // ESP32
