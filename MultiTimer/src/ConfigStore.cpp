// ConfigStore.cpp, Version: 2.47
#include <GnBasics.h>
#include "BuildConfig.h"
#if !defined( SYSTEM_ESP )
#include "ConfigStore.h"
#include <GnUdpStatus.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <json-c/json.h>
#include <sstream>

namespace
 {
char const * const ErrorDuplicateActiveTime = "DuplicateActiveTime";

bool HasActiveTimeConflict( const TSwitchTimes & records )
 {
  for( size_t first = 0;  first < records.size();  ++first )
   {
    const TSwitchTime & firstRecord = records[ first ];
    if( !firstRecord.wday.bits.Active )
      continue;
    for( size_t second = first + 1;  second < records.size();  ++second )
     {
      const TSwitchTime & secondRecord = records[ second ];
      if( secondRecord.wday.bits.Active && firstRecord.MinuteOfDay == secondRecord.MinuteOfDay &&
          firstRecord.Channel == secondRecord.Channel &&
          ( firstRecord.wday.mask & secondRecord.wday.mask & 0x7F ) )
        return true;
     }
   }
  return false;
 }

bool HasActiveTimeConflict( const TSwitchIntervals & records )
 {
  for( size_t first = 0;  first < records.size();  ++first )
   {
    const TSwitchInterval & firstRecord = records[ first ];
    if( !firstRecord.wday.bits.Active )
      continue;
    for( size_t second = first + 1;  second < records.size();  ++second )
     {
      const TSwitchInterval & secondRecord = records[ second ];
      if( secondRecord.wday.bits.Active && firstRecord.StartMinuteOfDay == secondRecord.StartMinuteOfDay &&
          firstRecord.Channel == secondRecord.Channel &&
          ( firstRecord.wday.mask & secondRecord.wday.mask & 0x7F ) )
        return true;
     }
   }
  return false;
 }

bool HasActiveTimeConflict( const TSwitchTimes & records, const TSwitchTime & candidate )
 {
  if( !candidate.wday.bits.Active )
    return false;
  for( const TSwitchTime & record : records )
    if( record.wday.bits.Active && record.MinuteOfDay == candidate.MinuteOfDay &&
        record.Channel == candidate.Channel && ( record.wday.mask & candidate.wday.mask & 0x7F ) )
      return true;
  return false;
 }

bool HasActiveTimeConflict( const TSwitchIntervals & records, const TSwitchInterval & candidate )
 {
  if( !candidate.wday.bits.Active )
    return false;
  for( const TSwitchInterval & record : records )
    if( record.wday.bits.Active && record.StartMinuteOfDay == candidate.StartMinuteOfDay &&
        record.Channel == candidate.Channel && ( record.wday.mask & candidate.wday.mask & 0x7F ) )
      return true;
  return false;
 }

void AddUnknownChannelWarning( std::vector<TGNString> & warnings, const TGNString & channelName )
 {
  const TGNString warning = "Unbekannte Kanal-ID (" + channelName + ")";
  if( std::find( warnings.begin(), warnings.end(), warning ) == warnings.end() )
    warnings.push_back( warning );
 }

void AddInvalidRowWarning( std::vector<TGNString> & warnings, size_t rowNr )
 {
  warnings.push_back( "Fehlerhafte Zeile (" + std::to_string( rowNr ) + ")" );
 }

TGNString ReadAll( const TGNString & FileName, TGNString & Error )
 {
  std::ifstream File( FileName );
  if ( !File )
   {
    Error = "Cannot open " + FileName;
    return {};
   }
  std::ostringstream Data;
  Data << File.rdbuf();
  return Data.str();
 }

bool GetOptionalBool( json_object * Obj, const char * Name, bool Default, bool & Result )
 {
  json_object * Value = nullptr;
  if( !json_object_object_get_ex( Obj, Name, &Value ) )
   {
    Result = Default;
    return true;
   }
  if( !json_object_is_type( Value, json_type_boolean ) )
    return false;
  Result = json_object_get_boolean( Value );
  return true;
 }

bool GetOptionalInt( json_object * Obj, const char * Name, std::optional<int> & Result )
 {
  json_object * Value = nullptr;
  if( !json_object_object_get_ex( Obj, Name, &Value ) )
   {
    Result = std::nullopt;
    return true;
   }
  if( !json_object_is_type( Value, json_type_int ) )
    return false;
  Result = json_object_get_int( Value );
  return true;
 }

bool GetRequiredString( json_object * Obj, const char * Name, TGNString & Value )
 {
  json_object * JsonValue = nullptr;
  if( !json_object_object_get_ex( Obj, Name, &JsonValue ) ||
      !json_object_is_type( JsonValue, json_type_string ) )
    return false;
  const char * Text = json_object_get_string( JsonValue );
  if( Text == nullptr || *Text == '\0' )
    return false;
  Value = Text;
  return true;
 }

bool GetRequiredFloat( json_object * Obj, const char * Name, float & Value )
 {
  json_object * JsonValue = nullptr;
  if( !json_object_object_get_ex( Obj, Name, &JsonValue ) ||
      ( !json_object_is_type( JsonValue, json_type_double ) &&
        !json_object_is_type( JsonValue, json_type_int ) ) )
    return false;
  const double Number = json_object_get_double( JsonValue );
  const float Result = static_cast<float>( Number );
  if( !std::isfinite( Number ) || !std::isfinite( Result ) )
    return false;
  Value = Result;
  return true;
 }

bool ParseChannelRole( json_object * Row, TChannelConfig & Entry, TGNString & Error )
 {
  json_object * Role = nullptr;
  if( !json_object_object_get_ex( Row, "Role", &Role ) || !json_object_is_type( Role, json_type_string ) )
   {
    Error = "Role missing or invalid for channel " + Entry.Id;
    return false;
   }
  const TGNString RoleText = json_object_get_string( Role );
  if( !TGnHeaterControl::ParseRole( RoleText, Entry.Role ) )
   {
    Error = "Invalid Role for channel " + Entry.Id + ": " + RoleText;
    return false;
   }

  json_object * Profile = nullptr;
  if( json_object_object_get_ex( Row, "Profile", &Profile ) )
   {
    if( !json_object_is_type( Profile, json_type_string ) ||
        json_object_get_string( Profile ) == nullptr || *json_object_get_string( Profile ) == '\0' )
     {
      Error = "Invalid Profile for channel " + Entry.Id;
      return false;
     }
    Entry.Profile = json_object_get_string( Profile );
   }

  if( Entry.Role == TChannelRole::Timer )
   {
    if( !Entry.Profile.empty() )
     {
      Error = "Profile requires heater Role for channel " + Entry.Id;
      return false;
     }
    return true;
   }
  if( Entry.Profile.empty() )
   {
    Error = "Profile missing for heater channel " + Entry.Id;
    return false;
   }
  return true;
 }

bool ParseHeaterProfiles( json_object * Root, THeaterProfiles & Profiles, TGNString & Error )
 {
  json_object * Array = nullptr;
  if( !json_object_object_get_ex( Root, "Profiles", &Array ) )
   {
    Profiles.clear();
    return true;
   }
  if( !json_object_is_type( Array, json_type_array ) )
   {
    Error = "Invalid Profiles JSON";
    return false;
   }

  THeaterProfiles Result;
  Result.reserve( json_object_array_length( Array ) );
  for( size_t Index = 0; Index < json_object_array_length( Array ); ++Index )
   {
    json_object * Row = json_object_array_get_idx( Array, Index );
    if( !json_object_is_type( Row, json_type_object ) )
     {
      Error = "Invalid heater profile in row " + std::to_string( Index + 1 );
      return false;
     }

    THeaterProfile Entry;
    json_object * NoTempOn = nullptr;
    if( !GetRequiredString( Row, "Id", Entry.Id ) ||
        !GetRequiredString( Row, "TempId", Entry.TempId ) ||
        !GetRequiredFloat( Row, "FloorOn", Entry.FloorOn ) ||
        !GetRequiredFloat( Row, "FloorOff", Entry.FloorOff ) ||
        !GetRequiredFloat( Row, "MainOn", Entry.MainOn ) ||
        !GetRequiredFloat( Row, "MainOff", Entry.MainOff ) ||
        !json_object_object_get_ex( Row, "NoTempOn", &NoTempOn ) ||
        !json_object_is_type( NoTempOn, json_type_boolean ) )
     {
      Error = "Invalid heater profile in row " + std::to_string( Index + 1 );
      return false;
     }
    json_object * Measurement = nullptr;
    if( json_object_object_get_ex( Row, "Measurement", &Measurement ) )
     {
      if( !json_object_is_type( Measurement, json_type_string ) ||
          !GnUdpMeasurementFieldValid( json_object_get_string( Measurement ) ) )
       { Error = "Invalid profile Measurement"; return false; }
      Entry.Measurement = json_object_get_string( Measurement );
     }
    if( TGnHeaterControl::FindProfile( Result, Entry.Id ) != nullptr )
     {
      Error = "Duplicate heater profile ID: " + Entry.Id;
      return false;
     }
    if( Entry.FloorOn >= Entry.FloorOff || Entry.MainOn >= Entry.MainOff )
     {
      Error = "Invalid heater hysteresis in profile " + Entry.Id;
      return false;
     }
    Entry.NoTempOn = json_object_get_boolean( NoTempOn );
    Result.push_back( std::move( Entry ) );
   }

  Profiles = std::move( Result );
  return true;
 }

bool ParseTime( const TGNString & value, bool duration, uint16_t & minutes )
 {
  if( value.size() != 5 || value[ 2 ] != ':' || !std::isdigit( value[ 0 ] ) ||
      !std::isdigit( value[ 1 ] ) || !std::isdigit( value[ 3 ] ) || !std::isdigit( value[ 4 ] ) )
    return false;

  const unsigned hours = static_cast<unsigned>( value[ 0 ] - '0' ) * 10 + value[ 1 ] - '0';
  const unsigned minute = static_cast<unsigned>( value[ 3 ] - '0' ) * 10 + value[ 4 ] - '0';
  if( minute > 59 || ( !duration && hours > 23 ) )
    return false;
  minutes = static_cast<uint16_t>( hours * 60 + minute );
  return true;
 }

void FormatTime( uint16_t minutes, char ( &value )[ 6 ] )
 {
  const unsigned hours = minutes / 60;
  const unsigned minute = minutes % 60;
  value[ 0 ] = static_cast<char>( '0' + hours / 10 );
  value[ 1 ] = static_cast<char>( '0' + hours % 10 );
  value[ 2 ] = ':';
  value[ 3 ] = static_cast<char>( '0' + minute / 10 );
  value[ 4 ] = static_cast<char>( '0' + minute % 10 );
  value[ 5 ] = '\0';
 }

TGNString FormatJsonRows( json_object * Array, unsigned Indent )
 {
  if( !json_object_is_type( Array, json_type_array ) )
    return {};

  const size_t RowCount = json_object_array_length( Array );
  size_t ColumnCount = 0;
  for( size_t RowIndex = 0;  RowIndex < RowCount;  ++RowIndex )
   {
    json_object * Row = json_object_array_get_idx( Array, RowIndex );
    if( !json_object_is_type( Row, json_type_array ) )
      return {};
    ColumnCount = std::max( ColumnCount, json_object_array_length( Row ) );
   }

  std::vector<size_t> Widths( ColumnCount, 0 );
  for( size_t RowIndex = 0;  RowIndex < RowCount;  ++RowIndex )
   {
    json_object * Row = json_object_array_get_idx( Array, RowIndex );
    for( size_t Column = 0;  Column < json_object_array_length( Row );  ++Column )
     {
      json_object * Cell = json_object_array_get_idx( Row, Column );
      const char * const Text = json_object_to_json_string_ext( Cell, JSON_C_TO_STRING_PLAIN );
      Widths[ Column ] = std::max( Widths[ Column ], strlen( Text ) );
     }
   }

  TGNString Result;
  const auto Build = [&]( auto & Output )
   {
    Output += '[';
    if( RowCount == 0 )
     {
      Output += ']';
      return;
     }

    Output += '\n';
    for( size_t RowIndex = 0;  RowIndex < RowCount;  ++RowIndex )
     {
      json_object * Row = json_object_array_get_idx( Array, RowIndex );
      Output.append( Indent + 2, ' ' );
      Output += "[ ";
      const size_t CellCount = json_object_array_length( Row );
      for( size_t Column = 0;  Column < CellCount;  ++Column )
       {
        json_object * Cell = json_object_array_get_idx( Row, Column );
        const char * const Text = json_object_to_json_string_ext( Cell, JSON_C_TO_STRING_PLAIN );
        const size_t TextLength = strlen( Text );
        Output.append( Text, TextLength );
        if( Column + 1 < CellCount )
         {
          Output += ", ";
          Output.append( Widths[ Column ] - TextLength, ' ' );
         }
       }
      Output += " ]";
      if( RowIndex + 1 < RowCount )
        Output += ',';
      Output += '\n';
     }
    Output.append( Indent, ' ' );
    Output += ']';
   };

  if( !GnBuildStringExact( Result, Build ) )
    return {};
  return Result;
 }

TGNString FormatScheduleJson( json_object * SwitchTimes, json_object * Intervals )
 {
  const TGNString SwitchText = FormatJsonRows( SwitchTimes, 2 );
  const TGNString IntervalText = FormatJsonRows( Intervals, 2 );
  if( SwitchText.empty() || IntervalText.empty() )
    return {};

  TGNString Result;
  const auto Build = [&]( auto & Output )
   {
    Output += "{\n  \"SwitchTimes\":";
    Output.append( SwitchText );
    Output += ",\n  \"Intervals\":";
    Output.append( IntervalText );
    Output += "\n}";
   };
  if( !GnBuildStringExact( Result, Build ) )
    return {};
  return Result;
 }


 } // namespace

