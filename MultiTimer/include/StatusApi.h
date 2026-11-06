// StatusApi.h, Version: 2.44
#pragma once
#include <GnBasics.h>
#include <GnCoopTimer.h>
#include "IHardware.h"
#include "Scheduler.h"
#include "Types.h"
#include "TimeSync.h"
#include "RoomTemperature.h"
#include <ctime>
#include <mutex>
#include <vector>

class TStatusApi
 {
 public:
                                                    TStatusApi( TIHardware & hardware, TScheduler & scheduler,
                                                                const TChannelConfigs & channels, TGNString deviceId,
                                                                TGNString name, unsigned webSocketPort,
                                                                std::vector<TGNString> configErrors, TRoomTemperature * RoomTemperature = nullptr );
  TGNString                                         DeviceJson(void) const;
  void                                              ClearConfigErrors(void);
  TGNString                                         StatusJson(void);
  TGNString                                         StatusJson( uint64_t & signature );

 private:
  static constexpr unsigned                         FApiVersion = 1;
  static constexpr unsigned                         FPollingFallbackSeconds = 5;
  static constexpr TGnCoopTimer_t                   FTimeSyncCacheMs = 5000;
  static constexpr char                             FDeviceType[] = "MultiTimer";

  TGNString                                         BuildStatusJson( uint64_t * signature );
  static uint64_t                                   StatusSignature( TGNString const & json );
  void                                              UpdateTimeSyncStatus(void);
  static const char *                               Platform(void);
  static void                                       AppendInteger( TGNString & json, long long value );

  TRoomTemperature *                                FRoomTemperature;
  TIHardware &                                      FHardware;
  TScheduler &                                      FScheduler;
  const TChannelConfigs &                           FChannels;
  TGNString                                         FDeviceId;
  TGNString                                         FName;
  unsigned                                          FWebSocketPort;
  std::vector<TGNString>                            FConfigErrors;
  std::time_t                                       FOnlineSince;
  std::mutex                                        FStatusMutex;
  bool                                              FTimeSyncCached = false;
  TTimeSyncState                                    FCachedTimeSyncState = TTimeSyncState::NotSynchronized;
  TGnElapsedTimerMs                                 FTimeSyncCacheTimer;
 };
