// FeederStatusApi.h, Version: 2.53
#pragma once

#include <GnBasics.h>
#include <GnCoopTimer.h>
#include <GnUdpStatus.h>

class TFeederStatusApi : public TSingleton<TFeederStatusApi>
 {
  friend class TSingleton<TFeederStatusApi>;

 public:
  struct THeapSample
   {
    uint32_t Free = 0;
    uint32_t Largest = 0;
   };
  struct TStatusMemory
   {
    THeapSample Entry;
    THeapSample BeforeText;
    THeapSample Text;
    uint32_t    BufferBytes = 0;
    uint32_t    JsonBytes = 0;
   };

 private:
                    TFeederStatusApi(void);

  TGNString         BuildStatusJson( uint64_t * signature, char const ** Failure = nullptr, TStatusMemory * Memory = nullptr,
                                   TGnUdpInfoParts * Parts = nullptr, bool Events = false, bool Discovery = false, uint64_t const * Sequence = nullptr ) const;

 public:
  TGNString         DeviceJson(void) const;
  TGNString         StatusJson( char const ** Failure = nullptr, TStatusMemory * Memory = nullptr ) const;
  TGNString         HttpStatusJson( bool Measure, int & Code ) const;
  bool              StatusSignature( uint64_t & Signature ) const;
  TGNString         StatusEventJson( uint64_t Sequence ) const;
  bool              CompactStatus( uint64_t & Signature, TGnUdpInfoParts * Parts, TGNString & Packet ) const;
  void              FileSystemMounted( bool Mounted )          { FFileSystemMounted = Mounted; }
  void              SetupError( TGNString const & Error )      { FSetupError = Error; }
  void              WebSocketStarted( bool Started )           { FWebSocketStarted = Started; }

 private:
  TGnElapsedTimerMs64 FStartedTimer;
  mutable time_t      FOnlineSince = 0;
  mutable bool        FReportedStatusFailure = false;
  bool                FFileSystemMounted = false;
  bool                FWebSocketStarted = false;
  TGNString           FSetupError;
 };

#define FeederStatusApi TFeederStatusApi::Instance()
