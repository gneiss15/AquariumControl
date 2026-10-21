// GnDeviceIdentity.h, Version: 1.02
#pragma once
#include <GnBasics.h>
#include <map>

// MAC/service identity is fixed; functions and source mappings are persisted separately.
class TGnDeviceIdentity
 {
 public:
  void Setup( TGNString const & InitialFunction, unsigned ServiceId, TGNString const & StateFile );
  bool ApplyEnvelope( TGNString const & Envelope );
  bool CommitAssignment(void);
  bool AssignmentPending( TGNString const & Envelope ) const;
  bool FunctionChanged(void) const { return FFunctionChanged; }
  TGNString WireId(void) const { return FDeviceId; }
  TGNString Function(void) const { return FFunction.empty() ? FInitialFunction : FFunction; }
  TGNString ReportJson(void) const;
  TGNString const & DeviceId(void) const { return FDeviceId; }
  TGNString ResolveTemperatureSource( TGNString const & Function ) const;
  bool SetSources( TGNString const & Json, bool DashboardAvailable );
  uint64_t BootCounter(void) const { return FBootCounter; }
 private:
  bool ApplyState( TGNString const & Text, bool Persist );
  TGNString FDeviceId, FInitialFunction, FStateFile, FFunction, FRevision, FAppliedRevision;
  bool FFunctionChanged = false, FLocalSources = false;
  uint64_t FBootCounter = 0;
  unsigned FServiceId = 0;
  bool FPrepared = false;
  std::map<TGNString,TGNString> FSources;
 };

extern TGnDeviceIdentity GnDeviceIdentity;

// Probe from the consumer itself, never trust a browser-provided lock flag.
bool GnDashboardAvailable(void);
