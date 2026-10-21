// FeederStatusApi.h, Version: 2.46
#pragma once

#include <GnBasics.h>
#include <GnCoopTimer.h>

class TFeederStatusApi : public TSingleton<TFeederStatusApi>
 {
  friend class TSingleton<TFeederStatusApi>;

 private:
                    TFeederStatusApi(void);

  template <typename TString>
    static void     AppendJsonChar( TString & Json, char Char );
  template <typename TString>
    static void     AppendJsonString( TString & Json, char const * Value );
  template <typename TString>
    static void     AppendJsonString( TString & Json, TGNString const & Value );
  #if defined( GNPlatformESP )
    template <typename TString>
      static void   AppendJsonString( TString & Json, __FlashStringHelper const * Value );
  #endif
  template <typename TString>
    static void     AppendInteger( TString & Json, long long Value );
  template <typename TString>
    static void     AppendNumber( TString & Json, float Value, unsigned Decimals );

  TGNString         BuildStatusJson( uint64_t * signature ) const;
  static uint64_t   StatusSignature( TGNString const & json );

 public:
  TGNString         DeviceJson(void) const;
  TGNString         StatusJson(void) const;
  TGNString         StatusJson( uint64_t & signature ) const;
  void              FileSystemMounted( bool Mounted )          { FFileSystemMounted = Mounted; }
  void              SetupError( TGNString const & Error )      { FSetupError = Error; }
  void              WebSocketStarted( bool Started )           { FWebSocketStarted = Started; }

 private:
  TGnElapsedTimerMs64 FStartedTimer;
  mutable time_t      FOnlineSince = 0;
  bool                FFileSystemMounted = false;
  bool                FWebSocketStarted = false;
  TGNString           FSetupError;
 };

#define FeederStatusApi TFeederStatusApi::Instance()
