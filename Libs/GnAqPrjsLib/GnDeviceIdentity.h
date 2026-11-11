// GnDeviceIdentity.h, Version: 1.06
#pragma once
#include <GnBasics.h>
#include <map>
#include <functional>

template<bool> struct GnImpl_TJsonIdentity;
using TJsonIdentity = GnImpl_TJsonIdentity<false>;

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
  TGNString const & Function(void) const { return FFunction.empty() ? FInitialFunction : FFunction; }
  TGNString const & Revision(void) const { return FRevision; }
  TGNString const & AppliedRevision(void) const { return FAppliedRevision; }
  bool Prepared(void) const { return FPrepared; }
  bool LocalSources(void) const { return FLocalSources; }
  void ReportIdentity( TJsonIdentity & State ) const;
  TGNString ReportJson(void) const;
  TGNString UdpInfoJson(void) const;
  TGNString const & DeviceId(void) const { return FDeviceId; }
  TGNString ResolveTemperatureSource( TGNString const & Function ) const;
  TGNString ConfirmedSourceRevision(void) const { return FPrepared && !FLocalSources && FRevision==FAppliedRevision ? FRevision : TGNString{}; }
  std::map<TGNString,TGNString> const & ConfirmedSources(void) const { return FSources; }
  using TSourceResolver = std::function<TGNString(TGNString const &,TGNString const &,TGNString const &)>;
  void SourceResolver(TSourceResolver Resolver) { FSourceResolver=std::move(Resolver); }
  bool SetSources( TGNString const & Json, bool DashboardAvailable );
  uint64_t BootCounter(void) const { return FBootCounter; }
 private:
  bool ApplyState( TJsonIdentity const & State, bool Persist );
  TGNString FDeviceId, FInitialFunction, FStateFile, FFunction, FRevision, FAppliedRevision;
  bool FFunctionChanged = false, FLocalSources = false;
  uint64_t FBootCounter = 0;
  unsigned FServiceId = 0;
  bool FPrepared = false;
  std::map<TGNString,TGNString> FSources;
  TSourceResolver FSourceResolver;
 };

extern TGnDeviceIdentity GnDeviceIdentity;

// Probe from the consumer itself, never trust a browser-provided lock flag.
bool GnDashboardAvailable(void);
