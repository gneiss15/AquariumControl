// ConfigurationTransaction.h, Version: 1.00
#pragma once
#include <GnBasics.h>
#if !defined( SYSTEM_ESP )
#include <filesystem>
#include <json-c/json.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <sys/stat.h>

// A durable redo journal makes a multi-file configuration recoverable before hardware setup.
namespace ConfigurationTransaction
 {
inline bool Write( std::filesystem::path const & Path, TGNString const & Text, TGNString & Error )
 {
  const auto Temporary = Path.string() + ".new";
  const int File = open( Temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600 );
  if( File < 0 ) { Error = "Cannot stage configuration"; return false; }
  size_t Offset = 0;
  while( Offset < Text.size() )
   {
    const auto Count = write( File, Text.data() + Offset, Text.size() - Offset );
    if( Count < 0 && errno == EINTR ) continue;
    if( Count <= 0 ) { close( File ); Error = "Cannot write configuration"; return false; }
    Offset += static_cast<size_t>( Count );
   }
  const bool Synced = fsync( File ) == 0;
  close( File );
  if( !Synced || rename( Temporary.c_str(), Path.c_str() ) != 0 )
    { Error = "Cannot commit configuration file"; return false; }
  const int Directory = open( Path.parent_path().c_str(), O_RDONLY | O_DIRECTORY );
  const bool Durable = Directory >= 0 && fsync( Directory ) == 0;
  if( Directory >= 0 ) close( Directory );
  if( !Durable ) Error = "Cannot sync configuration directory";
  return Durable;
 }
inline bool Recover( std::filesystem::path const & Directory, TGNString & Error )
 {
  const auto Journal = Directory / "Configuration.transaction.json";
  if( !std::filesystem::exists( Journal ) ) return true;
  json_object * Root = json_object_from_file( Journal.c_str() ), * Files = nullptr;
  if( !Root || !json_object_object_get_ex( Root, "Files", &Files ) || !json_object_is_type( Files, json_type_object ) )
   { if( Root ) json_object_put( Root ); Error = "Invalid configuration transaction"; return false; }
  json_object * ChannelFile = nullptr, * ScheduleFile = nullptr;
  bool Ok = json_object_object_length( Files ) == 2 &&
    json_object_object_get_ex( Files, "Channels.json", &ChannelFile ) &&
    json_object_object_get_ex( Files, "SwitchTimes.json", &ScheduleFile );
  if( !Ok ) Error = "Incomplete configuration transaction";
  json_object_object_foreach( Files, Name, Value )
   {
    const TGNString Key = Name;
    if( ( Key != "Channels.json" && Key != "SwitchTimes.json" ) || !json_object_is_type( Value, json_type_string ) )
      { Ok = false; Error = "Invalid transaction file"; break; }
   }
  if( Ok )
   {
    json_object_object_foreach( Files, Name, Value )
     { if( !Write( Directory / Name, json_object_get_string( Value ), Error ) ) { Ok = false; break; } }
   }
  json_object_put( Root );
  if( !Ok ) return false; // Keep the journal for deterministic retry/restart recovery.
  if( unlink( Journal.c_str() ) != 0 ) { Error = "Cannot retire configuration transaction"; return false; }
  const int File = open( Directory.c_str(), O_RDONLY | O_DIRECTORY );
  Ok = File >= 0 && fsync( File ) == 0;
  if( File >= 0 ) close( File );
  if( !Ok ) Error = "Cannot sync transaction completion";
  return Ok;
 }
inline bool Commit( std::filesystem::path const & Directory, TGNString const & Channels,
                    TGNString const & Schedule, TGNString & Error )
 {
  if( !Recover( Directory, Error ) ) return false;
  auto Root = json_object_new_object(), Files = json_object_new_object();
  json_object_object_add( Files, "Channels.json", json_object_new_string( Channels.c_str() ) );
  json_object_object_add( Files, "SwitchTimes.json", json_object_new_string( Schedule.c_str() ) );
  json_object_object_add( Root, "Files", Files );
  const TGNString Text = json_object_to_json_string_ext( Root, JSON_C_TO_STRING_PLAIN );
  const bool Saved = Write( Directory / "Configuration.transaction.json", Text, Error );
  json_object_put( Root );
  return Saved && Recover( Directory, Error );
 }
 }
#endif