TChannelId TConfigStore::FindChannelId( const TChannelConfigs & Channels, const TGNString & Id )
 {
  const auto It = std::find_if( Channels.begin(), Channels.end(), [&]( const TChannelConfig & Channel ) { return Channel.Id == Id; } );
  if ( It == Channels.end() )
   {
    return InvalidChannelId;
   }
  return static_cast<TChannelId>( std::distance( Channels.begin(), It ) );
 }

const TGNString & TConfigStore::GetChannelConfigId( const TChannelConfigs & Channels, TChannelId Id )
 {
  static const TGNString InvalidId;
  return Id < Channels.size() ? Channels[Id].Id : InvalidId;
 }

bool TConfigStore::ChannelsFromJson( const TGNString & Json, TChannelConfigs & Channels,
                                      THeaterProfiles & HeaterProfiles, TGNString & Error ) const
 {
  Error.clear();
  json_object * Root = json_tokener_parse( Json.c_str() );
  json_object * Array = nullptr;
  if( !Root || !json_object_is_type( Root, json_type_object ) ||
      !json_object_object_get_ex( Root, "Channels", &Array ) || !json_object_is_type( Array, json_type_array ) )
   {
    Error = "Invalid Channels JSON";
    if( Root )
      json_object_put( Root );
    return false;
   }
  if( json_object_array_length( Array ) >= InvalidChannelId )
   {
    Error = "Too many channels";
    json_object_put( Root );
    return false;
   }

  THeaterProfiles NewHeaterProfiles;
  if( !ParseHeaterProfiles( Root, NewHeaterProfiles, Error ) )
   {
    json_object_put( Root );
    return false;
   }

  TChannelConfigs Result;
  for( size_t Index = 0; Index < json_object_array_length( Array ); ++Index )
   {
    json_object * Row = json_object_array_get_idx( Array, Index );
    TChannelConfig Entry;
    if( !json_object_is_type( Row, json_type_object ) || !GetRequiredString( Row, "Id", Entry.Id ) )
     {
      Error = "Channel ID missing in row " + std::to_string( Index + 1 );
      json_object_put( Root );
      return false;
     }
    json_object * Position = nullptr;
    if( json_object_object_get_ex( Row, "PhysPos", &Position ) )
     {
      if( !json_object_is_type( Position, json_type_string ) )
       { Error = "Invalid PhysPos"; json_object_put( Root ); return false; }
      Entry.PhysPos = json_object_get_string( Position );
     }
    if( FindChannelId( Result, Entry.Id ) != InvalidChannelId )
     {
      Error = "Duplicate channel ID: " + Entry.Id;
      json_object_put( Root );
      return false;
     }
    if( !GetOptionalInt( Row, "InputPin", Entry.InputPin ) ||
        !GetOptionalInt( Row, "OutputPin", Entry.OutputPin ) ||
        ( Entry.InputPin && *Entry.InputPin < 0 ) || ( Entry.OutputPin && *Entry.OutputPin < 0 ) ||
        !GetOptionalBool( Row, "InputActiveHigh", false, Entry.InputActiveHigh ) ||
        !GetOptionalBool( Row, "OutputActiveHigh", true, Entry.OutputActiveHigh ) ||
        !GetOptionalBool( Row, "InputPullup", true, Entry.InputPullup ) )
     {
      Error = "Invalid channel fields for " + Entry.Id;
      json_object_put( Root );
      return false;
     }
    if( !ParseChannelRole( Row, Entry, Error ) )
     {
      json_object_put( Root );
      return false;
     }
    if( Entry.Role != TChannelRole::Timer && !Entry.HasOutput() )
     {
      Error = "Heater channel has no output: " + Entry.Id;
      json_object_put( Root );
      return false;
     }
    if( Entry.Role != TChannelRole::Timer &&
        TGnHeaterControl::FindProfile( NewHeaterProfiles, Entry.Profile ) == nullptr )
     {
      Error = "Unknown heater profile for " + Entry.Id + ": " + Entry.Profile;
      json_object_put( Root );
      return false;
     }
    Result.push_back( std::move( Entry ) );
   }

  json_object_put( Root );
  Channels = std::move( Result );
  HeaterProfiles = std::move( NewHeaterProfiles );
  return true;
 }

