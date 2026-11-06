// SwitchTimes.cpp, Version: 2.61
//****************************************************************
// Includes
//****************************************************************

#define Do_TJsonHeatConfig
#define Do_TJsonInterval
#define Do_TJsonSwitchTime
#define Do_TUdpHeatControl
#define Do_TUdpHeatDiagnostics
#include <GnBasics.h>
#include <AddressResolver.h>
#include <GnJsonSchema.h>
#include "SwitchTimes.h"

#include "TmrSwChannels.h"
#include "TmrSwHw.h"
#include "TmrSwProject.h"
#ifndef TMR_SW_HAS_POWER_METER
#error TMR_SW_HAS_POWER_METER must be provided by TmrSwProject.h
#endif
#include "TmrSwWifi.h"
#include <GnTimeSync.h>
#include <GnUdpStatus.h>
#include <GnDeviceIdentity.h>

namespace
 {
String FormatSwitchTimesJson( String const & jsonStr )
 {
  JsonDocument doc;
  if( deserializeJson( doc, jsonStr ) != DeserializationError::Ok || !doc.is<JsonArray>() )
    return jsonStr;

  const JsonArrayConst rows = doc.as<JsonArrayConst>();
  if( rows.size() == 0 )
    return "[]\n";

  size_t widths[ 4 ] = { 0, 0, 0, 0 };
  for( JsonVariantConst value : rows )
   {
    const JsonArrayConst row = value.as<JsonArrayConst>();
    if( row.isNull() || row.size() < 3 || row.size() > 4 )
      return jsonStr;
    for( size_t column = 0;  column < row.size();  ++column )
     {
      const size_t cellLength = measureJson( row[ column ] );
      if( cellLength > widths[ column ] )
        widths[ column ] = cellLength;
     }
   }

  TGNString result;
  const auto build = [&]( auto & output )
   {
    output += "[\n";
    size_t rowIndex = 0;
    for( JsonVariantConst value : rows )
     {
      const JsonArrayConst row = value.as<JsonArrayConst>();
      output += "  [ ";
      for( size_t column = 0;  column < row.size();  ++column )
       {
        const JsonVariantConst cell = row[ column ];
        const size_t cellLength = measureJson( cell );
        GnAppendGenerated( output, cellLength, [&]( TGNString & target )
         {
          TGnStringAppendWriter Writer( target );
          serializeJson( cell, Writer );
         } );
        if( column + 1 < row.size() )
         {
          output += ", ";
          output.append( widths[ column ] - cellLength, ' ' );
         }
       }
      output += " ]";
      if( ++rowIndex < rows.size() )
        output += ',';
      output += '\n';
     }
    output += "]\n";
   };

  if( !GnBuildStringExact( result, build ) )
    return jsonStr;
  return result;
 }

} // namespace

//****************************************************************
// TSwitchTimes constants
//****************************************************************

char const TSwitchTimes::FErrorInvalidData[] = "InvalidData";
char const TSwitchTimes::FErrorDuplicateActiveTime[] = "DuplicateActiveTime";
char const TSwitchTimes::FErrorNotEnoughSpace[] = "NotEnoughSpace";
char const TSwitchTimes::FErrorWriteFailed[] = "WriteFailed";

//****************************************************************
// TSwitchDef
//****************************************************************

void TSwitchDef::Init( uint16_t minuteOfDay, uint16_t on, uint8_t wdayMask )
 {
  FSwitchRecord.MinuteOfDay = minuteOfDay;
  FSwitchRecord.On = on;
  FSwitchRecord.wday.mask = wdayMask;
 }

void TSwitchDef::AddSwitchTimes( TEvents & events ) const
 {
  if( !Active() )
    return;
  for( uint8_t wday = 0; wday < 7; ++wday )
    if( WdayMask() & ( 1 << wday ) )
      events.push_back( TEvent( GnMinuteOfWeek( MinuteOfDay(), wday ), On() ) );
 }

//****************************************************************
// TSwitchTimes
//****************************************************************

TSwitchTimes::TSwitchTimes(void)
 {
 }

bool TSwitchTimes::Setup(void)
 {
  RecoverFiles( "/SwitchTimes.json" );
  RecoverFiles( "/Intervals.json" );
  if( LoadFile( "/SwitchTimes.json" ) != nullptr )
    return false;
  if( LittleFS.exists( "/Intervals.json" ) && LoadIntervalsFile( "/Intervals.json" ) != nullptr )
    return false;
  FHeaterControl.Profiles( TmrSwChannels.Profiles() );
  return true;
 }

void TSwitchTimes::ResetFunctionState(void)
 {
  FChargeRunOn.Reset();
  FHeatPi.CancelLearning();
  FOverrideActive = FOverrideState = false;
  FCalibrationOverrideActive = FCalibrationOverrideState = false;
  FScheduleState = FAutomaticState = FSchedulePresent = false;
  FHysteresisStateKnown = false;
  ApplyOutputState( false );
  UpdateHeater();
 }

