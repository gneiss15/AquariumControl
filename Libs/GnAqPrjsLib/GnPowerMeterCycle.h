// GnPowerMeterCycle.h, Version: 1.01
#pragma once

#include <cstdint>

// Stage independent metering windows; only complete cycles become observable.
class TGnPowerMeterCycle
 {
 public:
  enum TField : unsigned { Voltage, Current, Power };
  struct TSample { float Voltage = 0, Current = 0, Power = 0; };

  void Reset(void) { FPendingMask = 0; FValid = false; }
  void Add( TField Field, float Value, bool Good, uint32_t Now )
   {
    if( !Good ) { Reset(); return; }
    if( FPendingMask && uint32_t( Now - FPendingSince ) > 7500 ) FPendingMask = 0;
    if( !FPendingMask ) FPendingSince = Now;
    if( Field == Voltage ) FPending.Voltage = Value;
    else if( Field == Current ) FPending.Current = Value;
    else FPending.Power = Value;
    FPendingMask |= 1U << Field;
    if( FPendingMask == 7 )
     {
      FSample = FPending;
      FCompletedAt = Now;
      FValid = true;
      FPendingMask = 0;
     }
   }
  bool Valid( uint32_t Now ) const { return FValid && uint32_t( Now - FCompletedAt ) < 15000; }
  uint32_t AgeMs( uint32_t Now ) const { return uint32_t(Now-FCompletedAt); }
  TSample const & Sample(void) const { return FSample; }

 private:
  TSample FPending, FSample;
  unsigned FPendingMask = 0;
  uint32_t FPendingSince = 0, FCompletedAt = 0;
  bool FValid = false;
 };
