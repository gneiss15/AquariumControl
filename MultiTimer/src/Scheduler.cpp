// Scheduler.cpp, Version: 2.43
#include <GnBasics.h>
#include "Scheduler.h"
#include <algorithm>
#include <climits>

//****************************************************************
// TEvent
//****************************************************************

class TEvent
 {
 private:
  uint16_t   FMinuteOfWeek : 14;
  uint16_t   FOn : 1;
  uint16_t   FOneTime : 1;
  TChannelId FChannel;

 public:
             TEvent(void)                                       : FMinuteOfWeek( 0 ), FOn( false ), FOneTime( false ), FChannel( InvalidChannelId ) {}
             TEvent( uint16_t minuteOfWeek, TChannelId channel,
                     bool on, bool oneTime )                    : FMinuteOfWeek( minuteOfWeek ), FOn( on ), FOneTime( oneTime ), FChannel( channel ) {}
  uint16_t   MinuteOfWeek(void) const                           { return FMinuteOfWeek; }
  TChannelId Channel(void) const                                { return FChannel; }
  bool       On(void) const                                     { return FOn != 0; }
  bool       OneTime(void) const                                { return FOneTime != 0; }
 };

static_assert( sizeof( TEvent ) == 4 );

//****************************************************************
// TSwitchDef
//****************************************************************

class TSwitchDef
 {
 private:
  TSwitchRecord FSwitchRecord{};

 public:
  void          Init( uint16_t minuteOfDay, TChannelId channel,
                      bool on, uint8_t wdayMask );
  size_t        EventCount(void) const;
  void          AddSwitchTimes( TEvents & events ) const;
 };

void TSwitchDef::Init( uint16_t minuteOfDay, TChannelId channel, bool on, uint8_t wdayMask )
 {
  FSwitchRecord.MinuteOfDay = minuteOfDay;
  FSwitchRecord.Channel = channel;
  FSwitchRecord.On = on;
  FSwitchRecord.wday.mask = wdayMask;
 }

size_t TSwitchDef::EventCount(void) const
 {
  if( !FSwitchRecord.wday.bits.Active )
    return 0;
  size_t count = 0;
  uint8_t days = FSwitchRecord.wday.mask & 0x7F;
  while( days )
   {
    count += days & 1;
    days >>= 1;
   }
  return count;
 }

void TSwitchDef::AddSwitchTimes( TEvents & events ) const
 {
  if( !FSwitchRecord.wday.bits.Active )
    return;
  for( uint8_t wday = 0;  wday < 7;  ++wday )
    if( FSwitchRecord.wday.mask & ( 1u << wday ) )
      events.emplace_back( GnMinuteOfWeek( FSwitchRecord.MinuteOfDay, wday ),
                           FSwitchRecord.Channel, FSwitchRecord.On != 0, false );
 }

//****************************************************************
// TSwitchIntervalDef
//****************************************************************

class TSwitchIntervalDef
 {
 private:
  static constexpr uint16_t FMinutesPerDay = 24 * 60;
  static constexpr uint16_t FMinutesPerWeek = 7 * FMinutesPerDay;

  uint16_t   FStartMinuteOfDay = 0;
  uint16_t   FPeriodMinutes = 0;
  uint16_t   FOnMinutes = 0;
  TChannelId FChannel = InvalidChannelId;
  TWdayUnion FWday{};

 public:
  void       Init( uint16_t startMinuteOfDay, uint16_t periodMinutes, uint16_t onMinutes,
                   TChannelId channel, uint8_t wdayMask );
  size_t     EventCount(void) const;
  void       AddSwitchTimes( TEvents & events ) const;
 };

void TSwitchIntervalDef::Init( uint16_t startMinuteOfDay, uint16_t periodMinutes, uint16_t onMinutes,
                               TChannelId channel, uint8_t wdayMask )
 {
  FStartMinuteOfDay = startMinuteOfDay;
  FPeriodMinutes = periodMinutes;
  FOnMinutes = onMinutes;
  FChannel = channel;
  FWday.mask = wdayMask;
 }