void TSwitchTimes::RefreshProfiles(void)
 {
  FHeaterControl.Profiles( TmrSwChannels.Profiles() );
 }



bool TSwitchTimes::ParseTime( String const & Value, uint16_t & Minutes )
 {
  if(!GnTimeOfDay(Value.c_str()))return false;
  Minutes=uint16_t(((Value[0]-'0')*10+Value[1]-'0')*60+(Value[3]-'0')*10+Value[4]-'0');
  return true;
 }

bool TSwitchTimes::ParseDuration( String const & Value, uint16_t & Minutes )
 {
  if(!GnDuration(Value.c_str()))return false;
  Minutes=uint16_t(((Value[0]-'0')*10+Value[1]-'0')*60+(Value[3]-'0')*10+Value[4]-'0');
  return true;
 }

size_t TSwitchTimes::CalcEventCount( TSwitchDefs const & records, TSwitchIntervals const & intervals )
 {
  size_t count = 0;
  for( TSwitchDef const & record : records )
   {
    if( !record.Active() )
      continue;
    uint8_t days = record.WdayMask() & 0x7F;
    while( days )
     {
      count += days & 1;
      days >>= 1;
     }
   }
  for( TSwitchInterval const & interval : intervals )
   {
    if( !( interval.WdayMask & 0x80 ) || interval.PeriodMinutes == 0 || interval.StartMinuteOfDay >= 24 * 60 ||
        interval.OnMinutes == 0 || interval.OnMinutes >= interval.PeriodMinutes )
      continue;
    uint8_t daysMask = interval.WdayMask & 0x7F;
    size_t days = 0;
    while( daysMask )
     {
      days += daysMask & 1;
      daysMask >>= 1;
     }
    const size_t cycles = ( 24 * 60 - 1 - interval.StartMinuteOfDay ) / interval.PeriodMinutes + 1;
    count += days * cycles * 2;
   }
  return count;
 }

bool TSwitchTimes::HasActiveTimeConflict( TSwitchDefs const & records )
 {
  for( size_t first = 0; first < records.size(); ++first )
   {
    TSwitchDef const & firstRecord = records[ first ];
    if( !firstRecord.Active() )
      continue;

    const uint8_t firstDays = firstRecord.WdayMask() & 0x7F;
    for( size_t second = first + 1; second < records.size(); ++second )
     {
      TSwitchDef const & secondRecord = records[ second ];
      if( !secondRecord.Active() )
        continue;
      if( firstRecord.MinuteOfDay() == secondRecord.MinuteOfDay()
       && ( firstDays & ( secondRecord.WdayMask() & 0x7F ) ) )
        return true;
     }
   }
  return false;
 }

bool TSwitchTimes::HasActiveTimeConflict( TSwitchIntervals const & intervals )
 {
  for( size_t first = 0; first < intervals.size(); ++first )
   {
    TSwitchInterval const & firstInterval = intervals[ first ];
    if( !( firstInterval.WdayMask & 0x80 ) )
      continue;
    for( size_t second = first + 1; second < intervals.size(); ++second )
     {
      TSwitchInterval const & secondInterval = intervals[ second ];
      if( ( secondInterval.WdayMask & 0x80 ) &&
          firstInterval.StartMinuteOfDay == secondInterval.StartMinuteOfDay &&
          ( firstInterval.WdayMask & secondInterval.WdayMask & 0x7F ) )
        return true;
     }
   }
  return false;
 }

char const * TSwitchTimes::Parse( String const & jsonStr, TSwitchDefs & records )
 {
  if( !GnHasHeapSpace( jsonStr.length() + 512 ) )
    return FErrorNotEnoughSpace;

  JsonDocument doc;
  const DeserializationError error = deserializeJson( doc, jsonStr );
  if( error == DeserializationError::NoMemory )
    return FErrorNotEnoughSpace;
  if( error != DeserializationError::Ok || !doc.is<JsonArray>() )
    return FErrorInvalidData;

  TSwitchDefs newRecords;
  JsonArray array = doc.as<JsonArray>();
  if( array.size() > SIZE_MAX / sizeof( TSwitchDef ) )
    return FErrorNotEnoughSpace;
  const size_t recordBytes = array.size() * sizeof( TSwitchDef );
  if( !GnHasHeapSpace( recordBytes, recordBytes ) )
    return FErrorNotEnoughSpace;
  newRecords.reserve( array.size() );

  for( JsonVariant value : array )
   {
    TJsonSwitchTime row;
    TGNString error;
    if(!GnRead(TGnWireValue(value),row,error) || !row.Time.Known() || !row.State.Known() || !row.Days.Known())return FErrorInvalidData;
    if(row.Ch.Known())
     {
      TGNString Address;
      if(!ResolveChannelAddress(row.Ch.Value,{{GnDeviceIdentity.WireId()+"/0",1}},1,Address))return FErrorInvalidData;
     }
    const uint16_t minuteOfDay=uint16_t(((row.Time[0]-'0')*10+row.Time[1]-'0')*60+(row.Time[3]-'0')*10+row.Time[4]-'0');
    TSwitchDef record;
    record.Init(minuteOfDay,row.State,row.Days);
    newRecords.push_back( record );
   }

  if( HasActiveTimeConflict( newRecords ) )
    return FErrorDuplicateActiveTime;

  records.swap( newRecords );
  return nullptr;
 }

