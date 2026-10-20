// FeederTemperature.cpp, Version: 2.51
#include <GnBasics.h>
#include "FeederTemperature.h"

#include "FeederConfig.h"
#include "FeederDrive.h"
#include <LittleFS.h>
#include <new>

TFeederTemperature::TFeederTemperature(void)
: FOneWire( nullptr )
, FLoopTimer( Closure( this, &TFeederTemperature::StartConversion ), FConversionIntervalMilliseconds, false )
, FDelayTimer( Closure( this, &TFeederTemperature::ReadTemp ), FConversionMilliseconds, false )
, FConfigured( false )
, FValid( false )
, FDiagnosticPauseMilliseconds( 0 )
, FDiagnosticPaused( false )
, FTemperatureCelsius( 0.0F )
, FUpdatedAt( 0 )
, FError( TTemperatureError::None )
, FDiagnosticState( TDiagnosticState::Unknown )
, FDiagnosticPolls( 0 )
, FDiagnosticOk( 0 )
, FDiagnosticNok( 0 )
, FDiagnosticConversionResetFailed( 0 )
, FDiagnosticReadResetFailed( 0 )
, FDiagnosticReadCrcFailed( 0 )
, FDiagnosticTemperatureInvalid( 0 )
, FDiagnosticStateChanges( 0 )
, FDiagnosticConsecutiveNok( 0 )
, FDiagnosticMaxConsecutiveNok( 0 )
, FDiagnosticLogWriteErrors( 0 )
, FDiagnosticLogFull( false )
 {
  memset( FDiagnosticReadStatus, 0, sizeof( FDiagnosticReadStatus ) );
 }

TFeederTemperature::~TFeederTemperature(void)
 {
  delete FOneWire;
 }

namespace
{
constexpr uint32_t OneWireDiagnosticLogMagic = 0x3144574FUL; // "OWD1"
constexpr uint8_t OneWireDiagnosticLogVersion = 2;
constexpr uint32_t MinimumValidUnixTime = 1600000000UL;
constexpr char OneWireDiagnosticLogFile[] = "/OneWireDiag.bin";

struct __attribute__((packed)) TOneWireDiagnosticLogHeader
 {
  uint32_t                  Magic;
  uint8_t                   Version;
  uint8_t                   EntrySize;
  uint16_t                  Reserved;
 };

static_assert( sizeof( TOneWireDiagnosticLogHeader ) == 8, "Unexpected OneWire diagnostic log header size" );
static_assert( sizeof( TFeederTemperature::TDiagnosticLogEntry ) == 18, "Unexpected OneWire diagnostic log entry size" );

template <typename TString>
void AppendDiagnosticInteger( TString & Output, uint32_t Value )
 {
  char Text[16];
  snprintf( Text, sizeof( Text ), "%lu", static_cast<unsigned long>( Value ) );
  Output += Text;
 }

template <typename TString>
void AppendDiagnosticInteger64( TString & Output, uint64_t Value )
 {
  char Text[24];
  snprintf( Text, sizeof( Text ), "%llu", static_cast<unsigned long long>( Value ) );
  Output += Text;
 }
}

char const * TFeederTemperature::DiagnosticStateName_( TDiagnosticState State )
 {
  switch( State )
   {
    case TDiagnosticState::Unknown: return "Unknown";
    case TDiagnosticState::Boot:    return "BOOT";
    case TDiagnosticState::Ok:      return "OK";
    case TDiagnosticState::Nok:     return "NOK";
   }
  return "?";
 }

char const * TFeederTemperature::DiagnosticReasonName_( TDiagnosticReason Reason )
 {
  switch( Reason )
   {
    case TDiagnosticReason::None:                  return "";
    case TDiagnosticReason::Boot:                  return "Boot";
    case TDiagnosticReason::SensorNotFound:        return "SensorNotFound";
    case TDiagnosticReason::ConversionResetFailed: return "ConversionResetFailed";
    case TDiagnosticReason::ReadResetFailed:       return "ReadResetFailed";
    case TDiagnosticReason::ReadCrcFailed:         return "ReadCrcFailed";
    case TDiagnosticReason::TemperatureInvalid:    return "TemperatureInvalid";
   }
  return "Unknown";
 }

char const * TFeederTemperature::DiagnosticStateName( uint8_t State )
 {
  return DiagnosticStateName_( static_cast<TDiagnosticState>( State ) );
 }

char const * TFeederTemperature::DiagnosticReasonName( uint8_t Reason )
 {
  return DiagnosticReasonName_( static_cast<TDiagnosticReason>( Reason ) );
 }