size_t TSwitchIntervalDef::EventCount(void) const
 {
  if( !FWday.bits.Active || FPeriodMinutes == 0 || FStartMinuteOfDay >= FMinutesPerDay )
    return 0;

  size_t days = 0;
  uint8_t wdayMask = FWday.mask & 0x7F;
  while( wdayMask )
   {
    days += wdayMask & 1;
    wdayMask >>= 1;
   }
  const size_t cycles = ( FMinutesPerDay - 1 - FStartMinuteOfDay ) / FPeriodMinutes + 1;
  return days * cycles * 2;
 }

void TSwitchIntervalDef::AddSwitchTimes( TEvents & events ) const
 {
  if( !FWday.bits.Active || FPeriodMinutes == 0 || FStartMinuteOfDay >= FMinutesPerDay )
    return;

  for( uint8_t wday = 0;  wday < 7;  ++wday )
    if( FWday.mask & ( 1u << wday ) )
      for( uint16_t start = FStartMinuteOfDay;  start < FMinutesPerDay;  start += FPeriodMinutes )
       {
        const uint16_t on = GnMinuteOfWeek( start, wday );
        const uint16_t off = static_cast<uint16_t>( ( on + FOnMinutes ) % FMinutesPerWeek );
        events.emplace_back( on, FChannel, true, false );
        events.emplace_back( off, FChannel, false, false );
       }
 }

//****************************************************************
// TSchedulerChannelStatus
//****************************************************************

void TSchedulerChannelStatus::ResetCalculatedValues(void)
 {
  FOverrideUntilKnown = false;
  FOverrideUntil = 0;
  FNextChangeValid = false;
  FNextChangeTime = 0;
  FNextChangeState = false;
 }

//****************************************************************
// TScheduler
//****************************************************************

TScheduler::TScheduler( TIHardware & hardware, const TChannelConfigs & channels,
                       const THeaterProfiles & heaterProfiles )
: FHardware( hardware )
, FChannels( channels )
, FHeaterControl( heaterProfiles )
, FChannelStati( channels.size() )
 {
 }

TScheduler::~TScheduler(void) = default;

void TScheduler::SetHeaterProfiles( const THeaterProfiles & heaterProfiles )
 {
  FHeaterControl.Profiles( heaterProfiles );
 }

bool TScheduler::AddBytes( size_t bytes, size_t & requiredBytes, size_t & largestBlockBytes )
 {
  if( bytes > SIZE_MAX - requiredBytes )
    return false;
  requiredBytes += bytes;
  largestBlockBytes = std::max( largestBlockBytes, bytes );
  return true;
 }

bool TScheduler::CalcEventCount( const TSwitchTimes & switchTimes, const TSwitchIntervals & intervals,
                                 size_t & eventCount )
 {
  eventCount = 0;
  const auto addEvents = [&eventCount]( size_t count )
   {
    if( count > SIZE_MAX - eventCount )
      return false;
    eventCount += count;
    return true;
   };

  for( const TSwitchTime & entry : switchTimes )
   {
    if( entry.MinuteOfDay >= FMinutesPerDay )
      continue;
    TSwitchDef record;
    record.Init( entry.MinuteOfDay, entry.Channel, entry.On != 0, entry.wday.mask );
    if( !addEvents( record.EventCount() ) )
      return false;
   }

  for( const TSwitchInterval & entry : intervals )
   {
    if( entry.StartMinuteOfDay >= FMinutesPerDay || entry.PeriodMinutes == 0 ||
        entry.OnMinutes == 0 || entry.OnMinutes >= entry.PeriodMinutes )
      continue;
    TSwitchIntervalDef record;
    record.Init( entry.StartMinuteOfDay, entry.PeriodMinutes, entry.OnMinutes,
                 entry.Channel, entry.wday.mask );
    if( !addEvents( record.EventCount() ) )
      return false;
   }
  return eventCount <= UINT_MAX;
 }

