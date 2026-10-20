// DeviceRegistry.h, Version: 1.10
#pragma once
#include <GnBasics.h>
#include <filesystem>
#include <map>
#include <json-c/json.h>

// Functions address configuration; only MAC@Service addresses physical devices.
class TDeviceRegistry
 {
 public:
  struct TDevice { TGNString Function, Id, IP, DeviceType, HardwareName; unsigned Port; };
  explicit TDeviceRegistry( std::filesystem::path const & Root );
  TGNString Function( TGNString const & Id ) const;
  TGNString WireId( TGNString const & Id ) const;
  TGNString HostnameFor( TGNString const & DeviceId, TGNString const & NewFunction ) const;
  bool ValidateIdentity( TGNString const & Function, json_object * Envelope ) const;
  void AddPreparation( TGNString const & Function, json_object * Envelope ) const;
  std::map<TGNString,TDevice> const & Devices(void) const { return FDevices; }
  std::map<TGNString,TGNString> const & ExternalSources(void) const { return FExternalSources; }
  void ApplyAddresses( json_object * Configuration ) const;
  bool Pending(void) const { return FPending; }
  TGNString const & Revision(void) const { return FRevision; }
  bool AcceptWireId( TGNString const & Id ) const;
  bool NeedsAssignment( TGNString const & Function, json_object * Envelope ) const;
  bool Change( TGNString const & First, TGNString const & Second, json_object * Transfer, TGNString & Error );
  bool RestartConfirmed( TGNString const & Function, json_object * Identity ) const;
  bool Complete( TGNString & Error );
  json_object * View(void) const;
  std::filesystem::path const & Root(void) const { return FRoot; }
  bool InitializeEndpoints( json_object * Endpoints, TGNString & Error );
  bool ChangeEndpoints( json_object * Endpoints, json_object * Times, TGNString & Error );
  json_object * EditorView(void) const;
  bool EditDevices( json_object * Rows, json_object * Transfer, TGNString & Error );
 private:
  std::filesystem::path FRoot;
  bool FPending = false;
  TGNString FRevision;
  TGNString FEditorConfiguration;
  bool Save( json_object * Document, TGNString & Error );
  std::map<TGNString,TDevice> FDevices;
  std::map<TGNString,TGNString> FAliases;
  std::map<TGNString,TGNString> FExternalSources;
 };
