// TimeSync.cpp, Version: 2.37
#include <GnBasics.h>
#include "BuildConfig.h"
#include "TimeSync.h"
#include <ctime>

#if defined( SYSTEM_ESP )
  #include "EspConfig.h"
  #include <Arduino.h>
  #include <time.h>
#else
  #include <chrono>
  #include <cstdio>
  #include <thread>
#endif

#if defined( SYSTEM_ESP )
bool TTimeSync::TimeLooksValid(void)
 {
  const std::time_t now = std::time( nullptr );
  std::tm local{};
  localtime_r( &now, &local );
  return local.tm_year + 1900 >= 2024;
 }
#else
TTimeSyncState TTimeSync::NativeState(void)
 {
  FILE * pipe = popen( "timedatectl show -p NTPSynchronized --value 2>/dev/null", "r" );
  if( pipe == nullptr )
    return TTimeSyncState::QueryUnavailable;

  char buffer[ 32 ]{};
  const bool hasOutput = fgets( buffer, sizeof( buffer ), pipe ) != nullptr;
  const int status = pclose( pipe );
  if( !hasOutput || status != 0 )
    return TTimeSyncState::QueryUnavailable;
  return TGNString( buffer ).find( "yes" ) == 0 ?
         TTimeSyncState::Synchronized : TTimeSyncState::NotSynchronized;
 }
#endif

TTimeSyncState TTimeSync::State(void)
 {
  #if defined( SYSTEM_ESP )
    return TimeLooksValid() ? TTimeSyncState::Synchronized : TTimeSyncState::NotSynchronized;
  #else
    return NativeState();
  #endif
 }

char const * TTimeSync::Error( TTimeSyncState state )
 {
  switch( state )
   {
    case TTimeSyncState::Synchronized:
      return "";
    case TTimeSyncState::NotSynchronized:
      return "System time is not synchronized";
    case TTimeSyncState::QueryUnavailable:
      return "Cannot query time synchronization with timedatectl";
   }
  return "Unknown time synchronization state";
 }

bool TTimeSync::Wait( TTimeSyncState & state )
 {
  #if defined( SYSTEM_ESP )
    configTzTime( EspConfig.TimeZone.c_str(), EspConfig.NtpServerAdr.c_str() );
    while( ( state = State() ) != TTimeSyncState::Synchronized )
      delay( 500 );
    return true;
  #else
    while( true )
     {
      state = State();
      if( state == TTimeSyncState::Synchronized )
        return true;
      if( state == TTimeSyncState::QueryUnavailable )
        return false;
      std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
     }
  #endif
 }