void TFeederTemperature::ResetDiagnosticStats(void)
 {
  FDiagnosticState = TDiagnosticState::Unknown;
  FDiagnosticSessionTimer.Start();
  FDiagnosticPolls = 0;
  FDiagnosticOk = 0;
  FDiagnosticNok = 0;
  memset( FDiagnosticReadStatus, 0, sizeof( FDiagnosticReadStatus ) );
  FDiagnosticConversionResetFailed = 0;
  FDiagnosticReadResetFailed = 0;
  FDiagnosticReadCrcFailed = 0;
  FDiagnosticTemperatureInvalid = 0;
  FDiagnosticStateChanges = 0;
  FDiagnosticConsecutiveNok = 0;
  FDiagnosticMaxConsecutiveNok = 0;
  FDiagnosticLogWriteErrors = 0;
  FDiagnosticLogFull = false;
 }

bool TFeederTemperature::ReadDiagnosticLogHeader( File & Log ) const
 {
  if( !Log || Log.size() < sizeof( TOneWireDiagnosticLogHeader ) )
    return false;

  TOneWireDiagnosticLogHeader Header = {};
  if( Log.seek( 0, SeekSet ) == false
      || Log.read( reinterpret_cast<uint8_t *>( &Header ), sizeof( Header ) ) != sizeof( Header ) )
    return false;

  if( Header.Magic != OneWireDiagnosticLogMagic
      || Header.Version != OneWireDiagnosticLogVersion
      || Header.EntrySize != sizeof( TDiagnosticLogEntry ) )
    return false;
  return ( Log.size() - sizeof( Header ) ) % sizeof( TDiagnosticLogEntry ) == 0;
 }

bool TFeederTemperature::EnsureDiagnosticLog(void)
 {
  File Log = LittleFS.open( OneWireDiagnosticLogFile, "r" );
  if( Log )
   {
    const bool Valid = ReadDiagnosticLogHeader( Log );
    Log.close();
    if( Valid )
      return true;
    LittleFS.remove( OneWireDiagnosticLogFile );
   }

  Log = LittleFS.open( OneWireDiagnosticLogFile, "w" );
  if( !Log )
   {
    ++FDiagnosticLogWriteErrors;
    return false;
   }
  const TOneWireDiagnosticLogHeader Header =
   {
    OneWireDiagnosticLogMagic,
    OneWireDiagnosticLogVersion,
    static_cast<uint8_t>( sizeof( TDiagnosticLogEntry ) ),
    0
   };
  const bool Ok = Log.write( reinterpret_cast<uint8_t const *>( &Header ), sizeof( Header ) ) == sizeof( Header );
  Log.close();
  if( !Ok )
    ++FDiagnosticLogWriteErrors;
  return Ok;
 }

bool TFeederTemperature::AppendDiagnosticLog( TDiagnosticState State, TDiagnosticReason Reason )
 {
  if( FDiagnosticLogFull || !EnsureDiagnosticLog() )
    return false;

  File Log = LittleFS.open( OneWireDiagnosticLogFile, "a" );
  if( !Log )
   {
    ++FDiagnosticLogWriteErrors;
    return false;
   }
  if( Log.size() + sizeof( TDiagnosticLogEntry ) > FDiagnosticLogMaxBytes )
   {
    Log.close();
    FDiagnosticLogFull = true;
    return false;
   }

  const time_t CurrentTime = time( nullptr );
  const TDiagnosticLogEntry Entry =
   {
    CurrentTime >= static_cast<time_t>( MinimumValidUnixTime ) ? static_cast<uint32_t>( CurrentTime ) : 0,
    FUptimeTimer.Elapsed(),
    FDiagnosticPolls,
    static_cast<uint8_t>( State ),
    static_cast<uint8_t>( Reason )
   };
  const bool Ok = Log.write( reinterpret_cast<uint8_t const *>( &Entry ), sizeof( Entry ) ) == sizeof( Entry );
  Log.close();
  if( !Ok )
    ++FDiagnosticLogWriteErrors;
  return Ok;
 }

void TFeederTemperature::RecordDiagnosticBoot(void)
 {
  AppendDiagnosticLog( TDiagnosticState::Boot, TDiagnosticReason::Boot );
 }

void TFeederTemperature::RecordDiagnosticState( TDiagnosticState State, TDiagnosticReason Reason )
 {
  if( State != TDiagnosticState::Ok && State != TDiagnosticState::Nok )
    return;
  if( FDiagnosticState == State )
    return;

  if( FDiagnosticState != TDiagnosticState::Unknown )
    ++FDiagnosticStateChanges;
  FDiagnosticState = State;
  AppendDiagnosticLog( State, Reason );
 }

