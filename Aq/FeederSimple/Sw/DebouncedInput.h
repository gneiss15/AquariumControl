// DebouncedInput.h, Version: 1.04
#pragma once

#include <GnBasics.h>
#include <GnCoopTimer.h>

class TDebouncedInput
 {
 public:
                    TDebouncedInput(void);
  void              Setup( int Pin, bool ActiveLow, TGnCoopTimer_t DebounceMs, uint8_t InputMode );
  void              Reset(void);
  void              Update(void);
  bool              Valid(void) const                              { return FValid; }
  bool              Active(void) const                             { return FActive; }
  bool              Changed(void) const                            { return FChanged; }

 private:
  bool              ReadActive(void) const;

 private:
  int               FPin;
  bool              FActiveLow;
  TGnCoopTimer_t    FDebounceMs;
  bool              FRawActive;
  bool              FActive;
  bool              FValid;
  bool              FChanged;
  TGnElapsedTimerMs FRawChangedTimer;
 };
