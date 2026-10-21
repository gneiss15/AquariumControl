// WebApplication.h, Version: 2.41
#pragma once
#include <GnBasics.h>
#include "BuildConfig.h"
#include "ButtonController.h"
#include "ConfigStore.h"
#include "Logger.h"
#include "Scheduler.h"
#include "StatusApi.h"
#include "TimeConfigSync.h"
#include "Types.h"
#if !defined( SYSTEM_ESP )
  #include <atomic>
#endif
#include <vector>

enum class TContentType : uint8_t
 {
  PlainText,
  Html,
  Css,
  Json,
  JavaScript
 };

struct THttpResponse
 {
  int           Status = 200;
  TContentType  ContentType = TContentType::PlainText;
  TGNString     Body;
  bool          AllowCors = false;
  bool          ExitAfterResponse = false;

  char const * ContentTypeText(void) const
   {
    switch( ContentType )
     {
      case TContentType::PlainText:
        return "text/plain";
      case TContentType::Html:
        return "text/html";
      case TContentType::Css:
        return "text/css";
      case TContentType::Json:
        return "application/json";
      case TContentType::JavaScript:
        return "application/javascript";
     }
    return "application/octet-stream";
   }
 };

class TWebApplication
 {
 public:
                                TWebApplication( TScheduler & SchedulerRef, TButtonController & Buttons, TStatusApi & StatusApi,
                                                 TChannelConfigs & Channels, TSwitchTimes & SwitchTimes,
                                                 TSwitchIntervals & Intervals, TGNString DataDir, TTimeConfigSync & TimeConfigSync );
  THttpResponse                  Handle( const TGNString & Method, const TGNString & Path, const TGNString & Body );
  bool                           ApplyRemoteTimes( const TGNString & Content, const TGNString & TimeStamp, TGNString & Error );
  #if !defined( SYSTEM_ESP )
    bool                         ExitRequested(void) const { return FExitRequested.load(); }
    void                         RequestExit(void) { FExitRequested.store( true ); }
  #endif

 private:
  THttpResponse                  StaticFile( const TGNString & Path ) const;
  TScheduler &                  SchedulerRef;
  TButtonController &           Buttons;
  TStatusApi &                  StatusApi;
  TChannelConfigs &             Channels;
  TSwitchTimes &                SwitchTimes;
  TSwitchIntervals &            Intervals;
  TGNString                     DataDir;
  TTimeConfigSync &             TimeConfigSync;
  #if !defined( SYSTEM_ESP )
    std::atomic<bool>            FExitRequested = false;
  #endif
 };
