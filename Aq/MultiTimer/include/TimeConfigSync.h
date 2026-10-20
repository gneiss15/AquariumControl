// TimeConfigSync.h, Version: 1.05
#pragma once

#include <GnBasics.h>
#include <GnCoopTimer.h>

class TWebApplication;

class TTimeConfigSync : public TLoopInstance
 {
 public:
                      TTimeConfigSync( TGNString DeviceId, TGNString DataDir );

  void                Application( TWebApplication * Value ) { FApplication = Value; }
  void                LocalChanged(void);
  void                RequestSync(void) { FAttemptDelayMs = 0; FAttemptTimer.Start(); }
  int                 HandleIncoming( TGNString const & Body, TGNString & Response );
  TGNString const &   TimeStamp(void) const { return FTimeStamp; }
  void                RemoteApplied( TGNString const & TimeStamp );

 protected:
  void                Loop(void) override;

 private:
  TGNString            FDeviceId;
  TGNString            FDataDir;
  TGNString            FTimeStamp;
  bool                 FSynced = false;
  TGnElapsedTimerMs    FAttemptTimer;
  TGnCoopTimer_t       FAttemptDelayMs = 0;
  TWebApplication *    FApplication = nullptr;

  TGNString            MetaFile(void) const;
  TGNString            ScheduleFile(void) const;
  void                 LoadMeta(void);
  bool                 SaveMeta(void) const;
  TGNString            BuildPayload(void) const;
  bool                 SyncNow(void);
  static TGNString     NewTimeStamp(void);
  static bool          HttpPost( TGNString const & Body, int & StatusCode, TGNString & Response );
 };
