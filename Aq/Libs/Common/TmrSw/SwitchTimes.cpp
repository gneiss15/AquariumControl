// SwitchTimes.cpp, Version: 2.48
//****************************************************************
// Includes
//****************************************************************

#include <GnBasics.h>
#include "SwitchTimes.h"

#include "TmrSwChannels.h"
#include "TmrSwHw.h"
#include "TmrSwWifi.h"

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

bool TSwitchTimes::IsDigit( char value )
 {
  return value >= '0' && value <= '9';
 }

bool TSwitchTimes::ParseTime( String const & value, uint16_t & minuteOfDay )
 {
  if( value.length() != 5 || value[ 2 ] != ':'
   || !IsDigit( value[ 0 ] ) || !IsDigit( value[ 1 ] )
   || !IsDigit( value[ 3 ] ) || !IsDigit( value[ 4 ] ) )
    return false;

  const unsigned hour = ( value[ 0 ] - '0' ) * 10 + value[ 1 ] - '0';
  const unsigned minute = ( value[ 3 ] - '0' ) * 10 + value[ 4 ] - '0';
  if( hour > 23 || minute > 59 )
    return false;

  minuteOfDay = GnMinuteOfDay( hour, minute );
  return true;
 }

bool TSwitchTimes::ParseDuration( String const & value, uint16_t & minutes )
 {
  if( value.length() != 5 || value[ 2 ] != ':'
   || !IsDigit( value[ 0 ] ) || !IsDigit( value[ 1 ] )
   || !IsDigit( value[ 3 ] ) || !IsDigit( value[ 4 ] ) )
    return false;

  const unsigned hours = ( value[ 0 ] - '0' ) * 10 + value[ 1 ] - '0';
  const unsigned minute = ( value[ 3 ] - '0' ) * 10 + value[ 4 ] - '0';
  if( minute > 59 )
    return false;
  minutes = static_cast<uint16_t>( hours * 60 + minute );
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
    if( !value.is<JsonArray>() )
      return FErrorInvalidData;
    JsonArray row = value.as<JsonArray>();
    if( row.size() != 3 && row.size() != 4 )
      return FErrorInvalidData;

    uint16_t minuteOfDay;
    const String timeValue = row[ 0 ].as<String>();
    const int wdayMask = row[ 1 ].as<int>();
    const int on = row[ 2 ].as<int>();
    if( !ParseTime( timeValue, minuteOfDay )
     || wdayMask < 0 || wdayMask > 255
     || on < 0 || on > 1 )
      return FErrorInvalidData;

    TSwitchDef record;
    record.Init( minuteOfDay, static_cast<uint16_t>( on ), static_cast<uint8_t>( wdayMask ) );
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
    JsonArray row = value.as<JsonArray>();
    if( row.isNull() || ( row.size() != 5 && row.size() != 6 ) ||
        !row[ 0 ].is<const char *>() || !row[ 1 ].is<const char *>() ||
        !row[ 2 ].is<const char *>() || !row[ 3 ].is<int>() || !row[ 4 ].is<const char *>() )
      return FErrorInvalidData;

    uint16_t startMinuteOfDay = 0;
    uint16_t periodMinutes = 0;
    uint16_t onMinutes = 0;
    const String channelId = row[ 4 ].as<String>();
    const int wdayMask = row[ 3 ].as<int>();
    if( !ParseTime( row[ 0 ].as<String>(), startMinuteOfDay ) ||
        !ParseDuration( row[ 1 ].as<String>(), periodMinutes ) || periodMinutes == 0 ||
        !ParseDuration( row[ 2 ].as<String>(), onMinutes ) || onMinutes == 0 || onMinutes >= periodMinutes ||
        wdayMask < 0 || wdayMask > 255 || channelId.length() == 0 ||
        ( ( ExpectedChannel ? *ExpectedChannel != '\0' : !TmrSwChannels.Channel().Id.empty() ) && channelId != ( ExpectedChannel ? ExpectedChannel : TmrSwChannels.Channel().Id.c_str() ) ) )
      return FErrorInvalidData;

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
  if( TmrSwChannels.Channel().Role != THeaterRole::Timer &&
      TGnHeaterControl::FindProfile( TmrSwChannels.Profiles(), TmrSwChannels.Channel().Profile ) == nullptr &&
      !FOverrideActive && !FCalibrationOverrideActive )
    return;
  FAutomaticState = FHeaterControl.AutomaticState( TmrSwChannels.Channel().Role, TmrSwChannels.Channel().Profile,
                                                     FSchedulePresent, FScheduleState,
                                                     FHysteresisStateKnown, FHysteresisState );
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
