// TmrSwRestartLog.cpp, Version: 1.02
#include <GnBasics.h>
#include "TmrSwRestartLog.h"

#include <LittleFS.h>
#include <GnDeviceIdentity.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <atomic>
#include <cstdio>
#include <ctime>

namespace
 {
constexpr size_t MaxLogBytes = 4096;
constexpr char LogHeader[] = "Event;UtcUnixSeconds;BootCnt;ResetReason;RequestedBytes;FreeHeap;LargestBlock;Caps;Function\n";

struct TAllocationFailure
 {
  uint64_t Requested = 0;
  uint64_t Free = 0;
  uint64_t Largest = 0;
  uint64_t MonotonicUs = 0;
  uint32_t Caps = 0;
  char Function[48]{};
 };

TAllocationFailure AllocationFailure;
std::atomic<uint8_t> FailureState{0};
bool CallbackRegistered;
bool FileSystemMounted;
bool BootRecorded;
bool BootTimeRecorded;
bool RestartIssued;
uint64_t StartedAtUs;

void AllocationFailed( size_t Requested, uint32_t Caps, char const * Function )
 {
  uint8_t Expected = 0;
  if( !FailureState.compare_exchange_strong( Expected, 1, std::memory_order_acquire ) )return;

  AllocationFailure.Requested = Requested;
  AllocationFailure.Caps = Caps;
  AllocationFailure.Free = heap_caps_get_free_size( Caps );
  AllocationFailure.Largest = heap_caps_get_largest_free_block( Caps );
  AllocationFailure.MonotonicUs = esp_timer_get_time();
  if( Function )
   {
    size_t Index = 0;
    for( ; Index + 1 < sizeof(AllocationFailure.Function) && Function[Index]; ++Index )
      AllocationFailure.Function[Index] = Function[Index];
    AllocationFailure.Function[Index] = '\0';
   }
  FailureState.store( 2, std::memory_order_release );
 }

bool Append( char const * Event, uint64_t Time, esp_reset_reason_t ResetReason,
             TAllocationFailure const * Failure )
 {
  if( !FileSystemMounted )return false;

  char Line[192];
  const auto Boot = static_cast<unsigned long long>(GnDeviceIdentity.BootCounter());
  const int Length = Failure
    ? std::snprintf( Line, sizeof(Line), "%s;%llu;%llu;;%llu;%llu;%llu;%08lx;%s\n", Event,
                     static_cast<unsigned long long>(Time), Boot,
                     static_cast<unsigned long long>(Failure->Requested),
                     static_cast<unsigned long long>(Failure->Free),
                     static_cast<unsigned long long>(Failure->Largest),
                     static_cast<unsigned long>(Failure->Caps), Failure->Function )
    : std::snprintf( Line, sizeof(Line), "%s;%llu;%llu;%u;;;;;\n", Event,
                     static_cast<unsigned long long>(Time), Boot,
                     static_cast<unsigned>(ResetReason) );
  if( Length <= 0 || static_cast<size_t>(Length) >= sizeof(Line) )return false;

  File Journal = LittleFS.open( TmrSwRestartLogPath, "a" );
  if( !Journal )return false;
  if( Journal.size() + static_cast<size_t>(Length) > MaxLogBytes )
   {
    Journal.close();
    LittleFS.remove( TmrSwRestartLogPreviousPath );
    if( !LittleFS.rename( TmrSwRestartLogPath, TmrSwRestartLogPreviousPath ) )return false;
    Journal = LittleFS.open( TmrSwRestartLogPath, "a" );
    if( !Journal )return false;
   }
  if( !Journal.size() && Journal.write( reinterpret_cast<uint8_t const *>(LogHeader), sizeof(LogHeader)-1 ) != sizeof(LogHeader)-1 )
    return false;
  return Journal.write( reinterpret_cast<uint8_t const *>(Line), Length ) == static_cast<size_t>(Length);
 }
}

bool TmrSwRestartLogSetup( bool Mounted )
 {
  FileSystemMounted = Mounted;
  if( !CallbackRegistered )
   {
    if( heap_caps_register_failed_alloc_callback( AllocationFailed ) != ESP_OK )return false;
    CallbackRegistered = true;
    StartedAtUs = esp_timer_get_time();
   }
  if( Mounted )LittleFS.mkdir( "/diagnostics" );
  return true;
 }

void TmrSwRestartLogFileSystemMounted( bool Mounted )
 {
  FileSystemMounted = Mounted;
  if( Mounted )LittleFS.mkdir( "/diagnostics" );
 }

bool TmrSwRestartLogLoop( bool TimeValid )
 {
  if( RestartIssued )return true;
  const uint64_t NowUs = esp_timer_get_time();
  const uint64_t Now = TimeValid ? static_cast<uint64_t>(std::time(nullptr)) : 0;
  const auto ResetReason = esp_reset_reason();

  if( FailureState.load( std::memory_order_acquire ) == 2 )
   {
    RestartIssued = true;
    const uint64_t FailureTime = Now && NowUs >= AllocationFailure.MonotonicUs
      ? Now - (NowUs - AllocationFailure.MonotonicUs) / 1000000 : 0;
    Append( "oom", FailureTime, ResetReason, &AllocationFailure );
    esp_restart();
    return true;
   }
  if( !FileSystemMounted )return false;

  if( !BootRecorded )
   {
    BootRecorded = true;
    Append( "boot", 0, ResetReason, nullptr );
   }
  if( Now && !BootTimeRecorded )
   {
    BootTimeRecorded = true;
    const uint64_t Uptime = NowUs >= StartedAtUs ? (NowUs - StartedAtUs) / 1000000 : 0;
    Append( "boot-time", Now >= Uptime ? Now - Uptime : 0, ResetReason, nullptr );
   }
  return false;
 }
