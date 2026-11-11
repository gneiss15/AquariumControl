// ConfigurationTransaction.h, Version: 1.04
#pragma once
#include <GnBasics.h>
#if !defined( SYSTEM_ESP )
#include <filesystem>
#include <GnConfigurationJournal.h>
#include <fstream>
#include <sstream>
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
  std::ifstream Input(Journal);
  std::ostringstream Buffer;Buffer<<Input.rdbuf();
  TJsonConfigurationFiles Transaction;
  if(!Input || !GnReadConfigurationJournal(Buffer.str(),false,Transaction))
   {Error="Invalid configuration transaction";return false;}
  if(!Write(Directory/"Channels.json",Transaction.Chs.Value,Error) ||
     !Write(Directory/"SwitchTimes.json",Transaction.Schedule.Value,Error))
    return false; // Keep the journal for retry/restart recovery.
  bool Ok;
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
  TJsonConfigurationFiles Transaction;
  Transaction.Chs=Channels;
  Transaction.Schedule=Schedule;
  const TGNString Text=GnWriteJson(Transaction);
  if(Text.empty()) {Error="Invalid configuration transaction";return false;}
  const bool Saved=Write(Directory/"Configuration.transaction.json",Text,Error);
  return Saved && Recover( Directory, Error );
 }
 }
#endif