char const * TSwitchTimes::ParseIntervals( String const & jsonStr, TSwitchIntervals & intervals, char const * ExpectedChannel )
 {
  if( !GnHasHeapSpace( jsonStr.length() + 512 ) )
    return FErrorNotEnoughSpace;

  JsonDocument doc;
  const DeserializationError error = deserializeJson( doc, jsonStr );
  if( error == DeserializationError::NoMemory )
    return FErrorNotEnoughSpace;
  if( error != DeserializationError::Ok || !doc.is<JsonArray>() )
    return FErrorInvalidData;

  JsonArray array = doc.as<JsonArray>();
  if( array.size() > SIZE_MAX / sizeof( TSwitchInterval ) )
    return FErrorNotEnoughSpace;
  const size_t recordBytes = array.size() * sizeof( TSwitchInterval );
  if( !GnHasHeapSpace( recordBytes, recordBytes ) )
    return FErrorNotEnoughSpace;

  TSwitchIntervals newIntervals;
  newIntervals.reserve( array.size() );
  for( JsonVariant value : array )
   {
    TJsonInterval Row;
    TGNString Error;
    if(!GnRead(TGnWireValue(value),Row,Error) || !Row.Start.Known() || !Row.Period.Known() || !Row.OnDuration.Known() || !Row.Days.Known() || !Row.Ch.Known())return FErrorInvalidData;
    uint16_t startMinuteOfDay=0,periodMinutes=0,onMinutes=0;
    ParseTime(Row.Start.Value.c_str(),startMinuteOfDay);
    ParseDuration(Row.Period.Value.c_str(),periodMinutes);
    ParseDuration(Row.OnDuration.Value.c_str(),onMinutes);
    const int wdayMask=Row.Days;
    TGNString Address;
    if(!ResolveChannelAddress(Row.Ch.Value,{{GnDeviceIdentity.WireId()+"/0",1}},1,Address))return FErrorInvalidData;
    (void)ExpectedChannel;
    TSwitchInterval interval;
    interval.StartMinuteOfDay = startMinuteOfDay;
    interval.PeriodMinutes = periodMinutes;
    interval.OnMinutes = onMinutes;
    interval.WdayMask = static_cast<uint8_t>( wdayMask );
    newIntervals.push_back( interval );
   }

  if( HasActiveTimeConflict( newIntervals ) )
    return FErrorDuplicateActiveTime;
  intervals.swap( newIntervals );
  return nullptr;
 }

char const * TSwitchTimes::BuildEvents( TSwitchDefs const & records, TSwitchIntervals const & intervals,
                                        TEvents & events )
 {
  const size_t eventCount = CalcEventCount( records, intervals );
  if( eventCount > SIZE_MAX / sizeof( TEvent ) )
    return FErrorNotEnoughSpace;
  const size_t eventBytes = eventCount * sizeof( TEvent );
  if( !GnHasHeapSpace( eventBytes, eventBytes ) )
    return FErrorNotEnoughSpace;

  TEvents newEvents;
  newEvents.reserve( eventCount );
  for( TSwitchDef const & record : records )
    record.AddSwitchTimes( newEvents );
  for( TSwitchInterval const & interval : intervals )
   {
    if( interval.PeriodMinutes == 0 || interval.StartMinuteOfDay >= 24 * 60 ||
        interval.OnMinutes == 0 || interval.OnMinutes >= interval.PeriodMinutes ||
        !( interval.WdayMask & 0x80 ) )
      continue;
    for( uint8_t wday = 0; wday < 7; ++wday )
      if( interval.WdayMask & ( 1 << wday ) )
        for( uint16_t start = interval.StartMinuteOfDay; start < 24 * 60; start += interval.PeriodMinutes )
         {
          const uint16_t on = GnMinuteOfWeek( start, wday );
          const uint16_t off = static_cast<uint16_t>( ( on + interval.OnMinutes ) % ( 7 * 24 * 60 ) );
          newEvents.push_back( TEvent( on, true ) );
          newEvents.push_back( TEvent( off, false ) );
         }
   }
  sort( newEvents.begin(), newEvents.end(),
        []( TEvent const & first, TEvent const & second )
         {
          return first.MinuteOfWeek() < second.MinuteOfWeek();
         } );
  events.swap( newEvents );
  return nullptr;
 }