void TFeederTemperature::RecordDiagnosticPoll( bool Ok, TDiagnosticReason Reason )
 {
  ++FDiagnosticPolls;
  if( Ok )
   {
    ++FDiagnosticOk;
    FDiagnosticConsecutiveNok = 0;
    RecordDiagnosticState( TDiagnosticState::Ok, TDiagnosticReason::None );
    return;
   }

  ++FDiagnosticNok;
  ++FDiagnosticConsecutiveNok;
  if( FDiagnosticConsecutiveNok > FDiagnosticMaxConsecutiveNok )
    FDiagnosticMaxConsecutiveNok = FDiagnosticConsecutiveNok;

  switch( Reason )
   {
    case TDiagnosticReason::ConversionResetFailed: ++FDiagnosticConversionResetFailed; break;
    case TDiagnosticReason::TemperatureInvalid:    ++FDiagnosticTemperatureInvalid; break;
    default: break;
   }
  RecordDiagnosticState( TDiagnosticState::Nok, Reason );
 }

size_t TFeederTemperature::DiagnosticLogEntryCount(void) const
 {
  File Log = LittleFS.open( OneWireDiagnosticLogFile, "r" );
  if( !ReadDiagnosticLogHeader( Log ) )
   {
    if( Log )
      Log.close();
    return 0;
   }
  const size_t Count = ( Log.size() - sizeof( TOneWireDiagnosticLogHeader ) ) / sizeof( TDiagnosticLogEntry );
  Log.close();
  return Count;
 }

size_t TFeederTemperature::DiagnosticLogBytes(void) const
 {
  File Log = LittleFS.open( OneWireDiagnosticLogFile, "r" );
  if( !ReadDiagnosticLogHeader( Log ) )
   {
    if( Log )
      Log.close();
    return 0;
   }
  const size_t Bytes = Log.size();
  Log.close();
  return Bytes;
 }

size_t TFeederTemperature::ReadDiagnosticLog( size_t StartEntry, TDiagnosticLogEntry * Entries, size_t MaxEntries ) const
 {
  if( Entries == nullptr || MaxEntries == 0 )
    return 0;

  File Log = LittleFS.open( OneWireDiagnosticLogFile, "r" );
  if( !ReadDiagnosticLogHeader( Log ) )
   {
    if( Log )
      Log.close();
    return 0;
   }

  const size_t Count = ( Log.size() - sizeof( TOneWireDiagnosticLogHeader ) ) / sizeof( TDiagnosticLogEntry );
  if( StartEntry >= Count )
   {
    Log.close();
    return 0;
   }
  const size_t Available = Count - StartEntry;
  const size_t ToRead = MaxEntries < Available ? MaxEntries : Available;
  const size_t Offset = sizeof( TOneWireDiagnosticLogHeader ) + StartEntry * sizeof( TDiagnosticLogEntry );
  if( !Log.seek( Offset, SeekSet ) )
   {
    Log.close();
    return 0;
   }
  const size_t Bytes = Log.read( reinterpret_cast<uint8_t *>( Entries ), ToRead * sizeof( TDiagnosticLogEntry ) );
  Log.close();
  return Bytes / sizeof( TDiagnosticLogEntry );
 }

