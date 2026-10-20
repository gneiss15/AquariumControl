// TmrSwWifi.h, Version: 4.06
#ifndef TmrSwWifiH
#define TmrSwWifiH

#include <GnBasics.h>
#include <GnCoopTimer.h>
#include "TmrSwIncludes.h"

class TTmrSwWifi : public TSingleton<TTmrSwWifi>, TLoopInstance
 {
  friend class TSingleton<TTmrSwWifi>;
 private:
                      TTmrSwWifi(void);
 private:
  static constexpr uint16_t FMinutesPerWeek = 7 * 24 * 60;
  static constexpr uint16_t FInvalidMinuteOfWeek = 0xFFFF;

  uint8_t             FSsidNr;
  int                 F500msConnectTicker;
  bool                FFirstTimeValid;
  bool                FSwitchingEnabled;
  WebServer           FHttpServer;
  struct tm           FTm;
  uint16_t            FLastMinuteOfWeek;

 private:
  time_t              GetTime(void);

 protected:
  virtual void        Loop(void);

 public:
  bool                SwitchingEnabled(void)                         { return FSwitchingEnabled; }
  struct tm           GetTm(void);
  void                Every500ms( uint8_t tick );
  bool                Setup(void);
  bool                SetupNetwork(void);
  bool                SetupWeb(void);
  bool                SetupRecovery( char const * ProjectName );
  bool                SetupRecoveryNetwork( char const * ProjectName );
  bool                SetupStartupError( char const * ProjectName, String const & Error );
  bool                TimeIsValid(void)                              { return FTimeIsValid; }

 private: // Wifi
  void                StartSTA( String const & ssid, String const & pw );
  bool                SetupWifi(void);

 private: // NTP
  static bool         FTimeIsValid;
  static void         STimeIsSet_cb(void);
  bool                SetupNTP(void);

 private: // HttpServer
  static void         SHandleNotFound(void);
  static void         SHandleApiOptions(void);
  static void         SHandleDeviceApi(void);
  static void         SHandleStatusApi(void);
  static void         SHandlePowerCalibrationApi(void);
  static void         SHandleSetPowerCalibrationApi(void);
  static void         SHandleTimesApi(void);
  static void         SHandleShortButtonApi(void);
  static void         SHandleCalibrationOverrideApi(void);
  static void         SSendJson( TGNString const & Json );
  static void         SSendPostResponce( String const & Response, bool Reboot );
  static void         SSendPostResponce( bool Ok, bool Reboot );
  static bool         SChkPost( String postName );
  static void         SHandleSetSwitchTimes(void);
  static void         SHandleSetConfig(void);
  static void         SHandleSetChannels(void);
  static void         SHandleResetDevice(void);
  static void         SHandleStartRescue(void);
  static void         SHandleUpdateRescue(void);
  static void         SHandleUpdateRescueUpload(void);
  bool                SelectRescueBoot(void);
  bool                StartRescueFromStartup(void);
  bool                SetupHttpServer(void);
 };

#define TmrSwWifi TTmrSwWifi::Instance()
#endif