char const * TSwitchTimes::Load( String const & jsonStr )
 {
  TSwitchDefs newRecords;
  TEvents newEvents;
  const char * error = Parse( jsonStr, newRecords );
  if( !error )
    error = BuildEvents( newRecords, FIntervalRecords, newEvents );
  Debug( "TSwitchTimes::Load: %s%s%s\n", error ? "NOK" : "OK", error ? ", " : "", error ? error : "" );
  if( error )
    return error;

  FSwitchRecords.swap( newRecords );
  FEvents.swap( newEvents );
  return nullptr;
 }

char const * TSwitchTimes::LoadIntervals( String const & jsonStr )
 {
  TSwitchIntervals newIntervals;
  TEvents newEvents;
  const char * error = ParseIntervals( jsonStr, newIntervals );
  if( !error )
    error = BuildEvents( FSwitchRecords, newIntervals, newEvents );
  Debug( "TSwitchTimes::LoadIntervals: %s%s%s\n", error ? "NOK" : "OK", error ? ", " : "", error ? error : "" );
  if( error )
    return error;

  FIntervalRecords.swap( newIntervals );
  FEvents.swap( newEvents );
  return nullptr;
 }

char const * TSwitchTimes::LoadFile( char const * path )
 {
  File file = LittleFS.open( path, "r" );
  if( !file )
    return FErrorInvalidData;
  const size_t fileSize = file.size();
  file.close();

  if( fileSize && !GnHasHeapSpace( fileSize + 1, fileSize + 1 ) )
    return FErrorNotEnoughSpace;
  const String jsonStr = ReadFile( path );
  if( jsonStr.length() != fileSize )
    return FErrorNotEnoughSpace;
  return Load( jsonStr );
 }

char const * TSwitchTimes::LoadIntervalsFile( char const * path )
 {
  File file = LittleFS.open( path, "r" );
  if( !file )
    return FErrorInvalidData;
  const size_t fileSize = file.size();
  file.close();

  if( fileSize && !GnHasHeapSpace( fileSize + 1, fileSize + 1 ) )
    return FErrorNotEnoughSpace;
  const String jsonStr = ReadFile( path );
  if( jsonStr.length() != fileSize )
    return FErrorNotEnoughSpace;
  return LoadIntervals( jsonStr );
 }

String TSwitchTimes::ValidateBundle( String const & Schedule, String const & Intervals, char const * ChannelId )
 {
  TSwitchDefs Records; TSwitchIntervals IntervalRecords; TEvents Events;
  const char * Error = Parse( Schedule, Records );
  if( !Error ) Error = ParseIntervals( Intervals, IntervalRecords, ChannelId );
  if( !Error ) Error = BuildEvents( Records, IntervalRecords, Events );
  return Error ? String( Error ) : String( "1" );
 }

String TSwitchTimes::LoadBundle( String const & Schedule, String const & Intervals )
 {
  TSwitchDefs Records; TSwitchIntervals IntervalRecords; TEvents Events;
  const char * Error = Parse( Schedule, Records );
  if( !Error ) Error = ParseIntervals( Intervals, IntervalRecords );
  if( !Error ) Error = BuildEvents( Records, IntervalRecords, Events );
  if( Error ) return String( Error );
  FSwitchRecords.swap( Records ); FIntervalRecords.swap( IntervalRecords ); FEvents.swap( Events );
  RefreshProfiles(); FNextMow = 0;
  if( TmrSwWifi.SwitchingEnabled() ) DoEvents( TmrSwWifi.GetTm() );
  return String( "1" );
 }

void TSwitchTimes::Clear(void)
 {
  TSwitchDefs().swap( FSwitchRecords );
  TEvents().swap( FEvents );
  FNextMow = 0;
 }

void TSwitchTimes::RecoverFiles( char const * path )
 {
  const String temporaryPath = String( path ) + ".tmp";
  const String backupPath = String( path ) + ".bak";

  if( !LittleFS.exists( path ) && LittleFS.exists( backupPath.c_str() ) )
    LittleFS.rename( backupPath.c_str(), path );
  if( LittleFS.exists( path ) && LittleFS.exists( backupPath.c_str() ) )
    LittleFS.remove( backupPath.c_str() );
  if( LittleFS.exists( temporaryPath.c_str() ) )
    LittleFS.remove( temporaryPath.c_str() );
 }