bool TConfigStore::LoadChannels( const TGNString & FileName, TChannelConfigs & Channels,
                                 THeaterProfiles & HeaterProfiles, TGNString & Error ) const
 {
  Error.clear();
  const TGNString Json = ReadAll( FileName, Error );
  return Error.empty() && ChannelsFromJson( Json, Channels, HeaterProfiles, Error );
 }

bool TConfigStore::SaveChannels( const TGNString & FileName, const TGNString & Json, TGNString & Error ) const
 {
  Error.clear();
  std::ofstream File( FileName, std::ios::binary | std::ios::trunc );
  if( !File )
   {
    Error = "Cannot open " + FileName + " for writing";
    return false;
   }
  File.write( Json.data(), static_cast<std::streamsize>( Json.size() ) );
  if( !File )
   {
    Error = "Cannot write " + FileName;
    return false;
   }
  return true;
 }

bool TConfigStore::SwitchTimesFromJson( const TGNString & json, const TChannelConfigs & channels,
                                        TSwitchTimes & switchTimes, TGNString & error ) const
 {
  error.clear();
  json_object * root = json_tokener_parse( json.c_str() );
  if( !root || !json_object_is_type( root, json_type_array ) )
   {
    error = "Invalid SwitchTimes array";
    if( root )
      json_object_put( root );
    return false;
   }

  TSwitchTimes result;
  for( size_t index = 0;  index < json_object_array_length( root );  ++index )
   {
    json_object * row = json_object_array_get_idx( root, index );
    if( !json_object_is_type( row, json_type_array ) || ( json_object_array_length( row ) != 4 && json_object_array_length( row ) != 5 ) )
     {
      error = "Invalid SwitchTimes row " + std::to_string( index + 1 );
      json_object_put( root );
      return false;
     }

    const TGNString timeValue = json_object_get_string( json_object_array_get_idx( row, 0 ) );
    const int wdayMask = json_object_get_int( json_object_array_get_idx( row, 1 ) );
    const TGNString channelName = json_object_get_string( json_object_array_get_idx( row, 2 ) );
    const TChannelId channel = FindChannelId( channels, channelName );
    uint16_t minuteOfDay = 0;
    if( !ParseTime( timeValue, false, minuteOfDay ) || wdayMask < 0 || wdayMask > UINT8_MAX )
     {
      error = "Invalid switch time in row " + std::to_string( index + 1 ) + ": " + timeValue;
      json_object_put( root );
      return false;
     }
    if( channel == InvalidChannelId )
     {
      error = "Unknown channel in switch time row " + std::to_string( index + 1 ) + ": " + channelName;
      json_object_put( root );
      return false;
     }

    TSwitchTime entry{};
    entry.MinuteOfDay = minuteOfDay;
    entry.On = json_object_get_boolean( json_object_array_get_idx( row, 3 ) );
    entry.wday.mask = static_cast<uint8_t>( wdayMask );
    entry.Channel = channel;
    result.push_back( entry );
   }

  json_object_put( root );
  if( HasActiveTimeConflict( result ) )
   {
    error = ErrorDuplicateActiveTime;
    return false;
   }
  switchTimes = std::move( result );
  return true;
 }

