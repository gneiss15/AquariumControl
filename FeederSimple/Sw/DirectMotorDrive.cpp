// DirectMotorDrive.cpp, Version: 1.15
#include "DirectMotorDrive.h"

#include "FeederConfig.h"
#include "FeederWifi.h"
#include "HwConfig.FeederSimple.h"

namespace
 {
bool ValidPin( int Pin )
 {
  return Pin >= 0 && Pin <= 15;
 }

 } // namespace

TDirectMotorDrive::TDirectMotorDrive(void)
: FState( TState::Startup )
, FError( TError::None )
, FRemainingPortions( 0 )
, FButtonKnown( false )
, FButtonApRequested( false )
, FSetupComplete( false )
, FStatusLedOn( false )
 {
 }

bool TDirectMotorDrive::ConfigurationValid(void)
 {
  if( !ValidPin( FeederConfig.ButtonPin )
      || !ValidPin( FeederConfig.HomePin )
      || !ValidPin( FeederConfig.MotorPin )
      || !ValidPin( StatusLedPin ) )
   {
    FError = TError::InvalidPin;
    return false;
   }

  if( FeederConfig.ButtonPin == FeederConfig.HomePin
      || FeederConfig.ButtonPin == FeederConfig.MotorPin
      || FeederConfig.ButtonPin == StatusLedPin
      || FeederConfig.HomePin == FeederConfig.MotorPin
      || FeederConfig.HomePin == StatusLedPin
      || FeederConfig.MotorPin == StatusLedPin )
   {
    FError = TError::PinConflict;
    return false;
   }

  if( FeederConfig.ButtonDebounceMs == 0
      || FeederConfig.ButtonLongPressMs <= FeederConfig.ButtonDebounceMs
      || FeederConfig.ButtonResetPressMs <= FeederConfig.ButtonLongPressMs )
   {
    FError = TError::InvalidButtonTiming;
    return false;
   }

  if( FeederConfig.HomeDebounceMs == 0
      || FeederConfig.HomeLeaveTimeoutMs <= FeederConfig.HomeDebounceMs
      || FeederConfig.RotationTimeoutMs <= FeederConfig.HomeDebounceMs
      || FeederConfig.HomingTimeoutMs <= FeederConfig.HomeDebounceMs )
   {
    FError = TError::InvalidHomeTiming;
    return false;
   }

  return true;
 }

void TDirectMotorDrive::Setup(void)
 {
  FError = TError::None;
  FState = TState::Startup;
  FSetupComplete = false;
  FButtonKnown = false;
  FButtonApRequested = false;
  if( !ConfigurationValid() )
    return;

  MotorOff();
  pinMode( FeederConfig.MotorPin, OUTPUT );

  digitalWrite( StatusLedPin, LOW );
  pinMode( StatusLedPin, OUTPUT );
  FStatusLedOn = false;

  FButton.Setup
   (
    FeederConfig.ButtonPin,
    true,
    FeederConfig.ButtonDebounceMs,
    INPUT_PULLUP
   );
  FHome.Setup
   (
    FeederConfig.HomePin,
    FeederSimpleHomeActiveLow != 0,
    FeederConfig.HomeDebounceMs,
    FeederSimpleHomeUsePullup ? INPUT_PULLUP : INPUT
   );
  FSetupComplete = true;
 }

void TDirectMotorDrive::MotorOff(void)
 {
  digitalWrite( FeederConfig.MotorPin, LOW );
 }

void TDirectMotorDrive::MotorOn(void)
 {
  digitalWrite( FeederConfig.MotorPin, HIGH );
 }

void TDirectMotorDrive::SetError( TError Error )
 {
  MotorOff();
  StatusLedOff();
  FRemainingPortions = 0;
  FError = Error;
  FState = TState::Error;
 }



void TDirectMotorDrive::StartHoming(void)
 {
  MotorOn();
  FState = TState::Homing;
  FStateTimer.Start();
 }

bool TDirectMotorDrive::Feed( uint8_t Portions )
 {

  if( Portions == 0
      || FError != TError::None
      || FState != TState::Idle
      || !FHome.Valid()
      || !FHome.Active() )
    return false;

  FRemainingPortions = Portions;
  FState = TState::StartFeed;
  return true;
 }

bool TDirectMotorDrive::Stop(void)
 {
  if( !FSetupComplete )
    return false;


  MotorOff();
  StatusLedOff();
  FRemainingPortions = 0;
  if( FError != TError::None )
    return true;
  if( FHome.Valid() && FHome.Active() )
    FState = TState::Idle;
   else
    SetError( TError::StoppedOutsideHome );
  return true;
 }

bool TDirectMotorDrive::Running(void) const
 {

  return FState == TState::Homing
      || FState == TState::StartFeed
      || FState == TState::WaitHomeLeave
      || FState == TState::WaitHomeReturn;
 }

bool TDirectMotorDrive::PinUsed( int Pin ) const
 {
  return Pin == FeederConfig.ButtonPin
      || Pin == FeederConfig.HomePin
      || Pin == FeederConfig.MotorPin
      || Pin == StatusLedPin;
 }

char const * TDirectMotorDrive::CapabilitiesJson(void) const
 {
  return ",\"DirectMotor\",\"HomeSensor\",\"LocalButton\",\"StatusLed\",\"Homing\"";
 }

bool TDirectMotorDrive::InputState(void) const
 {
  return FButton.Valid() && FButton.Active();
 }

