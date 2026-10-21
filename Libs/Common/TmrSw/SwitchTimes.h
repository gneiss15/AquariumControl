// SwitchTimes.h, Version: 1.14
//****************************************************************

#ifndef SwitchTimesH  // sentry
#define SwitchTimesH

//****************************************************************
// Includes
//****************************************************************

#include <GnBasics.h>
#include <GnHeaterControl.h>
#include "TmrSwIncludes.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <vector>

using namespace std;
using namespace chrono;

//****************************************************************
// typedefs
//****************************************************************

typedef struct
 {
  uint8_t             So : 1;
  uint8_t             Mo : 1;
  uint8_t             Di : 1;
  uint8_t             Mi : 1;
  uint8_t             Do : 1;
  uint8_t             Fr : 1;
  uint8_t             Sa : 1;
  uint8_t             Active : 1;
 }
 TWdayMask;

typedef union
 {
  uint8_t             mask;
  TWdayMask           bits;
 }
 TWdayUnion;

typedef struct
 {
  uint16_t            MinuteOfDay : 15;
  uint16_t            On : 1;
  TWdayUnion          wday;
 }
 TSwitchRecord;

//****************************************************************
// TEvent
//****************************************************************

class TEvent
 {
 private:
  uint16_t            FMinuteOfWeek : 15;
  uint16_t            FOn : 1;

 public:
                      TEvent(void)                                : FMinuteOfWeek( 0 ), FOn( 0 ) {}
                      TEvent( uint16_t minuteOfWeek, bool on )    : FMinuteOfWeek( minuteOfWeek ), FOn( on ) {}
  uint16_t            MinuteOfWeek(void) const                    { return FMinuteOfWeek; }
  void                MinuteOfWeek( uint16_t minuteOfWeek )       { FMinuteOfWeek = minuteOfWeek; }
  bool                On(void) const                              { return FOn; }
  void                On( bool on )                               { FOn = on; }
 };

typedef vector<TEvent> TEvents;

//****************************************************************
// TSwitchDef
//****************************************************************

class TSwitchDef
 {
 public:
  TSwitchRecord       FSwitchRecord;

 public:
                      TSwitchDef(void)                            {}
  void                Init( uint16_t minuteOfDay, uint16_t on, uint8_t wdayMask );
  uint16_t            MinuteOfDay(void) const                     { return FSwitchRecord.MinuteOfDay; }
  bool                On(void) const                              { return FSwitchRecord.On; }
  bool                Active(void) const                          { return FSwitchRecord.wday.bits.Active; }
  uint8_t             WdayMask(void) const                        { return FSwitchRecord.wday.mask; }
  void                AddSwitchTimes( TEvents & events ) const;
 };

typedef vector<TSwitchDef> TSwitchDefs;

struct TSwitchInterval
 {
  uint16_t StartMinuteOfDay = 0;
  uint16_t PeriodMinutes = 0;
  uint16_t OnMinutes = 0;
  uint8_t  WdayMask = 0;
 };
typedef vector<TSwitchInterval> TSwitchIntervals;

//****************************************************************
// TSwitchTimes
//****************************************************************

class TSwitchTimes : public TSingleton<TSwitchTimes>
 {
  friend class TSingleton<TSwitchTimes>;
 private:
                      TSwitchTimes(void);

 private:
  static char const   FErrorInvalidData[];
  static char const   FErrorDuplicateActiveTime[];
  static char const   FErrorNotEnoughSpace[];
  static char const   FErrorWriteFailed[];

  TSwitchDefs         FSwitchRecords;
  TSwitchIntervals    FIntervalRecords;
  TEvents             FEvents;
  TGnHeaterControl     FHeaterControl;
  uint16_t             FNextMow = 0;
  bool                 FSchedulePresent = false;
  bool                 FScheduleState = false;
  bool                 FAutomaticState = false;
  bool                 FHysteresisStateKnown = false;
  bool                 FHysteresisState = false;
  bool                 FOverrideActive = false;
  bool                 FOverrideState = false;
  bool                 FCalibrationOverrideActive = false;
  bool                 FCalibrationOverrideState = false;

 private:
  static bool         IsDigit( char value );
  static bool         ParseTime( String const & value, uint16_t & minuteOfDay );
  static bool         ParseDuration( String const & value, uint16_t & minutes );
  static size_t       CalcEventCount( TSwitchDefs const & records, TSwitchIntervals const & intervals );
  static char const * Parse( String const & jsonStr, TSwitchDefs & records );
  static char const * ParseIntervals( String const & jsonStr, TSwitchIntervals & intervals, char const * ExpectedChannel = nullptr );
  static bool         HasActiveTimeConflict( TSwitchDefs const & records );
  static bool         HasActiveTimeConflict( TSwitchIntervals const & intervals );
  void                Clear(void);
  static void         RecoverFiles( char const * path );
  static bool         CommitTemporary( char const * path );
  char const *        SaveTemporary( char const * path, String const & jsonStr );
  char const *        Load( String const & jsonStr );
  char const *        LoadFile( char const * path );
  char const *        LoadIntervals( String const & jsonStr );
  char const *        LoadIntervalsFile( char const * path );
  static char const * BuildEvents( TSwitchDefs const & records, TSwitchIntervals const & intervals,
                                   TEvents & events );
  void                CalculateScheduleState( uint16_t minuteOfWeek );
  void                ApplyAutomaticState(void);
  void                ApplyOutputState( bool on );

 public:
  void                DoEvents( struct tm const & timeParts, bool useNextMow = false );
  void                UpdateHeater(void);
  bool                ManualToggle(void);
  bool                SetCalibrationOverride( bool Active, bool State );
  bool                GetStatus( struct tm const & timeParts, time_t currentTime,
                                 bool & automaticState, time_t & nextChangeTime,
                                 bool & nextChangeState ) const;
  bool                AutomaticState(void) const                  { return FAutomaticState; }
  bool                OverrideActive(void) const                  { return FOverrideActive; }
  bool                OverrideState(void) const                   { return FOverrideState; }
  bool                CalibrationOverrideActive(void) const        { return FCalibrationOverrideActive; }
  bool                CalibrationOverrideState(void) const         { return FCalibrationOverrideState; }
  static String       ValidateBundle( String const & Schedule, String const & Intervals, char const * ChannelId );
  String              LoadBundle( String const & Schedule, String const & Intervals );
  String              Set( String const & jsonStr );
  String              SetIntervals( String const & jsonStr );
  bool                Setup(void);
  void                RefreshProfiles(void);
  void                ResetFunctionState(void);
 };
#define SwitchTimes   TSwitchTimes::Instance()

//****************************************************************
#endif // sentry