bool TConfigStore::IntervalsFromJson( const TGNString & json, const TChannelConfigs & channels,
                                      TSwitchIntervals & intervals, TGNString & error ) const
 {
  error.clear();
  json_object * root = json_tokener_parse( json.c_str() );
  if( !root || !json_object_is_type( root, json_type_array ) )
   {
    error = "Invalid Intervals array";
    if( root )
      json_object_put( root );
    return false;
   }

  TSwitchIntervals result;
  for( size_t index = 0;  index < json_object_array_length( root );  ++index )
   {
    json_object * row = json_object_array_get_idx( root, index );
    if( !json_object_is_type( row, json_type_array ) || ( json_object_array_length( row ) != 5 && json_object_array_length( row ) != 6 ) )
     {
      error = "Invalid interval row " + std::to_string( index + 1 );
      json_object_put( root );
      return false;
     }

    const TGNString startTime = json_object_get_string( json_object_array_get_idx( row, 0 ) );
    const TGNString period = json_object_get_string( json_object_array_get_idx( row, 1 ) );
    const TGNString onTime = json_object_get_string( json_object_array_get_idx( row, 2 ) );
    const int wdayMask = json_object_get_int( json_object_array_get_idx( row, 3 ) );
    const TGNString channelName = json_object_get_string( json_object_array_get_idx( row, 4 ) );
    const TChannelId channel = FindChannelId( channels, channelName );
    uint16_t startMinuteOfDay = 0;
    uint16_t periodMinutes = 0;
    uint16_t onMinutes = 0;
    if( !ParseTime( startTime, false, startMinuteOfDay ) ||
        !ParseTime( period, true, periodMinutes ) || periodMinutes == 0 ||
        !ParseTime( onTime, true, onMinutes ) || onMinutes == 0 || onMinutes >= periodMinutes ||
        wdayMask < 0 || wdayMask > UINT8_MAX )
     {
      error = "Invalid interval in row " + std::to_string( index + 1 );
      json_object_put( root );
      return false;
     }
    if( channel == InvalidChannelId )
     {
      error = "Unknown channel in interval row " + std::to_string( index + 1 ) + ": " + channelName;
      json_object_put( root );
      return false;
     }

    TSwitchInterval entry{};
    entry.StartMinuteOfDay = startMinuteOfDay;
    entry.PeriodMinutes = periodMinutes;
    entry.OnMinutes = onMinutes;
    entry.wday.mask = static_cast<uint8_t>( wdayMask );
    entry.Channel = channel;
    result.push_back( entry );
   }

  json_object_put( root );
  if( HasActiveTimeConflict( result ) )
   {
    error = ErrorDuplicateActiveTime;
    return false;
   }
  intervals = std::move( result );
  return true;
 }