bool TScheduler::GetHeapRequirement( const TSwitchTimes & switchTimes, const TSwitchIntervals & intervals,
                                     size_t channelCount, size_t & requiredBytes, size_t & largestBlockBytes )
 {
  size_t eventCount = 0;
  if( !CalcEventCount( switchTimes, intervals, eventCount ) || channelCount > SIZE_MAX - eventCount )
    return false;
  const size_t eventCapacity = eventCount + channelCount;
  if( eventCapacity > SIZE_MAX / sizeof( TEvent ) ||
      channelCount > SIZE_MAX / sizeof( TSchedulerChannelStatus ) )
    return false;
  requiredBytes = 0;
  largestBlockBytes = 0;
  return AddBytes( eventCapacity * sizeof( TEvent ), requiredBytes, largestBlockBytes ) &&
         AddBytes( channelCount * sizeof( TSchedulerChannelStatus ), requiredBytes, largestBlockBytes );
 }

void TScheduler::BuildEvents( const TSwitchTimes & switchTimes, const TSwitchIntervals & intervals,
                              size_t oneTimeCapacity, TEvents & events )
 {
  size_t eventCount = 0;
  CalcEventCount( switchTimes, intervals, eventCount );
  events.clear();
  events.reserve( eventCount + oneTimeCapacity );

  for( const TSwitchTime & entry : switchTimes )
   {
    if( entry.MinuteOfDay >= FMinutesPerDay )
      continue;
    TSwitchDef record;
    record.Init( entry.MinuteOfDay, entry.Channel, entry.On != 0, entry.wday.mask );
    record.AddSwitchTimes( events );
   }

  for( const TSwitchInterval & entry : intervals )
   {
    if( entry.StartMinuteOfDay >= FMinutesPerDay || entry.PeriodMinutes == 0 ||
        entry.OnMinutes == 0 || entry.OnMinutes >= entry.PeriodMinutes )
      continue;
    TSwitchIntervalDef record;
    record.Init( entry.StartMinuteOfDay, entry.PeriodMinutes, entry.OnMinutes,
                 entry.Channel, entry.wday.mask );
    record.AddSwitchTimes( events );
   }

  std::sort( events.begin(), events.end(), []( const TEvent & left, const TEvent & right )
   {
    return left.MinuteOfWeek() < right.MinuteOfWeek();
   } );
 }

void TScheduler::SetSchedule( const TSwitchTimes & switchTimes, const TSwitchIntervals & intervals )
 {
  TEvents events;
  BuildEvents( switchTimes, intervals, FChannels.size(), events );
  std::lock_guard<std::mutex> lock( FStateMutex );
  FEvents.swap( events );
 }

void TScheduler::CalculateStates( uint16_t minuteOfWeek )
 {
  for( TSchedulerChannelStatus & status : FChannelStati )
   {
    status.FTimeState = false;
    status.FTimeSchedulePresent = false;
    status.FAutomaticState = false;
    status.FState = false;
    status.FOverrideActive = false;
    status.FOverrideState = false;
   }

  for( const TEvent & event : FEvents )
    if( !event.OneTime() && event.Channel() < FChannelStati.size() )
     {
      TSchedulerChannelStatus & status = FChannelStati[ event.Channel() ];
      status.FTimeSchedulePresent = true;
      status.FTimeState = event.On();
     }

  for( const TEvent & event : FEvents )
   {
    if( event.MinuteOfWeek() > minuteOfWeek )
      break;
    if( !event.OneTime() && event.Channel() < FChannelStati.size() )
      FChannelStati[ event.Channel() ].FTimeState = event.On();
   }

  for( size_t channelNr = 0; channelNr < FChannelStati.size(); ++channelNr )
   {
    TSchedulerChannelStatus & status = FChannelStati[ channelNr ];
    bool hysteresisStateKnown = status.FHysteresisStateKnown != 0;
    bool hysteresisState = status.FHysteresisState != 0;
    const TChannelConfig & channel = FChannels[ channelNr ];
    const bool automaticState = FHeaterControl.AutomaticState( channel.Role, channel.Profile,
                                                                status.FTimeSchedulePresent != 0,
                                                                status.FTimeState != 0,
                                                                hysteresisStateKnown, hysteresisState );
    status.FHysteresisStateKnown = hysteresisStateKnown;
    status.FHysteresisState = hysteresisState;
    status.FAutomaticState = automaticState;
    status.FState = automaticState;
   }
 }

