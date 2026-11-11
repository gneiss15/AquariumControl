// FeederRestartLog.cpp, Version: 1.02
#include <GnBasics.h>
#include "FeederRestartLog.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <GnDeviceIdentity.h>
#include <core_esp8266_features.h>
#include <user_interface.h>
#include <cstdio>
#include <ctime>

namespace
 {
constexpr size_t MaxLogBytes = 4096;
constexpr char LogHeader[] = "Event;UtcUnixSeconds;BootCnt;ResetReason;RequestedBytes;FreeHeap;LargestBlock\n";

struct TAllocationFailure
 {
  uint32_t Requested;
  uint32_t Free;
  uint32_t Largest;
 };
TAllocationFailure AllocationFailure;
volatile bool AllocationFailed;
bool FileSystemMounted;
bool BootRecorded;
bool BootTimeRecorded;
bool RestartIssued;

// The allocation hook only records metrics. Flash writes run in loop().
void IRAM_ATTR RecordAllocationFailure( size_t Requested )
 {
  if( !Requested || AllocationFailed )return;
  const uint32_t InterruptState = xt_rsil(15);
  if( !AllocationFailed )
   {
    AllocationFailure.Requested = Requested;
    ESP.getHeapStats( &AllocationFailure.Free, &AllocationFailure.Largest );
    AllocationFailed = true;
   }
  xt_wsr_ps(InterruptState);
 }

size_t IRAM_ATTR AllocationBytes( size_t Count, size_t Size )
 {
  return Count && Size > SIZE_MAX / Count ? SIZE_MAX : Count * Size;
 }

bool Append( char const * Event, uint64_t Time, unsigned ResetReason, TAllocationFailure const * Failure )
 {
  if( !FileSystemMounted )return false;

  char Line[128];
  const auto Epoch = static_cast<unsigned long long>(Time);
  const auto Boot = static_cast<unsigned long long>(GnDeviceIdentity.BootCounter());
  const int Length = Failure
    ? std::snprintf( Line, sizeof(Line), "%s;%llu;%llu;;%lu;%lu;%lu\n", Event, Epoch, Boot,
                     static_cast<unsigned long>(Failure->Requested), static_cast<unsigned long>(Failure->Free),
                     static_cast<unsigned long>(Failure->Largest) )
    : std::snprintf( Line, sizeof(Line), "%s;%llu;%llu;%u;;;\n", Event, Epoch, Boot, ResetReason );
  if( Length <= 0 || static_cast<size_t>(Length) >= sizeof(Line) )return false;

  File Journal = LittleFS.open(FeederRestartLogPath, "a");
  if( !Journal )return false;
  if( Journal.size() + static_cast<size_t>(Length) > MaxLogBytes )
   {
    Journal.close();
    if( !LittleFS.rename(FeederRestartLogPath, FeederRestartLogPreviousPath) )return false;
    Journal = LittleFS.open(FeederRestartLogPath, "a");
    if( !Journal )return false;
   }
  if( !Journal.size() && Journal.write(reinterpret_cast<uint8_t const *>(LogHeader), sizeof(LogHeader)-1) != sizeof(LogHeader)-1 )
    return false;
  const bool Written = Journal.write(reinterpret_cast<uint8_t const *>(Line), Length) == static_cast<size_t>(Length);
  Journal.close();
  return Written;
 }
 }

extern "C"
 {
void * __real_malloc( size_t Size );
void * __real_calloc( size_t Count, size_t Size );
void * __real_realloc( void * Pointer, size_t Size );

void * IRAM_ATTR __wrap_malloc( size_t Size )
 {
  void * Result = __real_malloc(Size);
  if( !Result )RecordAllocationFailure(Size);
  return Result;
 }

void * IRAM_ATTR __wrap_calloc( size_t Count, size_t Size )
 {
  void * Result = __real_calloc(Count, Size);
  if( !Result )RecordAllocationFailure(AllocationBytes(Count, Size));
  return Result;
 }

void * IRAM_ATTR __wrap_realloc( void * Pointer, size_t Size )
 {
  void * Result = __real_realloc(Pointer, Size);
  if( !Result )RecordAllocationFailure(Size);
  return Result;
 }

// Cover the SDK entry points as well as C allocation calls, including SDK aliases
// whose internal calls cannot be redirected by the linker's wrapping option.
#define SDK_ALLOC(Name, Parameters, Arguments, Bytes) \
  void * __real_##Name Parameters; \
  void * IRAM_ATTR __wrap_##Name Parameters \
   { \
    void * Result = __real_##Name Arguments; \
    if( !Result )RecordAllocationFailure(Bytes); \
    return Result; \
   }

SDK_ALLOC(pvPortMalloc, (size_t Size, char const * File, int Line), (Size, File, Line), Size)
SDK_ALLOC(sdk3_pvPortMalloc, (size_t Size, char const * File, int Line, bool Iram), (Size, File, Line, Iram), Size)
SDK_ALLOC(pvPortRealloc, (void * Pointer, size_t Size, char const * File, int Line), (Pointer, Size, File, Line), Size)
SDK_ALLOC(pvPortCalloc, (size_t Count, size_t Size, char const * File, int Line), (Count, Size, File, Line), AllocationBytes(Count, Size))
SDK_ALLOC(pvPortCallocIram, (size_t Count, size_t Size, char const * File, int Line), (Count, Size, File, Line), AllocationBytes(Count, Size))
SDK_ALLOC(pvPortZalloc, (size_t Size, char const * File, int Line), (Size, File, Line), Size)
SDK_ALLOC(pvPortZallocIram, (size_t Size, char const * File, int Line), (Size, File, Line), Size)
#undef SDK_ALLOC
 }

void FeederRestartLogSetup( bool Mounted )
 {
  FileSystemMounted = Mounted;
  if( Mounted )LittleFS.mkdir("/diagnostics");
 }

bool FeederRestartLogLoop( bool TimeValid )
 {
  if( RestartIssued )return true;
  const uint64_t Now = TimeValid ? std::time(nullptr) : 0;
  if( AllocationFailed )
   {
    RestartIssued = true;
    Append("oom", Now, 0, &AllocationFailure);
    ESP.restart();
    return true;
   }
  if( !FileSystemMounted )return false;

  const unsigned ResetReason = ESP.getResetInfoPtr()->reason;
  if( !BootRecorded )
   {
    BootRecorded = true;
    Append("boot", 0, ResetReason, nullptr);
   }
  if( Now && !BootTimeRecorded )
   {
    BootTimeRecorded = true;
    const uint64_t Uptime = GnMillis64() / 1000;
    Append("boot-time", Now >= Uptime ? Now - Uptime : 0, ResetReason, nullptr);
   }
  return false;
 }