bool TConfigStore::LoadSchedule( const TGNString & FileName, const TChannelConfigs & Channels,
                                 TSwitchTimes & SwitchTimes, TSwitchIntervals & Intervals,
                                 std::vector<TGNString> & Warnings, TGNString & Error ) const
 {
  Error.clear();
  Warnings.clear();
  const TGNString Text = ReadAll( FileName, Error );
  if( !Error.empty() )
    return false;

  json_object * Root = json_tokener_parse( Text.c_str() );
  json_object * SwitchArray = nullptr;
  json_object * IntervalArray = nullptr;
  if( !Root || !json_object_is_type( Root, json_type_object ) ||
      !json_object_object_get_ex( Root, "SwitchTimes", &SwitchArray ) ||
      !json_object_is_type( SwitchArray, json_type_array ) ||
      !json_object_object_get_ex( Root, "Intervals", &IntervalArray ) ||
      !json_object_is_type( IntervalArray, json_type_array ) )
   {
    Error = "Invalid SwitchTimes JSON";
    if( Root )
      json_object_put( Root );
    return false;
   }

  TSwitchTimes NewSwitchTimes;
  for( size_t index = 0;  index < json_object_array_length( SwitchArray );  ++index )
   {
    json_object * row = json_object_array_get_idx( SwitchArray, index );
    if( !json_object_is_type( row, json_type_array ) || ( json_object_array_length( row ) != 4 && json_object_array_length( row ) != 5 ) )
     {
      AddInvalidRowWarning( Warnings, index + 1 );
      continue;
     }

    json_object * timeObject = json_object_array_get_idx( row, 0 );
    json_object * wdayObject = json_object_array_get_idx( row, 1 );
    json_object * channelObject = json_object_array_get_idx( row, 2 );
    json_object * onObject = json_object_array_get_idx( row, 3 );
    if( !json_object_is_type( timeObject, json_type_string ) || !json_object_is_type( wdayObject, json_type_int ) ||
        !json_object_is_type( channelObject, json_type_string ) || !json_object_is_type( onObject, json_type_boolean ) )
     {
      AddInvalidRowWarning( Warnings, index + 1 );
      continue;
     }

    const TGNString timeValue = json_object_get_string( timeObject );
    const int wdayMask = json_object_get_int( wdayObject );
    const TGNString channelName = json_object_get_string( channelObject );
    const TChannelId channel = FindChannelId( Channels, channelName );
    if( channel == InvalidChannelId )
     {
      AddUnknownChannelWarning( Warnings, channelName );
      continue;
     }

    uint16_t minuteOfDay = 0;
    if( !ParseTime( timeValue, false, minuteOfDay ) || wdayMask < 0 || wdayMask > UINT8_MAX )
     {
      AddInvalidRowWarning( Warnings, index + 1 );
      continue;
     }

    TSwitchTime entry{};
    entry.MinuteOfDay = minuteOfDay;
    entry.On = json_object_get_boolean( onObject );
    entry.wday.mask = static_cast<uint8_t>( wdayMask );
    entry.Channel = channel;
    if( HasActiveTimeConflict( NewSwitchTimes, entry ) )
     {
      AddInvalidRowWarning( Warnings, index + 1 );
      continue;
     }
    NewSwitchTimes.push_back( entry );
   }

  TSwitchIntervals NewIntervals;
  for( size_t index = 0;  index < json_object_array_length( IntervalArray );  ++index )
   {
    json_object * row = json_object_array_get_idx( IntervalArray, index );
    if( !json_object_is_type( row, json_type_array ) || ( json_object_array_length( row ) != 5 && json_object_array_length( row ) != 6 ) )
     {
      AddInvalidRowWarning( Warnings, index + 1 );
      continue;
     }

    json_object * startObject = json_object_array_get_idx( row, 0 );
    json_object * periodObject = json_object_array_get_idx( row, 1 );
    json_object * onObject = json_object_array_get_idx( row, 2 );
    json_object * wdayObject = json_object_array_get_idx( row, 3 );
    json_object * channelObject = json_object_array_get_idx( row, 4 );
    if( !json_object_is_type( startObject, json_type_string ) || !json_object_is_type( periodObject, json_type_string ) ||
        !json_object_is_type( onObject, json_type_string ) || !json_object_is_type( wdayObject, json_type_int ) ||
        !json_object_is_type( channelObject, json_type_string ) )
     {
      AddInvalidRowWarning( Warnings, index + 1 );
      continue;
     }

    const TGNString startTime = json_object_get_string( startObject );
    const TGNString period = json_object_get_string( periodObject );
    const TGNString onTime = json_object_get_string( onObject );
    const int wdayMask = json_object_get_int( wdayObject );
    const TGNString channelName = json_object_get_string( channelObject );
    const TChannelId channel = FindChannelId( Channels, channelName );
    if( channel == InvalidChannelId )
     {
      AddUnknownChannelWarning( Warnings, channelName );
      continue;
     }

    uint16_t startMinuteOfDay = 0;
    uint16_t periodMinutes = 0;
    uint16_t onMinutes = 0;
    if( !ParseTime( startTime, false, startMinuteOfDay ) ||
        !ParseTime( period, true, periodMinutes ) || periodMinutes == 0 ||
        !ParseTime( onTime, true, onMinutes ) || onMinutes == 0 || onMinutes >= periodMinutes ||
        wdayMask < 0 || wdayMask > UINT8_MAX )
     {
      AddInvalidRowWarning( Warnings, index + 1 );
      continue;
     }

    TSwitchInterval entry{};
    entry.StartMinuteOfDay = startMinuteOfDay;
    entry.PeriodMinutes = periodMinutes;
    entry.OnMinutes = onMinutes;
    entry.wday.mask = static_cast<uint8_t>( wdayMask );
    entry.Channel = channel;
    if( HasActiveTimeConflict( NewIntervals, entry ) )
     {
      AddInvalidRowWarning( Warnings, index + 1 );
      continue;
     }
    NewIntervals.push_back( entry );
   }

  json_object_put( Root );
  SwitchTimes = std::move( NewSwitchTimes );
  Intervals = std::move( NewIntervals );
  return true;
 }

