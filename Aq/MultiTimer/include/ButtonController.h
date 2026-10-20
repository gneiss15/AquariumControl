// ButtonController.h, Version: 1.11
#pragma once
#include <GnBasics.h>
#include <GnCoopTimer.h>
#include "IHardware.h"
#include "Logger.h"
#include "Scheduler.h"
#include "Types.h"
#include <vector>

class TButtonController : public TLoopInstance
 {
 public:
                                           TButtonController( TIHardware & hardware, TScheduler & timer,
                                                              const TChannelConfigs & channels );
  void                                     Loop(void) override;
  bool                                     SimulatePress( unsigned channelNr, bool longPress, TGNString & error );

 private:
  struct TButtonState
   {
    bool              Initialized      = false;
    bool              Pressed          = false;
    bool              LongPressHandled = false;
    TGnElapsedTimerMs PressTimer;
	TGnCoopTimer_t    LongPressMs;
   };

  typedef std::vector<TButtonState>        TButtonStates;

  void                                     HandlePressed( unsigned channelNr );
  void                                     HandleReleased( unsigned channelNr, TGnCoopTimer_t pressDurationMs );
  void                                     HandleLongPress( unsigned channelNr );
  void                                     HandlePressStartEffect(void);
  bool                                     ExecuteShortPress( unsigned channelNr, TGNString & error );
  bool                                     ExecuteLongPress( unsigned channelNr, TGNString & error );
  bool                                     ValidateActionChannel( unsigned channelNr, TGNString & error ) const;

  TIHardware &                             Hardware;
  TScheduler &                             Timer;
  const TChannelConfigs &                  Channels;
  TButtonStates                            States;
  bool                                     ShutdownPending = false;
 };
