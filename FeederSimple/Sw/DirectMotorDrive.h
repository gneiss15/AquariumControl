// DirectMotorDrive.h, Version: 1.13
#pragma once

#include <GnBasics.h>
#include <GnCoopTimer.h>
#include "Libs/Common/Feeder/FeederDriveBase.h"
#include "DebouncedInput.h"
#include "HwConfig.FeederSimple.h"

class TDirectMotorDrive : public TSingleton<TDirectMotorDrive>, public TFeederDrive
 {
  friend class TSingleton<TDirectMotorDrive>;

 private:
  enum class TState : uint8_t
   {
    Startup,
    Homing,
    Idle,
    StartFeed,
    WaitHomeLeave,
    WaitHomeReturn,
    Error
   };

  enum class TError : uint8_t
   {
    None,
    InvalidPin,
    PinConflict,
    InvalidButtonTiming,
    InvalidHomeTiming,
    HomingTimeout,
    HomeLeaveTimeout,
    RotationTimeout,
    StoppedOutsideHome
   };


 private:
                    TDirectMotorDrive(void);
  void              Loop(void) override;
  bool              ConfigurationValid(void);
  void              HandleButton(void);
  void              StartHoming(void);
  void              MotorOff(void);
  void              MotorOn(void);
  void              SetError( TError Error );
  void              StatusLedOff(void);
  void              StatusLedOn(void);
  void              StatusLedToggle(void);


 public:
  void              Setup(void) override;
  bool              Feed( uint8_t Portions = 1 ) override;
  bool              Stop(void) override;
  bool              Running(void) const override;
  bool              PinUsed( int Pin ) const override;
  char const *      CapabilitiesJson(void) const override;
  bool              HasInput(void) const override                  { return true; }
  bool              InputState(void) const override;
  char const *      Error(void) const override;
  void              SetNetworkStatus( bool Connected, bool TimeValid, uint8_t Tick ) override;


 private:
  TDebouncedInput   FButton;
  TDebouncedInput   FHome;
  TState            FState;
  TError            FError;
  uint8_t           FRemainingPortions;
  TGnElapsedTimerMs FStateTimer;
  TGnElapsedTimerMs FButtonPressTimer;
  bool              FButtonKnown;
  bool              FButtonApRequested;
  bool              FSetupComplete;
  bool              FStatusLedOn;

 };

#define DirectMotorDrive TDirectMotorDrive::Instance()