TGNString TConfigStore::SwitchTimesToJson( const TChannelConfigs & channels,
                                               const TSwitchTimes & switchTimes ) const
 {
  json_object * root = json_object_new_array();
  for( const TSwitchTime & entry : switchTimes )
   {
    char timeValue[ 6 ];
    FormatTime( entry.MinuteOfDay, timeValue );
    json_object * row = json_object_new_array();
    json_object_array_add( row, json_object_new_string( timeValue ) );
    json_object_array_add( row, json_object_new_int( entry.wday.mask ) );
    json_object_array_add( row, json_object_new_string( GetChannelConfigId( channels, entry.Channel ).c_str() ) );
    json_object_array_add( row, json_object_new_boolean( entry.On != 0 ) );
    json_object_array_add( root, row );
   }
  const TGNString Result = FormatJsonRows( root, 0 );
  json_object_put( root );
  return Result;
 }

TGNString TConfigStore::IntervalsToJson( const TChannelConfigs & channels,
                                             const TSwitchIntervals & intervals ) const
 {
  json_object * root = json_object_new_array();
  for( const TSwitchInterval & entry : intervals )
   {
    char startTime[ 6 ];
    char period[ 6 ];
    char onTime[ 6 ];
    FormatTime( entry.StartMinuteOfDay, startTime );
    FormatTime( entry.PeriodMinutes, period );
    FormatTime( entry.OnMinutes, onTime );
    json_object * row = json_object_new_array();
    json_object_array_add( row, json_object_new_string( startTime ) );
    json_object_array_add( row, json_object_new_string( period ) );
    json_object_array_add( row, json_object_new_string( onTime ) );
    json_object_array_add( row, json_object_new_int( entry.wday.mask ) );
    json_object_array_add( row, json_object_new_string( GetChannelConfigId( channels, entry.Channel ).c_str() ) );
    json_object_array_add( root, row );
   }
  const TGNString Result = FormatJsonRows( root, 0 );
  json_object_put( root );
  return Result;
 }

