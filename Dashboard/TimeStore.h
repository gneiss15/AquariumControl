// TimeStore.h, Version: 1.03
#pragma once

#include <GnBasics.h>
#include "DeviceRegistry.h"
#include <chrono>
#include <filesystem>
#include <map>

class TTimeStore
 {
 public:
  explicit                  TTimeStore( std::filesystem::path RootDirectory, TDeviceRegistry & Registry );

  bool                      HandleCentralRequest( TGNString const & Method, TGNString const & Body,
                                                  unsigned & StatusCode, TGNString & Response );
  bool                      HandleSyncRequest( TGNString const & Body,
                                               unsigned & StatusCode, TGNString & Response );
  void                      MarkAllPending(void);
  bool                      PrepareAssignment(void);
  void                      Tick(void);

 private:
  struct TEndpoint
   {
    TGNString Host;
    unsigned  Port = 80;
   };

  TDeviceRegistry &        Registry;
  std::filesystem::path     RootDirectory;
  std::filesystem::path     FileName;
  std::filesystem::path     ProfilesFile;
  std::map<TGNString,TEndpoint> Endpoints;
  std::chrono::steady_clock::time_point NextRetry{};

  void                      LoadEndpoints(void);
  void                      TryPushUnsynced(void);
  bool                      TryPushSet( TGNString const & Device, TGNString const & Type );
  bool                      HttpPost( TEndpoint const & Endpoint, TGNString const & Path,
                                      TGNString const & Body, unsigned & StatusCode, TGNString & Response ) const;
 };
