// DebouncedInput.cpp, Version: 1.02
#include <GnBasics.h>
#include "DebouncedInput.h"

TDebouncedInput::TDebouncedInput(void)
: FPin( -1 )
, FActiveLow( true )
, FDebounceMs( 0 )
, FRawActive( false )
, FActive( false )
, FValid( false )
, FChanged( false )
 {
 }

void TDebouncedInput::Setup( int Pin, bool ActiveLow, TGnCoopTimer_t DebounceMs, uint8_t InputMode )
 {
  FPin = Pin;
  FActiveLow = ActiveLow;
  FDebounceMs = DebounceMs;
  pinMode( FPin, InputMode );
  Reset();
 }

void TDebouncedInput::Reset(void)
 {
  FRawActive = ReadActive();
  FActive = FRawActive;
  FValid = false;
  FChanged = false;
  FRawChangedTimer.Start();
 }

bool TDebouncedInput::ReadActive(void) const
 {
  return digitalRead( FPin ) == ( FActiveLow ? LOW : HIGH );
 }

void TDebouncedInput::Update(void)
 {
  FChanged = false;
  const bool RawActive = ReadActive();
  if ( RawActive != FRawActive )
   {
    FRawActive = RawActive;
    FRawChangedTimer.Start();
   }

  if ( FRawChangedTimer.Elapsed() < FDebounceMs )
    return;
  if ( FValid && FActive == FRawActive )
    return;

  FActive = FRawActive;
  FValid = true;
  FChanged = true;
 }