TGNString TConfigStore::ScheduleToJson( const TChannelConfigs & Channels, const TSwitchTimes & SwitchTimes,
                                        const TSwitchIntervals & Intervals ) const
 {
  json_object * Root = json_object_new_object();
  json_object * SwitchRoot = json_tokener_parse( SwitchTimesToJson( Channels, SwitchTimes ).c_str() );
  json_object * IntervalRoot = json_tokener_parse( IntervalsToJson( Channels, Intervals ).c_str() );
  json_object_object_add( Root, "SwitchTimes", SwitchRoot );
  json_object_object_add( Root, "Intervals", IntervalRoot );
  const TGNString Result = FormatScheduleJson( SwitchRoot, IntervalRoot );
  json_object_put( Root );
  return Result;
 }

bool TConfigStore::CheckScheduleHeapSpace( const TChannelConfigs & Channels,
                                             const TSwitchTimes & SwitchTimes,
                                             const TSwitchIntervals & Intervals,
                                             TGNString & Error ) const
 {
  (void)Channels;
  (void)SwitchTimes;
  (void)Intervals;
  Error.clear();
  return true;
 }

bool TConfigStore::SaveSchedule( const TGNString & FileName, const TChannelConfigs & Channels,
                                 const TSwitchTimes & SwitchTimes, const TSwitchIntervals & Intervals,
                                 TGNString & Error ) const
 {
  Error.clear();
  std::ofstream File( FileName, std::ios::trunc );
  if ( !File )
   {
    Error = "Cannot write " + FileName;
    return false;
   }
  File << ScheduleToJson( Channels, SwitchTimes, Intervals ) << '\n';
  return true;
 }