bool TScheduler::UpdateAutomaticState( TChannelId channel, bool writeLog, TGNString & error )
 {
  if( channel >= FChannelStati.size() )
   {
    error = "Invalid output channel";
    return false;
   }

  TSchedulerChannelStatus & status = FChannelStati[ channel ];
  bool hysteresisStateKnown = status.FHysteresisStateKnown != 0;
  bool hysteresisState = status.FHysteresisState != 0;
  const TChannelConfig & channelConfig = FChannels[ channel ];
  const bool automaticState = FHeaterControl.AutomaticState( channelConfig.Role, channelConfig.Profile,
                                                              status.FTimeSchedulePresent != 0,
                                                              status.FTimeState != 0,
                                                              hysteresisStateKnown, hysteresisState );
  status.FHysteresisStateKnown = hysteresisStateKnown;
  status.FHysteresisState = hysteresisState;
  status.FAutomaticState = automaticState;
  if( !status.FOverrideActive && FChannels[ channel ].HasOutput() && status.FState != automaticState )
    return SetOutput( channel, automaticState, writeLog, error );
  error.clear();
  return true;
 }

void TScheduler::UpdateHeaters( bool writeLog )
 {
  for( size_t channelNr = 0; channelNr < FChannels.size(); ++channelNr )
   {
    if( FChannels[ channelNr ].Role == TChannelRole::Timer )
      continue;
    TGNString error;
    if( !UpdateAutomaticState( static_cast<TChannelId>( channelNr ), writeLog, error ) )
      Logger.Write( TLogLevel::Error, error );
   }
 }

bool TScheduler::SetOutput( TChannelId channel, bool on, bool writeLog, TGNString & error )
 {
  if( channel >= FChannels.size() )
   {
    error = "Invalid output channel";
    return false;
   }
  if( !FHardware.SetPin( channel, on, error ) )
   {
    if( writeLog )
      Logger.Write( TLogLevel::Error, error );
    return false;
   }
  FChannelStati[ channel ].FState = on;
  if( writeLog )
    Logger.Write( TLogLevel::Info, FChannels[ channel ].Id + ( on ? " switched on" : " switched off" ) );
  return true;
 }

bool TScheduler::SetAllOutputs( bool firstTime, TGNString & error )
 {
  for( size_t channelNr = 0;  channelNr < FChannels.size();  ++channelNr )
    if( FChannels[ channelNr ].HasOutput() &&
        !FHardware.SetPin( static_cast<TChannelId>( channelNr ), FChannelStati[ channelNr ].FState != 0,
                           error, firstTime ) )
      return false;
  error.clear();
  return true;
 }

void TScheduler::RemoveOneTimeEvent( TChannelId channel )
 {
  std::erase_if( FEvents, [channel]( const TEvent & event )
   {
    return event.OneTime() && event.Channel() == channel;
   } );
 }

const TEvent * TScheduler::FindOneTimeEvent( TChannelId channel ) const
 {
  const auto event = std::find_if( FEvents.begin(), FEvents.end(), [channel]( const TEvent & item )
   {
    return item.OneTime() && item.Channel() == channel;
   } );
  return event == FEvents.end() ? nullptr : &( *event );
 }

void TScheduler::InsertOneTimeEvent( const TEvent & event )
 {
  const auto position = std::upper_bound( FEvents.begin(), FEvents.end(), event.MinuteOfWeek(),
                                          []( uint16_t minute, const TEvent & item )
   {
    return minute < item.MinuteOfWeek();
   } );
  FEvents.insert( position, event );
 }

void TScheduler::ApplyEvents( uint16_t minuteOfWeek, bool writeLog )
 {
  const auto first = std::lower_bound( FEvents.begin(), FEvents.end(), minuteOfWeek,
                                       []( const TEvent & event, uint16_t minute )
   {
    return event.MinuteOfWeek() < minute;
   } );
  const auto end = std::upper_bound( first, FEvents.end(), minuteOfWeek,
                                     []( uint16_t minute, const TEvent & event )
   {
    return minute < event.MinuteOfWeek();
   } );

  for( auto event = first; event != end; ++event )
   {
    if( event->Channel() >= FChannelStati.size() )
      continue;
    TSchedulerChannelStatus & status = FChannelStati[ event->Channel() ];
    if( !event->OneTime() )
     {
      status.FTimeSchedulePresent = true;
      status.FTimeState = event->On();
     }
    status.FOverrideActive = false;
    status.FOverrideState = false;
   }

  for( auto event = first; event != end; ++event )
   {
    const auto later = std::find_if( event + 1, end, [event]( const TEvent & item )
     {
      return item.Channel() == event->Channel();
     } );
    if( later == end && event->Channel() < FChannelStati.size() )
     {
      TGNString error;
      UpdateAutomaticState( event->Channel(), writeLog, error );
     }
   }

  std::erase_if( FEvents, [minuteOfWeek]( const TEvent & event )
   {
    return event.OneTime() && event.MinuteOfWeek() == minuteOfWeek;
   } );
 }

