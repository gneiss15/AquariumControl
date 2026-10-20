// TmrSwHw.cpp, Version: 3.07
#include <GnBasics.h>
#include "TmrSwHw.h"
#include "SwitchTimes.h"
#include "TmrSwWifi.h"
#include <cmath>
#include "ShellyPowerMeter.h"

TTmrSwHw::TTmrSwHw(void)
: FRelayStatus( false )
, FRelayStatusKnown( false )
, FWiFiLedStatus( false )
, FPwrLedStatus( false )
, FInpStatus( TmrSwShellyHardware::KeyActiveLevel == LOW ? HIGH : LOW )
 {
 }

/*virtual*/ void TTmrSwHw::Loop(void)
 {
  ShellyPowerMeter.Loop();
  const bool Inp = GetInPin( TmrSwShellyHardware::KeyPin );
  if( Inp == FInpStatus )
    return;
  FInpStatus = Inp;
  if( Inp != TmrSwShellyHardware::KeyActiveLevel )
   {
    if( FButtonPressTimer.Elapsed() >= LongButtonPressMillis )
      TmrSwConfigInstance.Reset();
     else
      HandleShortButtonPress( false );
   }
   else
    FButtonPressTimer.Start();
 }

void TTmrSwHw::Setup(void)
 {
  PinMode_InputPullUp( TmrSwShellyHardware::KeyPin );
  FInpStatus = GetInPin( TmrSwShellyHardware::KeyPin );
  if( TmrSwShellyHardware::RelayConfirmed )
   {
    FRelayStatusKnown = false;
    // Set the inactive latch before connecting the GPIO output to the pad.
    TmrSwShellyHardware::RelayActiveLevel ? ClrPin( TmrSwShellyHardware::RelayPin ) : SetPin( TmrSwShellyHardware::RelayPin );
    PinMode_Output( TmrSwShellyHardware::RelayPin );
    FRelayStatus = false;
    FRelayStatusKnown = true;
   }
  if( TmrSwShellyHardware::StatusLedConfirmed )
   {
    const int Pins[] = { TmrSwShellyHardware::RedLedPin, TmrSwShellyHardware::StatusLedPin, TmrSwShellyHardware::BlueLedPin };
    for( int Pin : Pins )
     {
      SetPin( Pin );
      PinMode_Output( Pin );
     }
    const bool GreenReady = ledcAttach( TmrSwShellyHardware::StatusLedPin, 1000, 8 );
    const bool BlueReady = ledcAttach( TmrSwShellyHardware::BlueLedPin, 1000, 8 );
    if( GreenReady ) ledcWrite( TmrSwShellyHardware::StatusLedPin, TmrSwShellyHardware::StatusLedOffDuty );
    if( BlueReady ) ledcWrite( TmrSwShellyHardware::BlueLedPin, TmrSwShellyHardware::StatusLedOffDuty );
    FStatusLedReady = GreenReady && BlueReady;
   }
  ShellyPowerMeter.Setup();
 }

void TTmrSwHw::RelayOff(void)
 {
  if( !TmrSwShellyHardware::RelayConfirmed ) return;
  TmrSwShellyHardware::RelayActiveLevel ? ClrPin( TmrSwShellyHardware::RelayPin ) : SetPin( TmrSwShellyHardware::RelayPin );
  FRelayStatus = false;
  FRelayStatusKnown = true;

 }

void TTmrSwHw::RelayOn(void)
 {
  if( !TmrSwShellyHardware::RelayConfirmed ) return;
  TmrSwShellyHardware::RelayActiveLevel ? SetPin( TmrSwShellyHardware::RelayPin ) : ClrPin( TmrSwShellyHardware::RelayPin );
  FRelayStatus = true;
  FRelayStatusKnown = true;

 }

bool TTmrSwHw::HandleShortButtonPress( bool Simulated )
 {
  if( !SwitchTimes.ManualToggle() )
    return false;
  Debug( "%s short button press\n", Simulated ? "Simulated" : "Physical" );
  return true;
 }

bool TTmrSwHw::SimulateShortButtonPress(void)
 {
  return HandleShortButtonPress( true );
 }

void TTmrSwHw::RelayToggle(void)
 {
  RelaySet( !FRelayStatus );
 }

void TTmrSwHw::RelaySet( bool On )
 {
  if( On )
    RelayOn();
   else
    RelayOff();
 }

bool TTmrSwHw::ButtonPressed(void) const
 {
  return FInpStatus == TmrSwShellyHardware::KeyActiveLevel;
 }

void TTmrSwHw::StatusLedWaiting( bool Waiting )
 {
  if( FStatusLedWaiting == Waiting ) return;
  WiFiLedOff();
  FStatusLedWaiting = Waiting;
 }

void TTmrSwHw::WiFiLedOff(void)
 {
  if( FStatusLedReady ) ledcWrite( FStatusLedWaiting ? TmrSwShellyHardware::BlueLedPin : TmrSwShellyHardware::StatusLedPin, TmrSwShellyHardware::StatusLedOffDuty );
  FWiFiLedStatus = false;
 }

void TTmrSwHw::WiFiLedOn(void)
 {
  if( FStatusLedReady ) ledcWrite( FStatusLedWaiting ? TmrSwShellyHardware::BlueLedPin : TmrSwShellyHardware::StatusLedPin, TmrSwShellyHardware::StatusLedOnDuty );
  FWiFiLedStatus = true;
 }

void TTmrSwHw::WiFiLedToggle(void)
 {
  FWiFiLedStatus ? WiFiLedOff() : WiFiLedOn();
 }

void TTmrSwHw::PwrLedOff(void)
 {
  FPwrLedStatus = false;
 }

void TTmrSwHw::PwrLedOn(void)
 {
  FPwrLedStatus = true;
 }

void TTmrSwHw::PwrLedToggle(void)
 {
  FPwrLedStatus ? PwrLedOff() : PwrLedOn();
 }

bool TTmrSwHw::PowerMeterValid(void) const { return ShellyPowerMeter.Valid(); }
uint32_t TTmrSwHw::PowerMeterCfEdges(void) const { return 0; }
uint32_t TTmrSwHw::PowerMeterCf1Edges(void) const { return 0; }
uint32_t TTmrSwHw::PowerMeterVoltageIntervals(void) const { return 0; }
uint32_t TTmrSwHw::PowerMeterCurrentIntervals(void) const { return 0; }
int TTmrSwHw::PowerMeterSelectorLevel(void) const { return -1; }
bool TTmrSwHw::PowerMeterLoadEnabled(void) const { return FRelayStatus; }
float TTmrSwHw::Voltage(void) const { return ShellyPowerMeter.Voltage(); }
float TTmrSwHw::Current(void) const { return ShellyPowerMeter.Current(); }
float TTmrSwHw::ActivePower(void) const { return ShellyPowerMeter.Power(); }
