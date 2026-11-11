// GnTimeSync.h, Version: 1.07
#pragma once

#include <GnBasics.h>
#include <GnCoopTimer.h>

#if defined( GNPlatformESP )

struct TGnTimeSyncConfig
 {
  char const *        MetaFile;
  char const *        DataFile;
  char const *        Type;
  char const *        Format;
  uint16_t            HttpTimeoutMilliseconds;
  char const *        ChannelFile = nullptr;
  char const *        IntervalFile = nullptr;
  String (*ApplyFunctionConfig)( char const * Envelope ) = nullptr;
 };

class TGnTimeSync : public TSingleton<TGnTimeSync>, public TLoopInstance
 {
  friend class TSingleton<TGnTimeSync>;

 public:
  using TApply = String (*)( char const * Content );

 private:
                      TGnTimeSync(void);

 public:
  void                Setup( TGnTimeSyncConfig const & Config,
                             String const & Hostname, TApply Apply );
  void                LocalChanged(void);
  void                RequestSync(void) { ScheduleSync( 1 ); }
  int                 HandleIncoming( TGNString const & Body, TGNString & Response );

 protected:
  void                Loop(void) override;

 private:
  TGnTimeSyncConfig   FConfig = {};
  String              FHostname;
  TApply              FApply = nullptr;
  TGNString            FTimeStamp;
  bool                 FSynced = false;
  bool                 FSetup = false;
  TGnCoopTimerMs       FSyncTimer;
  bool                 FSyncDue = false;
  bool                 FRestartPending = false;
  uint32_t             FRestartAt = 0;
  bool                 ApplyFunctionConfig( TGNString const & Envelope );

  void                LoadMeta(void);
  bool                SaveMeta(void) const;
  void                RemoteApplied( TGNString const & TimeStamp );
  bool                SyncNow(void);
  TGNString            BuildPayload(void) const;
  static TGNString     NewTimeStamp(void);
  bool                HttpPost( TGNString const & Url, TGNString const & Body,
                                int & StatusCode, TGNString & Response ) const;
  void                ScheduleSync( TGnCoopTimer_t DelayMilliseconds );
  void                SyncTimerCb( TGnCoopTimerBase * Tmr, uint16_t IntervalsElapsed );
 };

#define GnTimeSync TGnTimeSync::Instance()

#endif