TGNString TFeederTemperature::DiagnosticStatsJson(void) const
 {
  const size_t LogEntries = DiagnosticLogEntryCount();
  const size_t LogBytes = DiagnosticLogBytes();
  const uint64_t SessionMs = FDiagnosticSessionTimer.Elapsed();
  TGNString Result;
  const auto Build = [&]( auto & Output )
   {
    Output += F( "{\"Enabled\":" );
    Output += Enabled() ? "true" : "false";
    Output += F( ",\"State\":\"" );
    Output += DiagnosticStateName_( FDiagnosticState );
    Output += F( "\",\"SessionMs\":" );
    AppendDiagnosticInteger64( Output, SessionMs );
    Output += F( ",\"Polls\":" );
    AppendDiagnosticInteger( Output, FDiagnosticPolls );
    Output += F( ",\"PollOk\":" );
    AppendDiagnosticInteger( Output, FDiagnosticOk );
    Output += F( ",\"PollNok\":" );
    AppendDiagnosticInteger( Output, FDiagnosticNok );
    Output += F( ",\"ReadNok\":" );
    AppendDiagnosticInteger( Output, FDiagnosticReadStatus[ 0 ] );
    for( int i = 1;  i <= FReadAttempts;  ++ i )
     {
      Output += F( ",\"ReadOk" );
      AppendDiagnosticInteger( Output, i );
      Output += F( "\":" );
      AppendDiagnosticInteger( Output, FDiagnosticReadStatus[ i ] );
     }
    Output += F( ",\"ConversionResetFailed\":" );
    AppendDiagnosticInteger( Output, FDiagnosticConversionResetFailed );
    Output += F( ",\"ReadResetFailed\":" );
    AppendDiagnosticInteger( Output, FDiagnosticReadResetFailed );
    Output += F( ",\"ReadCrcFailed\":" );
    AppendDiagnosticInteger( Output, FDiagnosticReadCrcFailed );
    Output += F( ",\"TemperatureInvalid\":" );
    AppendDiagnosticInteger( Output, FDiagnosticTemperatureInvalid );
    Output += F( ",\"StateChanges\":" );
    AppendDiagnosticInteger( Output, FDiagnosticStateChanges );
    Output += F( ",\"ConsecutiveNok\":" );
    AppendDiagnosticInteger( Output, FDiagnosticConsecutiveNok );
    Output += F( ",\"MaxConsecutiveNok\":" );
    AppendDiagnosticInteger( Output, FDiagnosticMaxConsecutiveNok );
    Output += F( ",\"LogEntries\":" );
    AppendDiagnosticInteger( Output, static_cast<uint32_t>( LogEntries ) );
    Output += F( ",\"LogBytes\":" );
    AppendDiagnosticInteger( Output, static_cast<uint32_t>( LogBytes ) );
    Output += F( ",\"LogEntryBytes\":" );
    AppendDiagnosticInteger( Output, static_cast<uint32_t>( sizeof( TDiagnosticLogEntry ) ) );
    Output += F( ",\"LogMaxBytes\":" );
    AppendDiagnosticInteger( Output, static_cast<uint32_t>( FDiagnosticLogMaxBytes ) );
    Output += F( ",\"ConversionMs\":" );
    AppendDiagnosticInteger( Output, FConversionMilliseconds );
    Output += F( ",\"ConversionIntervalMs\":" );
    AppendDiagnosticInteger( Output, FConversionIntervalMilliseconds );
    Output += F( ",\"ReadMaxAttempts\":" );
    AppendDiagnosticInteger( Output, FReadAttempts );
    Output += F( ",\"LogFull\":" );
    Output += FDiagnosticLogFull ? "true" : "false";
    Output += F( ",\"LogWriteErrors\":" );
    AppendDiagnosticInteger( Output, FDiagnosticLogWriteErrors );
    Output += '}';
   };
  if( !GnBuildStringExact( Result, Build ) )
    return TGNString();
  return Result;
 }

bool TFeederTemperature::ClearDiagnostics(void)
 {
  ResetDiagnosticStats();
  LittleFS.remove( OneWireDiagnosticLogFile );
  if( !EnsureDiagnosticLog() )
    return false;
  RecordDiagnosticBoot();
  return FDiagnosticLogWriteErrors == 0;
 }

bool TFeederTemperature::Enabled(void) const
 {
  return FeederConfig.DallasPresent;
 }

bool TFeederTemperature::Fahrenheit(void) const
 {
  return FeederConfig.TemperatureUnit.Fahrenheit();
 }

char const * TFeederTemperature::Error(void) const
 {
  switch( FError )
   {
    case TTemperatureError::None:
      return "";
    case TTemperatureError::InvalidPin:
      return "Dallas GPIO is outside the supported range 0..15";
    case TTemperatureError::PinConflict:
      return "Dallas GPIO conflicts with another configured hardware pin";
    case TTemperatureError::InvalidUnit:
      return "Temperature unit must be C or F";
    case TTemperatureError::OneWireAllocationFailed:
      return "Could not allocate Dallas OneWire bus";
    case TTemperatureError::ConversionStartFailed:
      return "Dallas temperature conversion could not be started";
    case TTemperatureError::ReadFailed:
      return "Dallas temperature read failed";
   }
  return "Unknown temperature error";
 }

bool TFeederTemperature::ConfigurationValid(void)
 {
  if( FeederConfig.DallasPin < 0 || FeederConfig.DallasPin > 15 )
   {
    FError = TTemperatureError::InvalidPin;
    return false;
   }

  if( FeederDrive.PinUsed( FeederConfig.DallasPin ) )
   {
    FError = TTemperatureError::PinConflict;
    return false;
   }

  if( !FeederConfig.TemperatureUnit.Valid() )
   {
    FError = TTemperatureError::InvalidUnit;
    return false;
   }

  return true;
 }

float TFeederTemperature::Value(void) const
 {
  if( Fahrenheit() )
    return FTemperatureCelsius * 9.0F / 5.0F + 32.0F;
  return FTemperatureCelsius;
 }

