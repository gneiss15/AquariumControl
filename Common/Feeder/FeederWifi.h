// FeederWifi.h, Version: 2.52
//****************************************************************

#ifndef FeederWifiH  // sentry
#define FeederWifiH

//****************************************************************
// Includes
//****************************************************************

#include <GnBasics.h>
#include <GnCoopTimer.h>
#ifdef ESP32
  #include <WiFi.h>
  #include <WebServer.h>
  #include <Update.h>
  using TFeederHttpServer = WebServer;
 #else
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <ESP8266HTTPUpdateServer.h>
  using TFeederHttpServer = ESP8266WebServer;
#endif
#include <LittleFS.h>
#include "../EspUpdate/EspWebUpdate.h"
#include <ArduinoJson.h>


//****************************************************************
// Sw Configuration
//****************************************************************

//****************************************************************
// TFeederWifi
//****************************************************************

class TFeederWifi : public TSingleton<TFeederWifi>, TLoopInstance
 {
  friend class TSingleton<TFeederWifi>;
 private:
                    TFeederWifi(void);
 protected:
  virtual void      Loop(void);

 public:
  bool              Setup(void);
  bool              SetupNetwork(void);
  bool              SetupWeb(void);
#if !defined( ESP32 )
  bool              SetupRecovery( char const * ProjectName );
  bool              SetupRecoveryNetwork( char const * ProjectName );
  bool              SetupStartupError( char const * ProjectName, String const & Error );
#endif
  void              Reconnect(void);
  bool              StartTemporaryAp(void);
  bool              TimeIsValid(void)                                               { return STimeIsValid && !FStartupErrorMode; }

 private: // Wifi
  uint8_t           FSsidNr;
  uint8_t           F500msConnectTicker;
  TGnCoopTimerMs     FWifiCheckTimer;
  TGnCoopTimerMs     FRecoveryBlinkTimer;
  uint8_t           FStatusTick;
  bool              FRecoveryMode;
  bool              FRecoveryFallbackApStarted;
  bool              FStartupErrorMode;
  bool              FTemporaryAp;

  void              StartSTA( String const & Ssid, String const & Password );
  bool              StartRecoveryFallbackAp(void);
  static void       SRecoveryBlinkTimerCb( TGnCoopTimerBase *, uint16_t );
  void              WifiCheckTimerCb( TGnCoopTimerBase * Tmr, uint16_t IntervalsElapsed );
  void              CheckWifiConnection(void);
  bool              SetupWifi(void);

 private: // NTP
  static bool       STimeIsValid;
  static void       STimeIsSet_cb(void);

  bool              SetupNTP(void);

 private: // HttpServer & FwUpdater
  static void       SHandleNotFound(void);
  static void       SHandleApiOptions(void);
  static void       SHandleDeviceApi(void);
  static void       SHandleStatusApi(void);
  static void       SHandleTimesApi(void);
  static void       SHandleOneWireDiag(void);
  static void       SHandleOneWireDiagStats(void);
  static void       SHandleOneWireDiagLog(void);
  static void       SHandleOneWireDiagClear(void);
  static void       SHandleHomeInputTestStatus(void);
  static void       SHandleStartHomeInputTest(void);
  static void       SHandleStopHomeInputTest(void);
  static void       SSendJson( TGNString const & Json );

  static void       SSendPostResponce( String const & Response, bool Reboot );
  static void       SSendPostResponce( bool Ok, bool Reboot );

  static bool       SChkPost( String postName );

  static void       SHandleSetFeedTimes(void);
  static void       SHandleManualFeed(void);
  static void       SHandleStopFeed(void);
  static void       SHandleMFeedChk(void);
  static void       SHandleSetConfig(void);
  static void       SHandleSetServoSteps(void);
  static void       SHandleResetDevice(void);

  TFeederHttpServer FHttpServer;
  TEspWebUpdate     FWebUpdate;
  bool              SetupHttpServer(void);
 };

#define FeederWifi    TFeederWifi::Instance()

//****************************************************************
#endif // sentry
