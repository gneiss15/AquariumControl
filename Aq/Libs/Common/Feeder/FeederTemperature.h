// FeederTemperature.h, Version: 2.49
#pragma once

#include <GnBasics.h>
#include <GnCoopTimer.h>
#include <OneWire.h>
#include <LittleFS.h>

class TFeederTemperature : public TSingleton<TFeederTemperature>, TLoopInstance
 {
  friend class TSingleton<TFeederTemperature>;

 public:
  enum class TDiagnosticState : uint8_t
   {
    Unknown,
    Boot,
    Ok,
    Nok
   };

  enum class TDiagnosticReason : uint8_t
   {
    None,
    Boot,
    SensorNotFound,
    ConversionResetFailed,
    ReadResetFailed,
    ReadCrcFailed,
    TemperatureInvalid
   };

  struct __attribute__((packed)) TDiagnosticLogEntry
   {
    uint32_t                  UnixTime;
    uint64_t                  UptimeMilliseconds;
    uint32_t                  Poll;
    uint8_t                   State;
    uint8_t                   Reason;
   };

 private:
  enum class TTemperatureError : uint8_t
   {
    None,
    InvalidPin,
    PinConflict,
    InvalidUnit,
    OneWireAllocationFailed,
    ConversionStartFailed,
    ReadFailed
   };

 private:
                            TFeederTemperature(void);
                            ~TFeederTemperature(void);
  void                      Loop(void) override;
  void                      StartConversion( TGnCoopTimerBase * Tmr, uint16_t IntervalsElapsed );
  void                      ReadTemp( TGnCoopTimerBase * Tmr, uint16_t IntervalsElapsed );
  int8_t                    ReadScratchpad( uint8_t * Scratchpad, TDiagnosticReason & LastReason );
  bool                      Fahrenheit(void) const;
  bool                      ConfigurationValid(void);
  void                      ResetDiagnosticStats(void);
  void                      RecordDiagnosticBoot(void);
  void                      RecordDiagnosticPoll( bool Ok, TDiagnosticReason Reason );
  void                      RecordDiagnosticState( TDiagnosticState State, TDiagnosticReason Reason );
  bool                      EnsureDiagnosticLog(void);
  bool                      AppendDiagnosticLog( TDiagnosticState State, TDiagnosticReason Reason );
  bool                      ReadDiagnosticLogHeader( File & Log ) const;
  static char const *       DiagnosticStateName_( TDiagnosticState State );
  static char const *       DiagnosticReasonName_( TDiagnosticReason Reason );

 public:
  void                      Setup(void);
  void                      PauseForDiagnostic( TGnCoopTimer_t Milliseconds );
  bool                      Enabled(void) const;
  bool                      Valid(void) const { return FValid; }
  float                     Value(void) const;
  float                     TemperatureCelsius(void) const { return FTemperatureCelsius; }
  char const *              Unit(void) const;
  time_t                    UpdatedAt(void) const { return FUpdatedAt; }
  char const *              Error(void) const;
  TGNString                 DiagnosticStatsJson(void) const;
  bool                      ClearDiagnostics(void);
  size_t                    ReadDiagnosticLog( size_t StartEntry, TDiagnosticLogEntry * Entries, size_t MaxEntries ) const;
  size_t                    DiagnosticLogEntryCount(void) const;
  size_t                    DiagnosticLogBytes(void) const;
  static char const *       DiagnosticStateName( uint8_t State );
  static char const *       DiagnosticReasonName( uint8_t Reason );

 private:
  static constexpr TGnCoopTimer_t FConversionMilliseconds = 1000;
  static constexpr TGnCoopTimer_t FConversionIntervalMilliseconds = 10000;
  static constexpr uint8_t        FReadAttempts = 20;
  static constexpr size_t         FDiagnosticLogMaxBytes = 128 * 1024;

  OneWire *                 FOneWire;
  TGnCoopTimerMs            FLoopTimer;
  TGnCoopTimerMs            FDelayTimer;
  bool                      FConfigured;
  bool                      FValid;
  TGnElapsedTimerMs         FDiagnosticPauseTimer;
  TGnCoopTimer_t            FDiagnosticPauseMilliseconds;
  bool                      FDiagnosticPaused;
  float                     FTemperatureCelsius;
  time_t                    FUpdatedAt;
  TTemperatureError         FError;
  TDiagnosticState          FDiagnosticState;
  TGnElapsedTimerMs64       FUptimeTimer;
  TGnElapsedTimerMs64       FDiagnosticSessionTimer;
  uint32_t                  FDiagnosticPolls;
  uint32_t                  FDiagnosticOk;
  uint32_t                  FDiagnosticNok;
  uint32_t                  FDiagnosticReadStatus[ FReadAttempts + 1 ];
  uint32_t                  FDiagnosticConversionResetFailed;
  uint32_t                  FDiagnosticReadResetFailed;
  uint32_t                  FDiagnosticReadCrcFailed;
  uint32_t                  FDiagnosticTemperatureInvalid;
  uint32_t                  FDiagnosticStateChanges;
  uint32_t                  FDiagnosticConsecutiveNok;
  uint32_t                  FDiagnosticMaxConsecutiveNok;
  uint32_t                  FDiagnosticLogWriteErrors;
  bool                      FDiagnosticLogFull;
 };

#define FeederTemperature TFeederTemperature::Instance()
