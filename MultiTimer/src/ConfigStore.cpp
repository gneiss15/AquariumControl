// ConfigStore.cpp, Version: 2.60
#define Do_TJsonChannels
#define Do_TJsonHeatConfig
#define Do_TJsonInterval
#define Do_TJsonIntervals
#define Do_TJsonProfile
#define Do_TJsonSchedule
#define Do_TJsonSwitchTime
#define Do_TJsonSwitchTimes
#include <GnBasics.h>
#include <GnChargeProfileJson.h>
#include <GnHeatPiProfileJson.h>
#include <GnProfileModel.h>
#include "BuildConfig.h"
#if !defined( SYSTEM_ESP )
#include "ConfigStore.h"
#include <GnUdpStatus.h>
#include <GnDeviceIdentity.h>
#include <AddressResolver.h>
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

bool ParseTime( const TGNString & Value, bool Duration, uint16_t & Minutes )
 {
  if(!(Duration?GnDuration(Value):GnTimeOfDay(Value)))return false;
  Minutes=uint16_t(((Value[0]-'0')*10+Value[1]-'0')*60+(Value[3]-'0')*10+Value[4]-'0');
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
  bool ObjectRows = RowCount > 0;
  for( size_t RowIndex = 0;  RowIndex < RowCount;  ++RowIndex )
    ObjectRows = ObjectRows && json_object_is_type( json_object_array_get_idx( Array, RowIndex ), json_type_object );
  if( ObjectRows || RowCount == 0 )
   {
    const char * const Text = json_object_to_json_string_ext( Array, JSON_C_TO_STRING_PRETTY );
    if( Text == nullptr )
      return {};
    const size_t Length = strlen( Text );
    TGNString Result;
    Result.reserve( Length + Indent * RowCount );
    for( size_t Index = 0;  Index < Length;  ++Index )
     {
      Result += Text[ Index ];
      if( Text[ Index ] == '\n' && Index + 1 < Length )
        Result.append( Indent, ' ' );
     }
    return Result;
   }

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

TChannelIndex TConfigStore::FindChannelId(const TChannelConfigs & Channels,const TGNString & Id)
 {
  std::vector<TPhysicalEndpoint> Endpoints;
  const auto Device=GnDeviceIdentity.WireId();
  for(auto const & Channel:Channels)
    if(Channel.PhysicalNumber())Endpoints.push_back({Device+"/"+std::to_string(Channel.PhysicalNumber()),Channel.HasOutput()?1u:0u});
  TGNString Resolved;
  if(!ResolveChannelAddress(Id,Endpoints,1,Resolved))return InvalidChannelId;
  TChannelAddress Address;
  if(!TChannelAddress::Parse(Resolved,Address))return InvalidChannelId;
  for(size_t Index=0;Index<Channels.size();++Index)
    if(Channels[Index].PhysicalNumber()==Address.Channel)return TChannelIndex(Index);
  return InvalidChannelId;
 }

TGNString TConfigStore::GetChannelConfigId(const TChannelConfigs & Channels,TChannelIndex Id)
 {
  if(Id>=Channels.size() || !Channels[Id].PhysicalNumber())return {};
  return GnDeviceIdentity.WireId()+"/"+std::to_string(Channels[Id].PhysicalNumber());
 }

bool TConfigStore::ChannelsFromJson( const TGNString & Json, TChannelConfigs & Channels,
                                      THeaterProfiles & HeaterProfiles, TGNString & Error ) const
 {
  Error.clear();
  TJsonChannels Document;
  if( !GnReadJsonValue( Json, Document ) ) { Error="Invalid Channels JSON";return false; }
  THeaterProfiles NewProfiles;
  if( Document.Profs.Present )
    for( auto const & Row : Document.Profs.Value )
     {
      THeaterProfile Profile;
      if(!GnApplyHeatProfile(Row,Profile)){Error="Invalid profile values";return false;}
      if(TGnHeaterControl::FindProfile(NewProfiles,Profile.Id))
       {Error="Duplicate heater profile ID: "+Profile.Id;return false;}
      NewProfiles.push_back(std::move(Profile));
     }
  TChannelConfigs Result;
  for( auto const & Row : Document.Chs )
   {
    TChannelConfig Entry;
    Entry.Id=Row.Id;
    Entry.PhysPos=Row.PhysPos.Value;
    if(FindChannelId(Result,Entry.Id)!=InvalidChannelId)
     {Error="Duplicate channel ID: "+Entry.Id;return false;}
    if(Row.InPin.Present)Entry.InputPin=int(Row.InPin.Value);
    if(Row.OutPin.Present)Entry.OutputPin=int(Row.OutPin.Value);
    Entry.InputActiveHigh=Row.InActiveHigh.Present?Row.InActiveHigh.Value:false;
    Entry.OutputActiveHigh=Row.OutActiveHigh.Present?Row.OutActiveHigh.Value:true;
    Entry.InputPullup=Row.InPullup.Present?Row.InPullup.Value:true;
    TGnHeaterControl::ParseRole(Row.Role,Entry.Role);
    if(Row.Prof.Present)Entry.Profile=Row.Prof.Value;
    if(Entry.Role==TChannelRole::Timer && !Entry.Profile.empty())
     {Error="Profile requires heater Role for channel "+Entry.Id;return false;}
    if(Entry.Role!=TChannelRole::Timer && (!Entry.HasOutput() || Entry.Profile.empty() ||
       !TGnHeaterControl::FindProfile(NewProfiles,Entry.Profile)))
     {Error="Missing output or profile for heater channel "+Entry.Id;return false;}
    Result.push_back(std::move(Entry));
   }
  Channels=std::move(Result);
  HeaterProfiles=std::move(NewProfiles);
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

bool TConfigStore::SwitchTimesFromJson( const TGNString & Json, const TChannelConfigs & Channels,
                                        TSwitchTimes & SwitchTimes, TGNString & Error ) const
 {
  Error.clear();
  TJsonSwitchTimes Document;
  if(!GnReadJsonValue(Json,Document)) {Error="Invalid SwitchTimes array";return false;}
  TSwitchTimes Result;
  for(auto const & Row:Document.Value)
   {
    if(!Row.Time.Known() || !Row.Days.Known() || !Row.State.Known() || !Row.Ch.Known())
     {Error="Missing switch time value";return false;}
    const auto Channel=FindChannelId(Channels,Row.Ch);
    if(Channel==InvalidChannelId) {Error="Unknown channel in switch time: "+Row.Ch.Value;return false;}
    TSwitchTime Entry{};
    uint16_t Minutes;
    ParseTime(Row.Time,false,Minutes);
    Entry.MinuteOfDay=Minutes;
    Entry.On=Row.State;
    Entry.wday.mask=Row.Days;
    Entry.Channel=Channel;
    Result.push_back(Entry);
   }
  if(HasActiveTimeConflict(Result)) {Error=ErrorDuplicateActiveTime;return false;}
  SwitchTimes=std::move(Result);
  return true;
 }

bool TConfigStore::IntervalsFromJson( const TGNString & Json, const TChannelConfigs & Channels,
                                      TSwitchIntervals & Intervals, TGNString & Error ) const
 {
  Error.clear();
  TJsonIntervals Document;
  if(!GnReadJsonValue(Json,Document)) {Error="Invalid Intervals array";return false;}
  TSwitchIntervals Result;
  for(auto const & Row:Document.Value)
   {
    if(!Row.Start.Known() || !Row.Period.Known() || !Row.OnDuration.Known() ||
       !Row.Days.Known() || !Row.Ch.Known())
     {Error="Missing interval value";return false;}
    const auto Channel=FindChannelId(Channels,Row.Ch);
    if(Channel==InvalidChannelId) {Error="Unknown channel in interval: "+Row.Ch.Value;return false;}
    TSwitchInterval Entry{};
    ParseTime(Row.Start,false,Entry.StartMinuteOfDay);
    ParseTime(Row.Period,true,Entry.PeriodMinutes);
    ParseTime(Row.OnDuration,true,Entry.OnMinutes);
    Entry.wday.mask=Row.Days;
    Entry.Channel=Channel;
    Result.push_back(Entry);
   }
  if(HasActiveTimeConflict(Result)) {Error=ErrorDuplicateActiveTime;return false;}
  Intervals=std::move(Result);
  return true;
 }

bool TConfigStore::LoadSchedule( const TGNString & FileName, const TChannelConfigs & Channels,
                                 TSwitchTimes & SwitchTimes, TSwitchIntervals & Intervals,
                                 std::vector<TGNString> & Warnings, TGNString & Error ) const
 {
  Error.clear();Warnings.clear();
  const TGNString Text=ReadAll(FileName,Error);
  if(!Error.empty())return false;
  TGnWireDocument Document(Text);
  const auto Schedule = Document.Root();
  if( !Document.Valid() || !GnTypeValid(Schedule,TGnJsonType<TJsonSchedule>{}) )
   {Error="Invalid SwitchTimes JSON";return false;}
  TGNString RowError;
  const auto Times = Schedule.Get("SwitchTimes");
  const auto IntervalsJson = Schedule.Get("Intervals");
  TSwitchTimes NewTimes;
  for(size_t Index=0;Index<(Times.Array() ? Times.Size() : 0);++Index)
   {
    TJsonSwitchTime Row;
    if(!GnRead(Times.At(Index),Row,RowError) ||
       !Row.Time.Known() || !Row.Days.Known() || !Row.State.Known() || !Row.Ch.Known())
     {AddInvalidRowWarning(Warnings,Index+1);continue;}
    const auto Channel=FindChannelId(Channels,Row.Ch);
    if(Channel==InvalidChannelId) {AddUnknownChannelWarning(Warnings,Row.Ch);continue;}
    TSwitchTime Entry{};
    uint16_t Minutes;
    ParseTime(Row.Time,false,Minutes);
    Entry.MinuteOfDay=Minutes;
    Entry.On=Row.State;Entry.wday.mask=Row.Days;Entry.Channel=Channel;
    if(HasActiveTimeConflict(NewTimes,Entry)) {AddInvalidRowWarning(Warnings,Index+1);continue;}
    NewTimes.push_back(Entry);
   }
  TSwitchIntervals NewIntervals;
  for(size_t Index=0;Index<(IntervalsJson.Array() ? IntervalsJson.Size() : 0);++Index)
   {
    TJsonInterval Row;
    if(!GnRead(IntervalsJson.At(Index),Row,RowError) ||
       !Row.Start.Known() || !Row.Period.Known() || !Row.OnDuration.Known() ||
       !Row.Days.Known() || !Row.Ch.Known())
     {AddInvalidRowWarning(Warnings,Index+1);continue;}
    const auto Channel=FindChannelId(Channels,Row.Ch);
    if(Channel==InvalidChannelId) {AddUnknownChannelWarning(Warnings,Row.Ch);continue;}
    TSwitchInterval Entry{};
    ParseTime(Row.Start,false,Entry.StartMinuteOfDay);
    ParseTime(Row.Period,true,Entry.PeriodMinutes);
    ParseTime(Row.OnDuration,true,Entry.OnMinutes);
    Entry.wday.mask=Row.Days;Entry.Channel=Channel;
    if(HasActiveTimeConflict(NewIntervals,Entry)) {AddInvalidRowWarning(Warnings,Index+1);continue;}
    NewIntervals.push_back(Entry);
   }
  SwitchTimes=std::move(NewTimes);Intervals=std::move(NewIntervals);
  return true;
 }

TGNString TConfigStore::SwitchTimesToJson( const TChannelConfigs & Channels, const TSwitchTimes & Values ) const
 {
  TJsonSwitchTimes Document;
  for(auto const & Entry:Values)
   {
    TJsonSwitchTime Row;
    char Time[6];FormatTime(Entry.MinuteOfDay,Time);
    Row.Time=Time;Row.Days=Entry.wday.mask;
    Row.Ch=GetChannelConfigId(Channels,Entry.Channel);
    Row.State=Entry.On!=0;
    Document.Value.push_back(std::move(Row));
   }
  TGnWireDocument Encoded(GnWriteJson(Document));
  return Encoded.Valid()?FormatJsonRows(Encoded.Root().V,0):TGNString{};
 }

TGNString TConfigStore::IntervalsToJson( const TChannelConfigs & Channels, const TSwitchIntervals & Values ) const
 {
  TJsonIntervals Document;
  for(auto const & Entry:Values)
   {
    TJsonInterval Row;
    char Time[6];FormatTime(Entry.StartMinuteOfDay,Time);Row.Start=Time;
    FormatTime(Entry.PeriodMinutes,Time);Row.Period=Time;
    FormatTime(Entry.OnMinutes,Time);Row.OnDuration=Time;
    Row.Days=Entry.wday.mask;
    Row.Ch=GetChannelConfigId(Channels,Entry.Channel);
    Document.Value.push_back(std::move(Row));
   }
  TGnWireDocument Encoded(GnWriteJson(Document));
  return Encoded.Valid()?FormatJsonRows(Encoded.Root().V,0):TGNString{};
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
