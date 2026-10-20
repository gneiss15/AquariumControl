// Scheduler.h, Version: 2.41
#pragma once
#include <GnBasics.h>
#include <GnCoopTimer.h>
#include <GnHeaterControl.h>
#include "IHardware.h"
#include "Logger.h"
#include "Types.h"

#ifdef Log
  #undef Log
#endif
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <vector>

class TEvent;
typedef std::vector<TEvent> TEvents;

class TSchedulerChannelStatus
 {
  friend class TScheduler;

 private:
  std::time_t FOverrideUntil = 0;
  std::time_t FNextChangeTime = 0;
  uint8_t     FState : 1;
  uint8_t     FAutomaticState : 1;
  uint8_t     FTimeState : 1;
  uint8_t     FTimeSchedulePresent : 1;
  uint8_t     FHysteresisStateKnown : 1;
  uint8_t     FHysteresisState : 1;
  uint8_t     FOverrideActive : 1;
  uint8_t     FOverrideState : 1;
  uint8_t     FOverrideUntilKnown : 1;
  uint8_t     FNextChangeValid : 1;
  uint8_t     FNextChangeState : 1;

  void         ResetCalculatedValues(void);

 public:
               TSchedulerChannelStatus(void)  : FState( false ), FAutomaticState( false ), FTimeState( false )
                                              , FTimeSchedulePresent( false ), FHysteresisStateKnown( false )
                                              , FHysteresisState( false ), FOverrideActive( false )
                                              , FOverrideState( false ), FOverrideUntilKnown( false )
                                              , FNextChangeValid( false ), FNextChangeState( false )
                                               {}
  bool         State(void) const              { return FState != 0; }
  bool         AutomaticState(void) const     { return FAutomaticState != 0; }
  bool         OverrideActive(void) const     { return FOverrideActive != 0; }
  bool         OverrideState(void) const      { return FOverrideState != 0; }
  bool         OverrideUntilKnown(void) const { return FOverrideUntilKnown != 0; }
  std::time_t  OverrideUntil(void) const      { return FOverrideUntil; }
  bool         NextChangeValid(void) const    { return FNextChangeValid != 0; }
  std::time_t  NextChangeTime(void) const     { return FNextChangeTime; }
  bool         NextChangeState(void) const    { return FNextChangeState != 0; }
 };
typedef std::vector<TSchedulerChannelStatus> TSchedulerChannelStati;

class TScheduler : public TLoopInstance
 {
 public:
                              TScheduler( TIHardware & hardware, const TChannelConfigs & channels,
                                          const THeaterProfiles & heaterProfiles );
                              ~TScheduler(void);
  void                        SetSchedule( const TSwitchTimes & switchTimes, const TSwitchIntervals & intervals );
  void                        SetHeaterProfiles( const THeaterProfiles & heaterProfiles );
  void                        ResetFunctionState( TChannelId Channel );
  bool                        ReconstructState( TGNString & error, bool firstTime = false );
  TSchedulerChannelStati      ChannelStati(void) const;
  bool                        ToggleChannel( TChannelId channel, TGNString & error );
  bool                        ToggleChannel( TChannelId channel, std::chrono::minutes duration, TGNString & error );
  static bool                 GetHeapRequirement( const TSwitchTimes & switchTimes, const TSwitchIntervals & intervals,
                                                  size_t channelCount, size_t & requiredBytes, size_t & largestBlockBytes );
  void                        Loop(void) override;

 private:
  static constexpr uint16_t   FMinutesPerDay = 24 * 60;
  static constexpr uint16_t   FMinutesPerWeek = 7 * FMinutesPerDay;

  static bool                 AddBytes( size_t bytes, size_t & requiredBytes, size_t & largestBlockBytes );
  static bool                 CalcEventCount( const TSwitchTimes & switchTimes, const TSwitchIntervals & intervals,
                                              size_t & eventCount );
  static void                 BuildEvents( const TSwitchTimes & switchTimes, const TSwitchIntervals & intervals,
                                           size_t oneTimeCapacity, TEvents & events );
  void                        CalculateStates( uint16_t minuteOfWeek );
  bool                        UpdateAutomaticState( TChannelId channel, bool writeLog, TGNString & error );
  void                        UpdateHeaters( bool writeLog );
  void                        ApplyEvents( uint16_t minuteOfWeek, bool writeLog );
  bool                        SetOutput( TChannelId channel, bool on, bool writeLog, TGNString & error );
  bool                        SetAllOutputs( bool firstTime, TGNString & error );
  bool                        ValidateOutputChannel( TChannelId channel, TGNString & error ) const;
  void                        RemoveOneTimeEvent( TChannelId channel );
  const TEvent *              FindOneTimeEvent( TChannelId channel ) const;
  void                        InsertOneTimeEvent( const TEvent & event );
  bool                        FindNextEvent( TChannelId channel, std::time_t currentTime, bool initialState,
                                             bool onlyStateChanges, bool includeOneTime, TEvent & nextEvent,
                                             std::time_t & nextTime ) const;

  TIHardware &                FHardware;
  const TChannelConfigs &     FChannels;
  TGnHeaterControl            FHeaterControl;
  TEvents                     FEvents;
  TSchedulerChannelStati      FChannelStati;
  mutable std::mutex          FStateMutex;
  uint16_t                    FLastMinuteOfWeek = 0;
  TGnElapsedTimerMs           FHeaterCheckTimer;
 };