bool TSwitchTimes::CommitTemporary( char const * path )
 {
  const String temporaryPath = String( path ) + ".tmp";
  const String backupPath = String( path ) + ".bak";

  if( LittleFS.exists( backupPath.c_str() ) && !LittleFS.remove( backupPath.c_str() ) )
    return false;

  const bool hadOriginal = LittleFS.exists( path );
  if( hadOriginal && !LittleFS.rename( path, backupPath.c_str() ) )
    return false;

  if( !LittleFS.rename( temporaryPath.c_str(), path ) )
   {
    if( hadOriginal )
      LittleFS.rename( backupPath.c_str(), path );
    return false;
   }

  if( hadOriginal && LittleFS.exists( backupPath.c_str() ) )
    LittleFS.remove( backupPath.c_str() );
  return true;
 }

char const * TSwitchTimes::SaveTemporary( char const * path, String const & jsonStr )
 {
  const String temporaryPath = String( path ) + ".tmp";
  if( LittleFS.exists( temporaryPath.c_str() ) && !LittleFS.remove( temporaryPath.c_str() ) )
    return FErrorWriteFailed;

  TFSInfo info;
  if( !GetLittleFSInfo( info ) )
    return FErrorWriteFailed;
  const uint64_t freeBytes = info.totalBytes >= info.usedBytes
                           ? static_cast<uint64_t>( info.totalBytes ) - info.usedBytes
                           : 0;
  if( jsonStr.length() > freeBytes )
    return FErrorNotEnoughSpace;

  File fileOut = LittleFS.open( temporaryPath.c_str(), "w" );
  if( !fileOut )
    return FErrorWriteFailed;
  const size_t written = fileOut.write( reinterpret_cast<uint8_t const *>( jsonStr.c_str() ), jsonStr.length() );
  fileOut.close();

  File verify = LittleFS.open( temporaryPath.c_str(), "r" );
  const bool correctSize = verify && verify.size() == jsonStr.length();
  verify.close();
  if( written == jsonStr.length() && correctSize )
    return nullptr;

  LittleFS.remove( temporaryPath.c_str() );
  return FErrorWriteFailed;
 }

String TSwitchTimes::Set( String const & jsonStr )
 {
  const String formattedJson = FormatSwitchTimesJson( jsonStr );
  TSwitchDefs candidateRecords;
  char const * error = Parse( formattedJson, candidateRecords );
  if( error )
    return error;
  error = SaveTemporary( "/SwitchTimes.json", formattedJson );
  if( error )
    return error;

  Clear();
  error = LoadFile( "/SwitchTimes.json.tmp" );
  if( error )
   {
    LittleFS.remove( "/SwitchTimes.json.tmp" );
    const char * restoreError = LoadFile( "/SwitchTimes.json" );
    Debug( "TSwitchTimes::Set restore: %s%s%s\n",
           restoreError ? "NOK" : "OK", restoreError ? ", " : "", restoreError ? restoreError : "" );
    if( !restoreError && TmrSwWifi.SwitchingEnabled() )
      DoEvents( TmrSwWifi.GetTm() );
    return error;
   }

  if( !CommitTemporary( "/SwitchTimes.json" ) )
   {
    Clear();
    RecoverFiles( "/SwitchTimes.json" );
    const char * restoreError = LoadFile( "/SwitchTimes.json" );
    Debug( "TSwitchTimes::Set commit restore: %s%s%s\n",
           restoreError ? "NOK" : "OK", restoreError ? ", " : "", restoreError ? restoreError : "" );
    if( !restoreError && TmrSwWifi.SwitchingEnabled() )
      DoEvents( TmrSwWifi.GetTm() );
    return FErrorWriteFailed;
   }

  if( TmrSwWifi.SwitchingEnabled() )
    DoEvents( TmrSwWifi.GetTm() );
  return "1";
 }

String TSwitchTimes::SetIntervals( String const & jsonStr )
 {
  TSwitchIntervals newIntervals;
  const char * error = ParseIntervals( jsonStr, newIntervals );
  if( error )
    return error;
  TEvents newEvents;
  error = BuildEvents( FSwitchRecords, newIntervals, newEvents );
  if( error )
    return error;

  JsonDocument document;
  if( deserializeJson( document, jsonStr ) )
    return FErrorInvalidData;
  String normalizedJson;
  if( serializeJson( document, normalizedJson ) == 0 )
    return FErrorNotEnoughSpace;
  normalizedJson += '\n';

  error = SaveTemporary( "/Intervals.json", normalizedJson );
  if( error )
    return error;
  if( !CommitTemporary( "/Intervals.json" ) )
   {
    RecoverFiles( "/Intervals.json" );
    return FErrorWriteFailed;
   }

  FIntervalRecords.swap( newIntervals );
  FEvents.swap( newEvents );
  if( TmrSwWifi.SwitchingEnabled() )
    DoEvents( TmrSwWifi.GetTm() );
  return "1";
 }

