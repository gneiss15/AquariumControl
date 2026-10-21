// TimeSync.h, Version: 2.37
#pragma once
#include <GnBasics.h>

enum class TTimeSyncState : uint8_t
 {
  Synchronized,
  NotSynchronized,
  QueryUnavailable
 };

class TTimeSync
 {
 public:
  static TTimeSyncState State(void);
  static bool           Wait( TTimeSyncState & state );
  static char const *   Error( TTimeSyncState state );

 private:
  static bool           TimeLooksValid(void);
  static TTimeSyncState NativeState(void);
 };