char const * TDirectMotorDrive::Error(void) const
 {

  switch( FError )
   {
    case TError::None:
      return "";
    case TError::InvalidPin:
      return "Invalid feeder rotor pin";
    case TError::PinConflict:
      return "Feeder rotor pin conflict";
    case TError::InvalidButtonTiming:
      return "Invalid feeder button timing";
    case TError::InvalidHomeTiming:
      return "Invalid feeder home timing";
    case TError::HomingTimeout:
      return "Feeder rotor homing timeout";
    case TError::HomeLeaveTimeout:
      return "Feeder rotor did not leave home";
    case TError::RotationTimeout:
      return "Feeder rotor did not return home";
    case TError::StoppedOutsideHome:
      return "Feeder rotor stopped outside home";
   }
  return "Unknown feeder drive error";
 }

void TDirectMotorDrive::StatusLedOff(void)
 {
  digitalWrite( StatusLedPin, LOW );
  FStatusLedOn = false;
 }

void TDirectMotorDrive::StatusLedOn(void)
 {
  digitalWrite( StatusLedPin, HIGH );
  FStatusLedOn = true;
 }

void TDirectMotorDrive::StatusLedToggle(void)
 {
  if( FStatusLedOn )
    StatusLedOff();
   else
    StatusLedOn();
 }

void TDirectMotorDrive::SetNetworkStatus( bool Connected, bool TimeValid, uint8_t Tick )
 {
  if( !FSetupComplete )
    return;
  if( FButton.Valid() && FButton.Active()
      && FButtonPressTimer.Elapsed() >= FeederConfig.ButtonLongPressMs )
    return;

  if( FState == TState::StartFeed
      || FState == TState::WaitHomeLeave
      || FState == TState::WaitHomeReturn )
   {
    StatusLedOn();
    return;
   }

  if( !Connected )
    StatusLedToggle();
   else if( !TimeValid )
   {
    if( Tick & 1 )
      StatusLedToggle();
   }
   else
    StatusLedOff();
 }

void TDirectMotorDrive::HandleButton(void)
 {
  FButton.Update();
  if( !FButton.Valid() )
    return;

  if( !FButtonKnown )
   {
    FButtonKnown = true;
    if( FButton.Active() )
      FButtonPressTimer.Start();
    return;
   }
  if( FButton.Active() )
   {
    if( FButton.Changed() )
     {
      FButtonApRequested = false;
      FButtonPressTimer.Start();
     }
    const TGnCoopTimer_t PressDuration = FButtonPressTimer.Elapsed();
    if( PressDuration >= FeederConfig.ButtonResetPressMs )
     {
      StatusLedOff();
      if( !FButtonApRequested )
       {
        FButtonApRequested = true;
        FeederWifi.StartTemporaryAp();
       }
     }
     else if( PressDuration >= FeederConfig.ButtonLongPressMs )
     {
      // Fast feedback must not be overwritten by the network status timer.
      if( ( PressDuration / 100 ) & 1 )
        StatusLedOn();
       else
        StatusLedOff();
     }
    return;
   }
  if( !FButton.Changed() )
    return;

  const TGnCoopTimer_t PressDuration = FButtonPressTimer.Elapsed();
  if( FButtonApRequested )
    return; // Never feed or reconnect again when the rescue press is released.
  if( PressDuration >= FeederConfig.ButtonResetPressMs )
    FeederWifi.StartTemporaryAp();
   else if( PressDuration >= FeederConfig.ButtonLongPressMs )
    FeederWifi.Reconnect();
   else
    Feed();
 }

void TDirectMotorDrive::Loop(void)
 {

  if( FError != TError::None )
   {
    if( FSetupComplete )
      HandleButton();
    return;
   }

  FHome.Update();

  switch( FState )
   {
    case TState::Startup:
      if( FHome.Valid() )
       {
        if( FHome.Active() )
          FState = TState::Idle;
         else
          StartHoming();
       }
      break;

    case TState::Homing:
      if( FHome.Valid() && FHome.Active() )
       {
        MotorOff();
        FState = TState::Idle;
       }
       else if( FStateTimer.Elapsed() >= FeederConfig.HomingTimeoutMs )
        SetError( TError::HomingTimeout );
      break;

    case TState::Idle:
      if( FHome.Valid() && !FHome.Active() )
        StartHoming();
      break;

    case TState::StartFeed:
      StatusLedOn();
      MotorOn();
      FState = TState::WaitHomeLeave;
      FStateTimer.Start();
      break;

    case TState::WaitHomeLeave:
      if( FHome.Valid() && !FHome.Active() )
       {
        FState = TState::WaitHomeReturn;
        FStateTimer.Start();
       }
       else if( FStateTimer.Elapsed() >= FeederConfig.HomeLeaveTimeoutMs )
        SetError( TError::HomeLeaveTimeout );
      break;

    case TState::WaitHomeReturn:
      if( FHome.Valid() && FHome.Active() )
       {
        if( FRemainingPortions > 0 )
          --FRemainingPortions;
        if( FRemainingPortions == 0 )
         {
          MotorOff();
          StatusLedOff();
          FState = TState::Idle;
         }
         else
         {
          FState = TState::WaitHomeLeave;
          FStateTimer.Start();
         }
       }
       else if( FStateTimer.Elapsed() >= FeederConfig.RotationTimeoutMs )
        SetError( TError::RotationTimeout );
      break;

    case TState::Error:
      break;
   }
  HandleButton();
 }