char const * TFeederTemperature::Unit(void) const
 {
  return Fahrenheit() ? "°F" : "°C";
 }

void TFeederTemperature::PauseForDiagnostic( TGnCoopTimer_t Milliseconds )
 {
  if( !FConfigured || FOneWire == nullptr )
    return;

  FDiagnosticPauseMilliseconds = Milliseconds;
  FDiagnosticPauseTimer.Start();
  if( FDiagnosticPaused )
    return;

  FDiagnosticPaused = true;
  FLoopTimer.Stop();
  FDelayTimer.Stop();
  FOneWire->depower();
 }

void TFeederTemperature::Setup(void)
 {
  ResetDiagnosticStats();
  FConfigured = Enabled();
  if( !FConfigured )
    return;

  if( !ConfigurationValid() )
    return;

  FOneWire = new ( std::nothrow ) OneWire( static_cast<uint8_t>( FeederConfig.DallasPin ) );
  if( FOneWire == nullptr )
   {
    FError = TTemperatureError::OneWireAllocationFailed;
    return;
   }

  EnsureDiagnosticLog();
  RecordDiagnosticBoot();
  FLoopTimer.Start();
  StartConversion( nullptr, 1 );
 }

void TFeederTemperature::StartConversion( TGnCoopTimerBase *, uint16_t )
 {
  if( FOneWire == nullptr || FDiagnosticPaused )
    return;

  if( FOneWire->reset() == 0 )
   {
    FDelayTimer.Stop();
    FValid = false;
    FError = TTemperatureError::ConversionStartFailed;
    RecordDiagnosticPoll( false, TDiagnosticReason::ConversionResetFailed );
    return;
   }

  FOneWire->skip();
  FOneWire->write( 0x44 );
  FDelayTimer.Start();
 }

int8_t TFeederTemperature::ReadScratchpad( uint8_t * Scratchpad, TDiagnosticReason & LastReason )
 {
  LastReason = TDiagnosticReason::ReadCrcFailed;
  for( uint8_t Attempt = 1; Attempt <= FReadAttempts; ++Attempt )
   {
    if( FOneWire->reset() == 0 )
     {
      ++FDiagnosticReadResetFailed;
      LastReason = TDiagnosticReason::ReadResetFailed;
      continue;
     }

    FOneWire->skip();
    FOneWire->write( 0xBE );
    for( uint8_t Index = 0; Index < 9; ++Index )
      Scratchpad[Index] = FOneWire->read();

    if( OneWire::crc8( Scratchpad, 8 ) != Scratchpad[8] )
     {
      ++FDiagnosticReadCrcFailed;
      LastReason = TDiagnosticReason::ReadCrcFailed;
      continue;
     }

    return static_cast<int8_t>( Attempt );
   }
  return 0;
 }

void TFeederTemperature::ReadTemp( TGnCoopTimerBase * Tmr, uint16_t )
 {
  Tmr->Stop();
  if( FOneWire == nullptr || FDiagnosticPaused )
    return;

  uint8_t Scratchpad[9] = {};
  TDiagnosticReason LastReason = TDiagnosticReason::ReadCrcFailed;
  const int8_t ReadAttempt = ReadScratchpad( Scratchpad, LastReason );
  ++FDiagnosticReadStatus[ ReadAttempt ];
  if( ReadAttempt == 0 )
   {
    FValid = false;
    FError = TTemperatureError::ReadFailed;
    RecordDiagnosticPoll( false, LastReason );
    return;
   }

  const int16_t Raw = static_cast<int16_t>( static_cast<uint16_t>( Scratchpad[0] )
                    | ( static_cast<uint16_t>( Scratchpad[1] ) << 8 ) );
  const float Temperature = static_cast<float>( Raw ) / 16.0F;
  if( Temperature < -55.0F || Temperature > 125.0F )
   {
    FValid = false;
    FError = TTemperatureError::ReadFailed;
    RecordDiagnosticPoll( false, TDiagnosticReason::TemperatureInvalid );
    return;
   }

  FTemperatureCelsius = Temperature;
  FValid = true;
  FError = TTemperatureError::None;
  RecordDiagnosticPoll( true, TDiagnosticReason::None );
  const time_t CurrentTime = time( nullptr );
  FUpdatedAt = CurrentTime > 0 ? CurrentTime : 0;
 }

void TFeederTemperature::Loop(void)
 {
  if( !FDiagnosticPaused )
    return;

  if( FDiagnosticPauseTimer.Elapsed() < FDiagnosticPauseMilliseconds )
    return;

  FDiagnosticPaused = false;
  FLoopTimer.Start();
  StartConversion( nullptr, 1 );
 }
