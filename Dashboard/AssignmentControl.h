// AssignmentControl.h, Version: 1.00
#pragma once
#include <GnBasics.h>
#include "DeviceRegistry.h"
#include "TimeStore.h"

// Atomic desired-state journal; completion requires fresh acknowledgements from all clients.
class TAssignmentControl
 {
 public:
  TAssignmentControl( TDeviceRegistry & Registry, TTimeStore & Times );
  void Tick(void);
  void Handle( TGNString const & Method, TGNString const & Body, unsigned & Code, TGNString & Response );
 private:
  TDeviceRegistry & FRegistry;
  TTimeStore & FTimes;
  std::chrono::steady_clock::time_point FNextInspect{};
  json_object * Inspect(void);
 };
