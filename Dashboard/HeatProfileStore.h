// HeatProfileStore.h, Version: 1.01
#pragma once

#include <GnBasics.h>
#include "DeviceRegistry.h"
#include <chrono>
#include <filesystem>
#include <map>

struct json_object;

class THeatProfileStore
 {
 public:
  explicit                  THeatProfileStore( std::filesystem::path RootDirectory, TDeviceRegistry & Registry );

  bool                      HandleCentralRequest( TGNString const & Method, TGNString const & Body,
                                                  unsigned & StatusCode, TGNString & Response );
  void                      Tick(void);

 private:
  struct TEndpoint
   {
    TGNString Host;
    unsigned  Port = 80;
   };

  TDeviceRegistry &        Registry;
  std::filesystem::path     RootDirectory;
  std::filesystem::path     ProfilesFile;
  std::filesystem::path     SyncFile;
  std::map<TGNString,TEndpoint> Endpoints;
  std::chrono::steady_clock::time_point NextRetry{};

  void                      LoadEndpoints(void);
  json_object *             LoadProfiles(void) const;
  json_object *             LoadSync(void) const;
  bool                      SaveProfiles( json_object * Profiles ) const;
  bool                      SaveSync( json_object * Sync ) const;
  bool                      TryPushUnsynced(void);
  bool                      TryPushSet( json_object * Sync, json_object * Set,
                                         json_object * Profiles );
  bool                      HttpPost( TEndpoint const & Endpoint, TGNString const & Path,
                                      TGNString const & Body, unsigned & StatusCode,
                                      TGNString & Response ) const;
 };