void TSwitchTimes::CalculateScheduleState( uint16_t minuteOfWeek )
 {
  FSchedulePresent = !FEvents.empty();
  FScheduleState = false;
  FNextMow = 0;
  if( FEvents.empty() )
    return;

  FScheduleState = FEvents.back().On();
  size_t index;
  for( index = 0; index < FEvents.size() && FEvents[ index ].MinuteOfWeek() <= minuteOfWeek; ++index )
    FScheduleState = FEvents[ index ].On();
  if( index >= FEvents.size() )
    index = 0;
  FNextMow = FEvents[ index ].MinuteOfWeek();
 }

void TSwitchTimes::ApplyOutputState( bool on )
 {
  if( TmrSwHw.RelayStatusKnown() && TmrSwHw.RelayStatus() == on )
    return;
  TmrSwHw.RelaySet( on );
 }

void TSwitchTimes::ApplyAutomaticState(void)
 {
  UpdateHeatPi();
  if( TmrSwChannels.Channel().Role != THeaterRole::Timer &&
      TGnHeaterControl::FindProfile( TmrSwChannels.Profiles(), TmrSwChannels.Channel().Profile ) == nullptr &&
      !FOverrideActive && !FCalibrationOverrideActive )
    return;
  FAutomaticState = FHeaterControl.AutomaticState( TmrSwChannels.Channel().Role, TmrSwChannels.Channel().Profile,
                                                     FSchedulePresent, FScheduleState,
                                                     FHysteresisStateKnown, FHysteresisState,
                                                     &FChargeRunOn, TmrSwHw.RelayStatusKnown() && TmrSwHw.RelayStatus(),
                                                     !FOverrideActive && !FCalibrationOverrideActive, "0" );
  const auto * PiProfile=TGnHeaterControl::FindProfile(TmrSwChannels.Profiles(),TmrSwChannels.Channel().Profile);
  if( PiProfile && PiProfile->Pi.Active && TmrSwChannels.Channel().Role==THeaterRole::HeaterMain )
   {
    const auto & S=FHeatPi.Status();
    FAutomaticState=PiProfile->Pi.MainDevice==GnDeviceIdentity.WireId() && S.Valid ? S.MainRequest : PiProfile->NoTempOn;
    if(FSchedulePresent && !FScheduleState)FAutomaticState=false;
   }
#if GN_DEBUG_CHARGE_LEARNING
  TGnChargeRunOn::TLearningStep LearningStep;
  const bool HasLearningStep = FChargeRunOn.TakeLearningStep(LearningStep);
  bool LearningSaved = false;
#endif
  uint32_t LearnedFactor;
  if( FChargeRunOn.TakeLearned( LearnedFactor ) )
   {
    if( TmrSwChannels.SaveLearnedChargeFactor( TmrSwChannels.Channel().Profile, LearnedFactor ) )
     {
#if GN_DEBUG_CHARGE_LEARNING
      LearningSaved = true;
#endif
      RefreshProfiles(); GnTimeSync.LocalChanged();
     }
    else Debug( "Charge factor could not be saved\n" );
   }
#if GN_DEBUG_CHARGE_LEARNING
  if(HasLearningStep)
   {
    JsonDocument D;
    D["Ver"] = 1; D["Seq"] = ++FChargeLearningSequence;
    D["CompletedAt"] = static_cast<uint64_t>(time(nullptr));
    D["Prof"] = TmrSwChannels.Channel().Profile;
    D["BeforePct"] = LearningStep.BeforePercent; D["AfterPct"] = LearningStep.AfterPercent;
    D["AppliedSecPerPct"] = LearningStep.AppliedFactor;
    D["ChargeMs"] = LearningStep.ChargeMs;
    D["NewSecPerPct"] = LearningStep.NewFactor;
    D["Persistence"] = !LearningStep.SaveRequired ? "RamOnly" : LearningSaved ? "Saved" : "SaveFailed";
    FChargeLearningJson.clear(); serializeJson(D,FChargeLearningJson);
   }
#endif
  ApplyOutputState( FCalibrationOverrideActive ? FCalibrationOverrideState :
                    FOverrideActive ? FOverrideState : FAutomaticState );
 }

void TSwitchTimes::DoEvents( struct tm const & timeParts, bool useNextMow )
 {
  const uint16_t minuteOfWeek = GnMinuteOfWeek( timeParts );
  const bool regularEvent = !FEvents.empty() && FNextMow == minuteOfWeek;
  if( useNextMow && !regularEvent )
   {
    if( TmrSwChannels.Channel().Role == THeaterRole::Timer )
     {
      // A timer without a schedule is manually controlled. Do not overwrite
      // the relay state or a manual override just because no event exists.
      if( FEvents.empty() )
        return;
     }
    else
      UpdateHeater();
    return;
   }

  CalculateScheduleState( minuteOfWeek );
  ApplyAutomaticState();
 }

