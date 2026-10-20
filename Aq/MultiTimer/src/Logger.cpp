// Logger.cpp, Version: 2.37
#include <GnBasics.h>
#include "BuildConfig.h"
#if !defined( SYSTEM_ESP )
#include "Logger.h"
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

void TLogger::Init( TGNString fileName )
 {
  std::lock_guard<std::mutex> lock( Mutex );
  FileName = std::move( fileName );
 }

void TLogger::Write( TLogLevel level, const TGNString & message )
 {
  std::lock_guard<std::mutex> lock( Mutex );
  const std::time_t now = std::time( nullptr );
  if( level == TLogLevel::Error )
   {
    LastErrorMessage = message;
    LastErrorTime = now;
   }

  std::ofstream file( FileName, std::ios::app );
  std::tm local{};
  localtime_r( &now, &local );
  file << std::put_time( &local, "%Y-%m-%d %H:%M:%S" ) << " [" << LevelText( level ) << "] " << message << '\n';
 }

bool TLogger::GetLastError( TGNString & message, std::time_t & time ) const
 {
  std::lock_guard<std::mutex> lock( Mutex );
  if( LastErrorTime == 0 )
    return false;

  message = LastErrorMessage;
  time = LastErrorTime;
  return true;
 }
#endif