bool TScheduler::ValidateOutputChannel( TChannelId channel, TGNString & error ) const
 {
  if( channel >= FChannels.size() || !FChannels[ channel ].HasOutput() )
   {
    error = "Invalid output channel";
    return false;
   }
  error.clear();
  return true;
 }

bool TScheduler::ToggleChannel( TChannelId channel, TGNString & error )
 {
  std::lock_guard<std::mutex> lock( FStateMutex );
  if( !ValidateOutputChannel( channel, error ) )
    return false;
  const bool current = FChannelStati[ channel ].FState != 0;
  const bool newState = !current;
  if( !SetOutput( channel, newState, false, error ) )
    return false;
  RemoveOneTimeEvent( channel );
  FChannelStati[ channel ].FOverrideActive = newState != FChannelStati[ channel ].FAutomaticState;
  FChannelStati[ channel ].FOverrideState = newState;
  Logger.Write( TLogLevel::Info, FChannels[ channel ].Id +
                        ( newState ? " switched on manually" : " switched off manually" ) );
  error.clear();
  return true;
 }

bool TScheduler::ToggleChannel( TChannelId channel, std::chrono::minutes duration, TGNString & error )
 {
  std::lock_guard<std::mutex> lock( FStateMutex );
  if( !ValidateOutputChannel( channel, error ) )
    return false;
  if( duration <= std::chrono::minutes::zero() )
   {
    error = "Temporary toggle duration must be greater than zero";
    return false;
   }
  const bool current = FChannelStati[ channel ].FState != 0;
  if( !SetOutput( channel, !current, false, error ) )
    return false;

  RemoveOneTimeEvent( channel );
  FChannelStati[ channel ].FOverrideActive = true;
  FChannelStati[ channel ].FOverrideState = !current;
  const std::time_t currentTime = std::time( nullptr );
  const std::time_t fireTime = currentTime +
    std::chrono::duration_cast<std::chrono::seconds>( duration ).count();
  TEvent nextEvent;
  std::time_t nextTime = 0;
  if( !FindNextEvent( channel, currentTime, current, false, false, nextEvent, nextTime ) || nextTime > fireTime )
   {
    InsertOneTimeEvent( TEvent( GnLocalMinuteOfWeek( fireTime ), channel, current, true ) );
    Logger.Write( TLogLevel::Info, FChannels[ channel ].Id +
                          StringFormat( " toggled for %lld minutes",
                                        static_cast<long long>( duration.count() ) ) );
   }
  else
    Logger.Write( TLogLevel::Info, FChannels[ channel ].Id + " toggled until next schedule event" );
  error.clear();
  return true;
 }