bool TConfigStore::LoadScheduleJson( const TGNString & FileName, TGNString & SwitchTimesJson,
                                     TGNString & IntervalsJson, TGNString & Error ) const
 {
  Error.clear();
  const TGNString Text = ReadAll( FileName, Error );
  if( !Error.empty() ) return false;
  json_object * Root = json_tokener_parse( Text.c_str() );
  json_object * SwitchTimes = nullptr;
  json_object * Intervals = nullptr;
  if( Root == nullptr || !json_object_is_type( Root, json_type_object ) ||
      !json_object_object_get_ex( Root, "SwitchTimes", &SwitchTimes ) || !json_object_is_type( SwitchTimes, json_type_array ) ||
      !json_object_object_get_ex( Root, "Intervals", &Intervals ) || !json_object_is_type( Intervals, json_type_array ) )
   {
    if( Root != nullptr ) json_object_put( Root );
    Error = "Invalid SwitchTimes JSON";
    return false;
   }
  SwitchTimesJson = json_object_to_json_string_ext( SwitchTimes, JSON_C_TO_STRING_PRETTY );
  IntervalsJson = json_object_to_json_string_ext( Intervals, JSON_C_TO_STRING_PRETTY );
  json_object_put( Root );
  return true;
 }

bool TConfigStore::SaveScheduleJson( const TGNString & FileName, const TGNString & SwitchTimesJson,
                                     const TGNString & IntervalsJson, TGNString & Error ) const
 {
  Error.clear();
  json_object * SwitchTimes = json_tokener_parse( SwitchTimesJson.c_str() );
  json_object * Intervals = json_tokener_parse( IntervalsJson.c_str() );
  if( SwitchTimes == nullptr || Intervals == nullptr || !json_object_is_type( SwitchTimes, json_type_array ) ||
      !json_object_is_type( Intervals, json_type_array ) )
   {
    if( SwitchTimes != nullptr ) json_object_put( SwitchTimes );
    if( Intervals != nullptr ) json_object_put( Intervals );
    Error = "Invalid schedule arrays";
    return false;
   }
  const TGNString Text = FormatScheduleJson( SwitchTimes, Intervals );
  json_object_put( SwitchTimes );
  json_object_put( Intervals );
  if( Text.empty() )
   {
    Error = "Invalid schedule rows";
    return false;
   }
  std::ofstream File( FileName, std::ios::binary | std::ios::trunc );
  if( !File ) { Error = "Cannot open " + FileName + " for writing"; return false; }
  File << Text << '\n';
  if( !File ) { Error = "Cannot write " + FileName; return false; }
  return true;
 }
#endif
