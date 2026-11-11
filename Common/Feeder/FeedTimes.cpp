// FeedTimes.cpp, Version: 2.49
//****************************************************************
// Includes
//****************************************************************

#define Do_TJsonFeedTime
#include <GnBasics.h>
#include <GnJsonSchema.h>
#include "FeedTimes.h"

#include "FeederWifi.h"
#include "FeederDrive.h"

namespace
 {
char const * const ErrorInvalidData = "InvalidData";
char const * const ErrorDuplicateActiveTime = "DuplicateActiveTime";
char const * const ErrorNotEnoughSpace = "NotEnoughSpace";
char const * const ErrorWriteFailed = "WriteFailed";



String FormatFeedTimesJson( String const & JsonStr )
 {
  JsonDocument Doc;
  if( deserializeJson( Doc, JsonStr ) != DeserializationError::Ok || !Doc.is<JsonArray>() )
    return JsonStr;

  const JsonArrayConst Rows = Doc.as<JsonArrayConst>();
  if( Rows.size() == 0 )
    return "[]\n";

  size_t Widths[ 4 ] = { 0, 0, 0, 0 };
  for( JsonVariantConst Value : Rows )
   {
    const JsonArrayConst Row = Value.as<JsonArrayConst>();
    if( Row.isNull() || ( Row.size() != 3 && Row.size() != 4 ) )
      return JsonStr;
    for( size_t Column = 0;  Column < Row.size();  ++Column )
     {
      const size_t CellLength = measureJson( Row[ Column ] );
      if( CellLength > Widths[ Column ] )
        Widths[ Column ] = CellLength;
     }
   }

  TGNString Result;
  const auto Build = [&]( auto & Output )
   {
    Output += "[\n";
    size_t RowIndex = 0;
    for( JsonVariantConst Value : Rows )
     {
      const JsonArrayConst Row = Value.as<JsonArrayConst>();
      Output += "  [ ";
      for( size_t Column = 0;  Column < Row.size();  ++Column )
       {
        const JsonVariantConst Cell = Row[ Column ];
        const size_t CellLength = measureJson( Cell );
        GnAppendGenerated( Output, CellLength, [&]( TGNString & Target )
         {
          TGnStringAppendWriter Writer( Target );
          serializeJson( Cell, Writer );
         } );
        if( Column + 1 < Row.size() )
         {
          Output += ", ";
          Output.append( Widths[ Column ] - CellLength, ' ' );
         }
       }
      Output += " ]";
      if( ++RowIndex < Rows.size() )
        Output += ',';
      Output += '\n';
     }
    Output += "]\n";
   };

  if( !GnBuildStringExact( Result, Build ) )
    return JsonStr;
  return Result;
 }
 } // namespace

//****************************************************************
// TFeedTimes
//****************************************************************

TFeedTimes::TFeedTimes(void)
 {
 }

bool TFeedTimes::Setup(void)
 {
  return Load( ReadFile( F( "/FeedTimes.json" ) ) );
 }

bool TFeedTimes::ParseTime( String const & Value, uint8_t & Hour, uint8_t & Minute )
 {
  if(!GnTimeOfDay(Value.c_str()))return false;
  Hour=uint8_t((Value[0]-'0')*10+Value[1]-'0');
  Minute=uint8_t((Value[3]-'0')*10+Value[4]-'0');
  return true;
 }

bool TFeedTimes::HasActiveTimeConflict( TFeedRecords const & Records )
 {
  for( size_t First = 0;  First < Records.size();  ++First )
   {
    TFeedRecord const & FirstRecord = Records[ First ];
    if( !FirstRecord.wday.bits.Active )
      continue;

    const uint8_t FirstDays = FirstRecord.wday.mask & 0x7F;
    for( size_t Second = First + 1;  Second < Records.size();  ++Second )
     {
      TFeedRecord const & SecondRecord = Records[ Second ];
      if( !SecondRecord.wday.bits.Active )
        continue;
      if( FirstRecord.h == SecondRecord.h && FirstRecord.m == SecondRecord.m
       && ( FirstDays & ( SecondRecord.wday.mask & 0x7F ) ) )
        return true;
     }
   }
  return false;
 }

char const * TFeedTimes::Parse( String const & JsonStr, TFeedRecords & Records )
 {
  if( !GnHasHeapSpace( JsonStr.length() + 512 ) )
    return ErrorNotEnoughSpace;

  JsonDocument Doc;
  const DeserializationError Error = deserializeJson( Doc, JsonStr );
  if( Error == DeserializationError::NoMemory )
    return ErrorNotEnoughSpace;
  if( Error != DeserializationError::Ok || !Doc.is<JsonArray>() )
    return ErrorInvalidData;

  TFeedRecords NewRecords;
  JsonArray Array = Doc.as<JsonArray>();
  if( Array.size() > SIZE_MAX / sizeof( TFeedRecord ) )
    return ErrorNotEnoughSpace;
  const size_t RecordBytes = Array.size() * sizeof( TFeedRecord );
  if( !GnHasHeapSpace( RecordBytes, RecordBytes ) )
    return ErrorNotEnoughSpace;
  NewRecords.reserve( Array.size() );

  for( JsonVariant Value : Array )
   {
    TJsonFeedTime Row;
    TGNString Error;
    if( !GnRead( TGnWireValue(Value), Row, Error ) )return ErrorInvalidData;
    TFeedRecord Record{};
    Record.h=uint8_t((Row.Time[0]-'0')*10+Row.Time[1]-'0');
    Record.m=uint8_t((Row.Time[3]-'0')*10+Row.Time[4]-'0');
    Record.wday.mask=Row.Days;
    Record.portions=Row.Portions;
    NewRecords.push_back( Record );
   }

  if( HasActiveTimeConflict( NewRecords ) )
    return ErrorDuplicateActiveTime;

  Records.swap( NewRecords );
  return nullptr;
 }

