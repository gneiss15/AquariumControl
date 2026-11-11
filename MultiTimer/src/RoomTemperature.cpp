// RoomTemperature.cpp, Version: 1.00
#include <GnBasics.h>
#include "RoomTemperature.h"
#include <json-c/json.h>
#include <cmath>
#include <fstream>
#include <regex>
#include <sstream>

TRoomTemperature::TRoomTemperature( std::filesystem::path Root, std::chrono::milliseconds Interval )
: FRoot( std::move(Root) ), FInterval(Interval)
 {
 }

bool TRoomTemperature::Setup( std::filesystem::path const & ConfigFile, TGNString & Error )
 {
  json_object * Root = json_object_from_file(ConfigFile.c_str()), * Config = nullptr, * Field = nullptr;
  if( !Root || !json_object_is_type(Root,json_type_object) )
   {
    if( Root ) json_object_put(Root);
    Error = "Cannot read Config.json for room temperature";
    return false;
   }
  if( !json_object_object_get_ex(Root,"RoomTemperature",&Config) )
   { json_object_put(Root); return true; }
  const bool Good = json_object_is_type(Config,json_type_object) &&
    json_object_object_get_ex(Config,"Enabled",&Field) && json_object_is_type(Field,json_type_boolean);
  if( !Good )
   { json_object_put(Root); Error = "RoomTemperature.Enabled must be boolean"; return false; }
  const bool Enabled = json_object_get_boolean(Field);
  if( json_object_object_get_ex(Config,"SensorId",&Field) )
   {
    if( !json_object_is_type(Field,json_type_string) )
     { json_object_put(Root); Error = "RoomTemperature.SensorId must be a string"; return false; }
    FSensorId = json_object_get_string(Field);
    if( !FSensorId.empty() && !std::regex_match(FSensorId,std::regex("28-[0-9a-f]{12}")) )
     { json_object_put(Root); Error = "Invalid DS18B20 SensorId"; return false; }
   }
  json_object_put(Root);
  FEnabled = Enabled;
  FSample.Error = "Waiting for first room temperature measurement";
  FNextRead = std::chrono::steady_clock::now();
  Error.clear();
  return true;
 }

TRoomTemperature::TSample TRoomTemperature::Read( std::filesystem::path const & Root, TGNString const & SensorId )
 {
  TSample Sample;
  try
   {
    Sample.SensorId = SensorId;
    if( Sample.SensorId.empty() )
     {
      std::error_code Error;
      std::filesystem::directory_iterator It(Root,Error);
      if( Error ) { Sample.Error = "1-Wire bus unavailable"; return Sample; }
      for( auto const & Entry : It )
       {
        const auto Name = Entry.path().filename().string();
        if( !std::regex_match(Name,std::regex("28-[0-9a-f]{12}")) ) continue;
        if( !Sample.SensorId.empty() ) { Sample.SensorId.clear(); Sample.Error = "Multiple DS18B20 sensors: configure SensorId"; return Sample; }
        Sample.SensorId = Name;
       }
      if( Sample.SensorId.empty() ) { Sample.Error = "Room temperature sensor not found"; return Sample; }
     }
    if( !std::regex_match(Sample.SensorId,std::regex("28-[0-9a-f]{12}")) )
     { Sample.Error = "Invalid DS18B20 SensorId"; return Sample; }
    // Opening/reading w1_slave starts a kernel-managed conversion. This blocks
    // only the worker, never the scheduler, HTTP handling or UDP loop.
    std::ifstream Input(Root / Sample.SensorId / "w1_slave");
    TGNString First, Second;
    if( !std::getline(Input,First) || !std::getline(Input,Second) )
     { Sample.Error = "Cannot read room temperature sensor"; return Sample; }
    std::istringstream Words(First); TGNString Word, Last;
    while( Words >> Word ) Last = Word;
    if( Last != "YES" ) { Sample.Error = "Room temperature CRC check failed"; return Sample; }
    const auto Position = Second.find("t=");
    if( Position == TGNString::npos ) { Sample.Error = "Room temperature value missing"; return Sample; }
    std::istringstream Number(Second.substr(Position+2)); long MilliCelsius = 0;
    if( !(Number >> MilliCelsius) || !(Number >> std::ws).eof() || MilliCelsius < -55000 || MilliCelsius > 125000 )
     { Sample.Error = "Invalid room temperature value"; return Sample; }
    Sample.Celsius = float(MilliCelsius) / 1000.0F;
    Sample.Valid = true;
    Sample.UpdatedAt = std::time(nullptr);
   }
  catch( std::exception const & ) { Sample.Valid = false; Sample.Error = "Room temperature read failed"; }
  return Sample;
 }

void TRoomTemperature::Loop(void)
 {
  if( !FEnabled ) return;
  const auto Now = std::chrono::steady_clock::now();
  if( FRead.valid() )
   {
    if( FRead.wait_for(std::chrono::seconds(0)) != std::future_status::ready ) return;
    try { FSample = FRead.get(); }
    catch( std::exception const & ) { FSample.Valid = false; FSample.Error = "Room temperature worker failed"; }
    if( !FSample.SensorId.empty() ) FSensorId = FSample.SensorId;
    FCompletedAt = Now;
    ++FRevision;
   }
  if( Now < FNextRead ) return;
  try
   {
    FRead = std::async(std::launch::async,[Root=FRoot,Id=FSensorId]() { return Read(Root,Id); });
    FNextRead += FInterval;
    if( FNextRead <= Now ) FNextRead = Now + FInterval;
   }
  catch( std::exception const & )
   {
    FSample.Valid = false;
    FSample.Error = "Cannot start room temperature measurement";
    FNextRead = Now + FInterval;
   }
 }

TRoomTemperature::TSample TRoomTemperature::Sample(void) const
 {
  auto Result = FSample;
  if( Result.Valid && std::chrono::steady_clock::now() - FCompletedAt > std::chrono::seconds(20) )
   { Result.Valid = false; Result.Error = "Room temperature measurement stale"; }
  return Result;
 }

TGnUdpTemperature TRoomTemperature::UdpTemperature(void) const
 {
  const auto Current = Sample();
  return { FEnabled, Current.Valid, Current.Celsius, FRevision };
 }
