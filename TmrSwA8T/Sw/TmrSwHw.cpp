// TmrSwHw.cpp, Version: 3.03
#include <GnBasics.h>
#include "TmrSwHw.h"
#include "SwitchTimes.h"
#include "TmrSwWifi.h"
#include "TmrSwA8TPowerMeter.h"

TTmrSwHw::TTmrSwHw(void)
: FRelayStatus( false )
, FRelayStatusKnown( false )
, FWiFiLedStatus( false )
, FPwrLedStatus( false )
, FInpStatus( TmrSwA8THardware::KeyActiveLevel == LOW ? HIGH : LOW )
 {
 }

/*virtual*/ void TTmrSwHw::Loop(void)
 {
  const bool Inp = GetInPin( TmrSwA8THardware::KeyPin );
  if( Inp == FInpStatus )
    return;
  FInpStatus = Inp;
  if( Inp != TmrSwA8THardware::KeyActiveLevel )
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
  FRelayStatusKnown = false;
  // Preload the inactive latches before enabling either output.
  TmrSwA8THardware::RelayActiveLevel ? ClrPin( TmrSwA8THardware::RelayPin ) : SetPin( TmrSwA8THardware::RelayPin );
  TmrSwA8THardware::StatusLedActiveLevel ? ClrPin( TmrSwA8THardware::StatusLedPin ) : SetPin( TmrSwA8THardware::StatusLedPin );
  PinMode_Output( TmrSwA8THardware::RelayPin );
  PinMode_Output( TmrSwA8THardware::StatusLedPin );
  FRelayStatus = false;
  FRelayStatusKnown = true;
  FWiFiLedStatus = false;

  PinMode_InputPullUp( TmrSwA8THardware::KeyPin );
  FInpStatus = GetInPin( TmrSwA8THardware::KeyPin );

  TmrSwA8TPowerMeter.Setup();
  TmrSwA8TPowerMeter.LoadEnabled( false );
 }

void TTmrSwHw::RelayOff(void)
 {
  TmrSwA8THardware::RelayActiveLevel ? ClrPin( TmrSwA8THardware::RelayPin ) : SetPin( TmrSwA8THardware::RelayPin );
  FRelayStatus = false;
  FRelayStatusKnown = true;
  TmrSwA8TPowerMeter.LoadEnabled( false );
 }

void TTmrSwHw::RelayOn(void)
 {
  TmrSwA8THardware::RelayActiveLevel ? SetPin( TmrSwA8THardware::RelayPin ) : ClrPin( TmrSwA8THardware::RelayPin );
  FRelayStatus = true;
  FRelayStatusKnown = true;
  TmrSwA8TPowerMeter.LoadEnabled( true );
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
  return FInpStatus == TmrSwA8THardware::KeyActiveLevel;
 }

void TTmrSwHw::WiFiLedOff(void)
 {
  TmrSwA8THardware::StatusLedActiveLevel ? ClrPin( TmrSwA8THardware::StatusLedPin ) : SetPin( TmrSwA8THardware::StatusLedPin );
  FWiFiLedStatus = false;
 }

void TTmrSwHw::WiFiLedOn(void)
 {
  TmrSwA8THardware::StatusLedActiveLevel ? SetPin( TmrSwA8THardware::StatusLedPin ) : ClrPin( TmrSwA8THardware::StatusLedPin );
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

bool TTmrSwHw::PowerMeterValid(void) const
 {
  return TmrSwA8TPowerMeter.Valid();
 }

uint32_t TTmrSwHw::PowerMeterCfEdges(void) const
 {
  return TmrSwA8TPowerMeter.CfEdges();
 }

uint32_t TTmrSwHw::PowerMeterCf1Edges(void) const
 {
  return TmrSwA8TPowerMeter.Cf1Edges();
 }

uint32_t TTmrSwHw::PowerMeterVoltageIntervals(void) const
 {
  return TmrSwA8TPowerMeter.VoltageIntervals();
 }

uint32_t TTmrSwHw::PowerMeterCurrentIntervals(void) const
 {
  return TmrSwA8TPowerMeter.CurrentIntervals();
 }

int TTmrSwHw::PowerMeterSelectorLevel(void) const
 {
  return TmrSwA8TPowerMeter.SelectorLevel();
 }

bool TTmrSwHw::PowerMeterLoadEnabled(void) const
 {
  return TmrSwA8TPowerMeter.LoadEnabled();
 }

float TTmrSwHw::Voltage(void) const
 {
  return TmrSwA8TPowerMeter.Voltage();
 }

float TTmrSwHw::Current(void) const
 {
  return TmrSwA8TPowerMeter.Current();
 }

float TTmrSwHw::ActivePower(void) const
 {
  return TmrSwA8TPowerMeter.ActivePower();
 }
