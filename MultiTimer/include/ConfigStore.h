// ConfigStore.h, Version: 2.37
#pragma once
#include <GnBasics.h>
#include <GnHeaterControl.h>
#include "Types.h"
#include <vector>

class TConfigStore : public TSingleton<TConfigStore>
 {
  friend class TSingleton<TConfigStore>;

 private:
                           TConfigStore(void) = default;

 public:
  bool                     LoadChannels( const TGNString & FileName, TChannelConfigs & Channels,
                                         THeaterProfiles & HeaterProfiles, TGNString & Error ) const;
  bool                     ChannelsFromJson( const TGNString & Json, TChannelConfigs & Channels,
                                             THeaterProfiles & HeaterProfiles, TGNString & Error ) const;
  bool                     SaveChannels( const TGNString & FileName, const TGNString & Json, TGNString & Error ) const;
  bool                     LoadSchedule( const TGNString & FileName, const TChannelConfigs & Channels,
                                         TSwitchTimes & SwitchTimes, TSwitchIntervals & Intervals,
                                         std::vector<TGNString> & Warnings, TGNString & Error ) const;
  bool                     SaveSchedule( const TGNString & FileName, const TChannelConfigs & Channels,
                                         const TSwitchTimes & SwitchTimes, const TSwitchIntervals & Intervals,
                                         TGNString & Error ) const;
  bool                     LoadScheduleJson( const TGNString & FileName, TGNString & SwitchTimesJson,
                                             TGNString & IntervalsJson, TGNString & Error ) const;
  bool                     SaveScheduleJson( const TGNString & FileName, const TGNString & SwitchTimesJson,
                                             const TGNString & IntervalsJson, TGNString & Error ) const;
  bool                     CheckScheduleHeapSpace( const TChannelConfigs & Channels,
                                                   const TSwitchTimes & SwitchTimes,
                                                   const TSwitchIntervals & Intervals,
                                                   TGNString & Error ) const;
  TGNString                SwitchTimesToJson( const TChannelConfigs & Channels,
                                              const TSwitchTimes & SwitchTimes ) const;
  TGNString                IntervalsToJson( const TChannelConfigs & Channels,
                                            const TSwitchIntervals & Intervals ) const;
  bool                     SwitchTimesFromJson( const TGNString & Json, const TChannelConfigs & Channels,
                                                TSwitchTimes & SwitchTimes, TGNString & Error ) const;
  bool                     IntervalsFromJson( const TGNString & Json, const TChannelConfigs & Channels,
                                              TSwitchIntervals & Intervals, TGNString & Error ) const;

 private:
  static TChannelId        FindChannelId( const TChannelConfigs & Channels, const TGNString & Id );
  static const TGNString & GetChannelConfigId( const TChannelConfigs & Channels, TChannelId Id );
  TGNString                ScheduleToJson( const TChannelConfigs & Channels, const TSwitchTimes & SwitchTimes,
                                           const TSwitchIntervals & Intervals ) const;
 };

#define ConfigStore TConfigStore::Instance()