void TSwitchTimes::UpdateHeater(void)
 {
  if( TmrSwChannels.Channel().Role != THeaterRole::Timer )
    ApplyAutomaticState();
 }

bool TSwitchTimes::ManualToggle(void)
 {
  FChargeRunOn.Reset();
  FHeatPi.CancelLearning();
  FCalibrationOverrideActive = false;

  if( FOverrideActive )
   {
    FOverrideActive = false;
    FOverrideState = false;
    if( TmrSwChannels.Channel().Role == THeaterRole::Timer && FEvents.empty() )
      return true;
    ApplyAutomaticState();
    return true;
   }

  if( !TmrSwHw.RelayStatusKnown() )
   {
    FOverrideActive = false;
    FOverrideState = false;
    ApplyOutputState( false );
    return true;
   }

  const bool newState = !TmrSwHw.RelayStatus();
  FOverrideActive = true;
  FOverrideState = newState;
  ApplyOutputState( newState );
  return true;
 }

bool TSwitchTimes::SetCalibrationOverride( bool Active, bool State )
 {
  if( !TmrSwHw.RelayStatusKnown() )
    return false;

  FChargeRunOn.Reset();
  FHeatPi.CancelLearning();
  FCalibrationOverrideActive = Active;
  FCalibrationOverrideState = State;
  ApplyAutomaticState();
  const bool ExpectedState = Active ? State : FOverrideActive ? FOverrideState : FAutomaticState;
  return TmrSwHw.RelayStatusKnown() && TmrSwHw.RelayStatus() == ExpectedState;
 }

bool TSwitchTimes::GetStatus( struct tm const & timeParts, time_t currentTime,
                              bool & automaticState, time_t & nextChangeTime,
                              bool & nextChangeState ) const
 {
  automaticState = FAutomaticState;
  if( FEvents.empty() )
    return false;

  const uint16_t minuteOfWeek = GnMinuteOfWeek( timeParts );

  size_t index;
  for( index = 0; index < FEvents.size() && FEvents[ index ].MinuteOfWeek() <= minuteOfWeek; ++index )
    ;
  if( index >= FEvents.size() )
    index = 0;

  const uint16_t nextMow = FEvents[ index ].MinuteOfWeek();
  nextChangeState = FEvents[ index ].On();
  nextChangeTime = GnNextLocalMinuteOfWeek( currentTime, nextMow );
  return nextChangeTime != static_cast<time_t>( -1 );
 }

void TSwitchTimes::UpdateHeatPi(void)
 {
  const auto * Profile = TGnHeaterControl::FindProfile( TmrSwChannels.Profiles(), TmrSwChannels.Channel().Profile );
  if( !Profile || !Profile->Pi.Enabled || TmrSwChannels.Channel().Role != THeaterRole::HeaterMain )
   { FHeatPi.Configure({}); FHeatPiKey.clear(); GnUdpStatus.WatchStatus({}); return; }
  const auto & C = Profile->Pi;
  const TGNString Source = GnDeviceIdentity.ResolveTemperatureSource(Profile->TempId);
  const TGNString Key = TmrSwChannels.Channel().Id + "\n" + Profile->Id + Source + C.FloorDevice + C.FloorChannel +
    std::to_string(C.Active) + C.MainDevice + std::to_string(C.Setpoint) + ":" + std::to_string(C.FloorWatts) + ":" + std::to_string(C.MainMaxWatts) + ":" +
    std::to_string(C.CapacityWhPerK) + ":" + std::to_string(C.LossWattsPerK) + ":" + std::to_string(C.Kp) + ":" +
    std::to_string(C.TiSeconds) + ":" + std::to_string(C.WindowSeconds) + ":" + std::to_string(C.DelaySeconds);
  if( Key != FHeatPiKey )
   { FHeatPi.Configure(C); FHeatPiKey = Key; FHeatPiDue = 0; GnUdpStatus.WatchStatus(C.FloorDevice); }
  const uint64_t Now = GnMillis64();
  if( Now < FHeatPiDue ) return;
  FHeatPiDue = Now + 10000;
  TGnHeatPiInput X; X.Now = Now;
  float Temperature = 0;
  X.TemperatureValid = GnUdpStatus.GetMeasurement(Source,"Temperature",Temperature);
  X.Temperature = Temperature;
  X.TemperatureAgeMs = GnUdpStatus.MeasurementAgeMs(Source,"Temperature");
#if TMR_SW_HAS_POWER_METER
  X.PowerValid = TmrSwHw.PowerMeterValid(); X.MainWatts = TmrSwHw.ActivePower(); X.PowerAgeMs = TmrSwHw.PowerMeterAgeMs();
#endif
  X.MainOn = TmrSwHw.RelayStatusKnown() && TmrSwHw.RelayStatus();
  X.Manual = FOverrideActive || FCalibrationOverrideActive;
  X.ScheduleAllowed = !FSchedulePresent || FScheduleState;
  TGNString Peer; uint64_t Revision = 0;
  if( GnUdpStatus.GetWatchedStatus(C.FloorDevice,Peer,Revision) )
   {
    JsonDocument D;
    if( !deserializeJson(D,Peer.c_str()) && D["Identity"]["BootCnt"].is<uint64_t>() &&
        D["CurrentTime"].is<uint64_t>() && (D["TimeSynchronized"] | false) &&
        std::abs(double(time(nullptr))-D["CurrentTime"].as<double>()) <= 60 )
     {
      X.PeerBoot = D["Identity"]["BootCnt"].as<uint64_t>();
      X.FloorAgeMs = uint32_t(std::max(0.0,double(time(nullptr))-D["CurrentTime"].as<double>())*1000);
      unsigned Matches = 0;
      for( JsonObjectConst Row : D["Chs"].as<JsonArrayConst>() )
        if( C.FloorDevice+"/"+C.FloorChannel == (Row["Id"] | "") )
         {
          ++Matches;
          X.FloorOverride = (Row["OvrActive"] | false) || (Row["CalOvrActive"] | false);
          X.FloorValid = Row["State"].is<bool>() && !X.FloorOverride;
          X.FloorOn = Row["State"] | false;
         }
      X.FloorValid = X.FloorValid && Matches == 1;
      for( JsonObjectConst Row : D["Meas"].as<JsonArrayConst>() )
        if( strcmp(Row["Kind"] | "","RoomTemperature")==0 && (Row["Valid"] | false) && !Row["Value"].is<bool>() && Row["Value"].is<double>() &&
            Row["UpdatedAt"].is<uint64_t>() && std::abs(double(time(nullptr))-Row["UpdatedAt"].as<double>())<=20 )
         { X.RoomValid = true; X.RoomTemperature = Row["Value"].as<double>();
           X.RoomAgeMs = uint32_t(std::max(0.0,double(time(nullptr))-Row["UpdatedAt"].as<double>())*1000); }
     }
   }
  FHeatPi.Update(X);
 }