bool TFeedTimes::Load( String const & JsonStr )
 {
  TFeedRecords NewRecords;
  const char * Error = Parse( JsonStr, NewRecords );
  Debug( "TFeedTimes::Load: %s%s%s\n", Error ? "NOK" : "OK", Error ? ", " : "", Error ? Error : "" );
  if( Error )
    return false;

  FFeedRecords.swap( NewRecords );
  FNextFeedCacheMinute = 0;
  FNextFeedValid = false;
  return true;
 }

char const * TFeedTimes::Save( String const & JsonStr )
 {
  TFSInfo Info;
  if( !GetLittleFSInfo( Info ) )
    return ErrorWriteFailed;

  char const * const Path = "/FeedTimes.json";
  File PreviousFile = LittleFS.open( Path, "r" );
  const size_t PreviousSize = PreviousFile ? PreviousFile.size() : 0;
  PreviousFile.close();
  if( PreviousSize && !GnHasHeapSpace( PreviousSize + 1, PreviousSize + 1 ) )
    return ErrorNotEnoughSpace;
  const String PreviousJson = ReadFile( Path );
  if( PreviousJson.length() != PreviousSize )
    return ErrorNotEnoughSpace;
  const uint64_t FreeBytes = Info.totalBytes >= Info.usedBytes
                           ? static_cast<uint64_t>( Info.totalBytes ) - Info.usedBytes
                           : 0;
  const uint64_t AvailableBytes = FreeBytes + PreviousSize;
  if( JsonStr.length() > AvailableBytes )
    return ErrorNotEnoughSpace;

  File FileOut = LittleFS.open( Path, "w" );
  if( !FileOut )
    return ErrorWriteFailed;
  const size_t Written = FileOut.write( reinterpret_cast<uint8_t const *>( JsonStr.c_str() ), JsonStr.length() );
  FileOut.close();

  if( Written == JsonStr.length() )
   {
    File Verify = LittleFS.open( Path, "r" );
    const bool CorrectSize = Verify && Verify.size() == JsonStr.length();
    Verify.close();
    if( CorrectSize )
      return nullptr;
   }

  File Restore = LittleFS.open( Path, "w" );
  if( Restore )
   {
    Restore.write( reinterpret_cast<uint8_t const *>( PreviousJson.c_str() ), PreviousJson.length() );
    Restore.close();
   }
  return ErrorWriteFailed;
 }

String TFeedTimes::Set( String const & JsonStr )
 {
  TFeedRecords NewRecords;
  const char * Error = Parse( JsonStr, NewRecords );
  if( Error )
    return Error;
  const String FormattedJson = FormatFeedTimesJson( JsonStr );
  Error = Save( FormattedJson );
  if( Error )
    return Error;

  FFeedRecords.swap( NewRecords );
  FNextFeedCacheMinute = 0;
  FNextFeedValid = false;
  return "1";
 }

uint8_t TFeedTimes::CheckTime( struct tm const & tm ) const
 {
  for( TFeedRecord const & Record : FFeedRecords )
    if( Record.wday.bits.Active
     && ( Record.wday.mask & ( 1 << tm.tm_wday ) )
     && Record.h == tm.tm_hour && Record.m == tm.tm_min )
      return Record.portions;
  return 0;
 }

bool TFeedTimes::GetNextFeed( time_t & FeedTime, uint8_t & Portions ) const
 {
  const time_t Now = time( nullptr );
  const time_t CacheMinute = Now / 60;
  if( FNextFeedCacheMinute == CacheMinute )
   {
    if( !FNextFeedValid )
      return false;
    FeedTime = FNextFeedTime;
    Portions = FNextFeedPortions;
    return true;
   }

  time_t BestTime = 0;
  uint8_t BestPortions = 0;

  struct tm LocalNow;
  localtime_r( &Now, &LocalNow );
  for( unsigned DayOffset = 0;  DayOffset <= 7;  ++DayOffset )
    for( TFeedRecord const & Record : FFeedRecords )
     {
      if( !Record.wday.bits.Active || Record.portions == 0 )
        continue;

      struct tm Candidate = LocalNow;
      Candidate.tm_mday += static_cast<int>( DayOffset );
      Candidate.tm_hour = Record.h;
      Candidate.tm_min = Record.m;
      Candidate.tm_sec = 0;
      Candidate.tm_isdst = -1;
      const time_t CandidateTime = mktime( &Candidate );
      if( CandidateTime <= Now || !( Record.wday.mask & ( 1 << Candidate.tm_wday ) ) )
        continue;
      if( BestTime != 0 && CandidateTime >= BestTime )
        continue;

      BestTime = CandidateTime;
      BestPortions = Record.portions;
     }

  FNextFeedCacheMinute = CacheMinute;
  FNextFeedTime = BestTime;
  FNextFeedPortions = BestPortions;
  FNextFeedValid = BestTime != 0;
  if( !FNextFeedValid )
    return false;
  FeedTime = FNextFeedTime;
  Portions = FNextFeedPortions;
  return true;
 }

void TFeedTimes::Loop(void)
 {
  if( !FeederWifi.TimeIsValid() )
    return;

  static struct tm LastTime{};
  time_t TimeNow;
  struct tm CurrentTime;
  time( &TimeNow );
  localtime_r( &TimeNow, &CurrentTime );

  if( LastTime.tm_min != CurrentTime.tm_min )
   {
    Debug( "TFeedTimes::Loop every min\n" );
    const uint8_t Portions = CheckTime( CurrentTime );
    if( Portions > 0 )
      FeederDrive.Feed( Portions );
   }
  LastTime = CurrentTime;
 }
