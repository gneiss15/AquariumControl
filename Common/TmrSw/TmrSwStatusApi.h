// TmrSwStatusApi.h, Version: 3.03
#pragma once

#include <GnBasics.h>
#include <GnCoopTimer.h>
#include <GnUdpInfoParts.h>

template<bool> struct GnImpl_TUdpStatusData;
using TUdpStatusData = GnImpl_TUdpStatusData<false>;

class TTmrSwStatusApi : public TSingleton<TTmrSwStatusApi>
 {
  friend class TSingleton<TTmrSwStatusApi>;

 private:
                                   TTmrSwStatusApi(void);
  template <typename TString>
  static void                      AppendJsonChar( TString & Json, char Char );
  template <typename TString>
  static void                      AppendJsonString( TString & Json, char const * Value );
  template <typename TString>
  static void                      AppendJsonString( TString & Json, TGNString const & Value );
#if defined( GNPlatformESP )
  template <typename TString>
  static void                      AppendJsonString( TString & Json, __FlashStringHelper const * Value );
#endif
  template <typename TString>
  static void                      AppendInteger( TString & Json, long long Value );
  template <typename TString>
  static void                      AppendFloat( TString & Json, float Value, unsigned Digits );
  void                             FillStatus( TUdpStatusData & Status ) const;
  static uint64_t                  StatusSignature( TGNString const & json );

 public:
  TGNString                        RescueVersion(void) const;
  TGNString                        DeviceJson(void) const;
  TGNString                        StatusJson(void) const;
  TGNString                        StatusJson( uint64_t & signature ) const;
  bool                             CompactStatus( uint64_t & Signature, TGnUdpInfoParts * Parts,
                                                   TGNString & Packet ) const;
  TGNString                        StatusEventJson( uint64_t Sequence ) const;
  void                             FileSystemMounted( bool Mounted )          { FFileSystemMounted = Mounted; }
  void                             SetupError( TGNString const & Error )      { FSetupError = Error; }
  void                             WebSocketStarted( bool Started )           { FWebSocketStarted = Started; }

 private:
  TGnElapsedTimerMs64              FStartedTimer;
  mutable time_t                   FOnlineSince = 0;
  bool                             FFileSystemMounted = false;
  bool                             FWebSocketStarted = false;
  TGNString                        FSetupError;
 };

#define TmrSwStatusApi TTmrSwStatusApi::Instance()