TGNString TSwitchTimes::HeatPiJson(void) const
 {
  auto const & S=FHeatPi.Status();
  const auto * Profile=TGnHeaterControl::FindProfile(TmrSwChannels.Profiles(),TmrSwChannels.Channel().Profile);
  if(!S.Available || !Profile)return "null";
  TUdpHeatDiagnostics D;
  D.Active=Profile->Pi.Active;D.Prof=TmrSwChannels.Channel().Profile;
  TJsonHeatConfig C;
  C.Setpoint=Profile->Pi.Setpoint;
  D.Cfg=C;
  D.ActualMainOn=S.ActualMainOn;D.ActualFloorOn=S.ActualFloorOn;
  D.LearnReason=S.LearningReason;D.WindowAgeSec=S.WindowAgeSeconds;
  D.Valid=S.Valid;D.Reason=S.Reason;D.Learn=S.Learning;
  D.Temp=S.Temperature;D.ErrK=S.Error;
  D.ProportionalW=S.P;D.IntegralW=S.I;D.FeedForwardW=S.FeedForward;
  D.DemandW=S.Demand;D.FloorDemandW=S.FloorDemand;D.MainDemandW=S.MainDemand;
  D.FloorRequest=S.Valid && !FOverrideActive && !FCalibrationOverrideActive && (!FSchedulePresent || FScheduleState) && S.FloorRequest;
  D.MainRequest=S.MainRequest;D.FloorAvailable=S.FloorAvailable;D.ActualW=S.ActualWatts;
  D.CapacityWhPerK=S.Capacity;D.LossWPerK=S.Loss;D.Kp=S.Kp;
  D.TiSec=S.TiSeconds;D.Samples=S.Samples;D.LearnedWindows=S.LearnedWindows;
  return GnWriteJson(D);
 }

TGNString TSwitchTimes::HeatControlJson(void) const
 {
  const auto * P=TGnHeaterControl::FindProfile(TmrSwChannels.Profiles(),TmrSwChannels.Channel().Profile);
  if(!P || TmrSwChannels.Channel().Role!=THeaterRole::HeaterMain || P->Pi.FloorDevice.empty())return "null";
  TUdpHeatControl D;
  D.Active=P->Pi.Active;D.Prof=TmrSwChannels.Channel().Profile;
  D.FloorEndpoint=P->Pi.FloorDevice+"/"+P->Pi.FloorChannel;
  const auto & S=FHeatPi.Status();
  D.FloorRequest=P->Pi.Active && S.Valid && !FOverrideActive && !FCalibrationOverrideActive && (!FSchedulePresent || FScheduleState) && S.FloorRequest;
  return GnWriteJson(D);
 }