bool TScheduler::FindNextEvent( TChannelId channel, std::time_t currentTime, bool initialState,
                                bool onlyStateChanges, bool includeOneTime, TEvent & nextEvent,
                                std::time_t & nextTime ) const
 {
  if( FEvents.empty() )
    return false;
  const uint16_t currentMinute = GnLocalMinuteOfWeek( currentTime );
  const auto first = std::upper_bound( FEvents.begin(), FEvents.end(), currentMinute,
                                       []( uint16_t minute, const TEvent & event )
   {
    return minute < event.MinuteOfWeek();
   } );
  const size_t firstIndex = static_cast<size_t>( std::distance( FEvents.begin(), first ) );
  bool state = initialState;

  for( unsigned pass = 0;  pass < 2;  ++pass )
   {
    size_t eventNr = pass == 0 ? firstIndex : 0;
    const size_t passEnd = pass == 0 ? FEvents.size() : firstIndex;
    while( eventNr < passEnd )
     {
      const uint16_t minute = FEvents[ eventNr ].MinuteOfWeek();
      bool found = false;
      TEvent finalEvent;
      while( eventNr < passEnd && FEvents[ eventNr ].MinuteOfWeek() == minute )
       {
        if( FEvents[ eventNr ].Channel() == channel &&
            ( includeOneTime || !FEvents[ eventNr ].OneTime() ) )
         {
          finalEvent = FEvents[ eventNr ];
          found = true;
         }
        ++eventNr;
       }
      if( !found )
        continue;
      const bool newState = finalEvent.On();
      if( !onlyStateChanges || newState != state )
       {
        nextEvent = finalEvent;
        nextTime = GnNextLocalMinuteOfWeek( currentTime, minute );
        return nextTime != static_cast<std::time_t>( -1 );
       }
      state = newState;
     }
   }
  return false;
 }

void TScheduler::ResetFunctionState( TChannelId Channel )
 {
  std::lock_guard<std::mutex> Lock( FStateMutex );
  if( Channel < FChannelStati.size() ) FChannelStati[Channel] = TSchedulerChannelStatus{};
 }

bool TScheduler::ReconstructState( TGNString & error, bool firstTime )
 {
  std::lock_guard<std::mutex> lock( FStateMutex );
  const uint16_t minute = GnLocalMinuteOfWeek( std::time( nullptr ) );
  std::erase_if( FEvents, []( const TEvent & event )
   {
    return event.OneTime();
   } );
  CalculateStates( minute );
  if( !SetAllOutputs( firstTime, error ) )
    return false;
  FLastMinuteOfWeek = minute;
  Logger.Write( TLogLevel::Info, "Scheduler event list reconstructed and applied to outputs" );
  return true;
 }

TSchedulerChannelStati TScheduler::ChannelStati(void) const
 {
  std::lock_guard<std::mutex> lock( FStateMutex );
  TSchedulerChannelStati stati = FChannelStati;
  const std::time_t currentTime = std::time( nullptr );
  for( size_t channelNr = 0; channelNr < FChannels.size(); ++channelNr )
   {
    const TChannelId channel = static_cast<TChannelId>( channelNr );
    TSchedulerChannelStatus & status = stati[ channelNr ];
    status.ResetCalculatedValues();
    if( !FChannels[ channelNr ].HasOutput() )
      continue;

    const TEvent * oneTimeEvent = status.FOverrideActive ? FindOneTimeEvent( channel ) : nullptr;
    if( oneTimeEvent != nullptr )
     {
      status.FOverrideUntil = GnNextLocalMinuteOfWeek( currentTime, oneTimeEvent->MinuteOfWeek() );
      status.FOverrideUntilKnown = status.FOverrideUntil != static_cast<std::time_t>( -1 );
     }

    if( FChannels[ channelNr ].Role != TChannelRole::Timer )
      continue;

    TEvent nextEvent;
    std::time_t nextChangeTime = 0;
    if( FindNextEvent( channel, currentTime, status.FState != 0, true, true, nextEvent, nextChangeTime ) )
     {
      status.FNextChangeValid = true;
      status.FNextChangeTime = nextChangeTime;
      status.FNextChangeState = nextEvent.On();
     }
   }
  return stati;
 }

void TScheduler::Loop(void)
 {
  const uint16_t minuteOfWeek = GnLocalMinuteOfWeek( std::time( nullptr ) );
  std::unique_lock<std::mutex> lock( FStateMutex );
  if( minuteOfWeek != FLastMinuteOfWeek )
   {
    if( minuteOfWeek == ( FLastMinuteOfWeek + 1 ) % FMinutesPerWeek )
     {
      FLastMinuteOfWeek = minuteOfWeek;
      ApplyEvents( minuteOfWeek, true );
     }
    else
     {
      lock.unlock();
      TGNString error;
      if( !ReconstructState( error ) )
        Logger.Write( TLogLevel::Error, error );
      return;
     }
   }

  if( FHeaterCheckTimer.Elapsed() < 250 )
    return;
  FHeaterCheckTimer.Start();
  UpdateHeaters( true );
 }
