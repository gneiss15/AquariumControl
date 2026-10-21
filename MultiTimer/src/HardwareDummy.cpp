// HardwareDummy.cpp, Version: 2.33
#include <GnBasics.h>
#include "BuildConfig.h"
#if defined( SYSTEM_DUMMY )
  #include "IHardware.h"
  #include <algorithm>
  #include <chrono>
  #include <ctime>
  #include <iomanip>
  #include <iostream>
  #include <mutex>
  #include <sstream>
  #include <thread>
  #include <vector>

class TIHardware::TImplementation
 {
 public:
                             TImplementation(void) = default;
  bool                       Setup( const TChannelConfigs & channels, TGNString & error );
  bool                       SetPin( unsigned channelNr, bool on, TGNString & error, bool firstTime );
  bool                       GetPin( unsigned channelNr, bool & active, TGNString & error );
  bool                       GetOutputState( unsigned channelNr, bool & on, TGNString & error );
  bool                       RequestShutdown( TGNString & error );
  bool                       CancelShutdown( TGNString & error );
  void                       Loop(void) {}
  void                       EnableRuntimeOutput(void);

 private:
  static TGNString           TimeStamp(void);
  int                        FindChannel( const TGNString & value ) const;
  void                       ConsoleLoop(void);

  TChannelConfigs            FConfig;
  std::vector<uint8_t>       FOutputStates;
  std::vector<uint8_t>       FInputStates;
  std::mutex                 FStateMutex;
  bool                       FOutputEnabled = false;
 };

TGNString TIHardware::TImplementation::TimeStamp(void)
 {
  const std::time_t now = std::time( nullptr );
  std::tm local{};
  localtime_r( &now, &local );
  std::ostringstream text;
  text << std::put_time( &local, "%Y-%m-%d %H:%M:%S" );
  return text.str();
 }

bool TIHardware::TImplementation::SetPin( unsigned channelNr, bool on, TGNString & error, bool )
 {
  std::lock_guard<std::mutex> lock( FStateMutex );
  if( channelNr >= FConfig.size() )
   {
    error = "Invalid channel number";
    return false;
   }
  if( !FConfig[ channelNr ].OutputPin )
   {
    error = "Channel has no OutputPin: " + FConfig[ channelNr ].Id;
    return false;
   }
  const bool changed = FOutputStates[ channelNr ] != static_cast<uint8_t>( on );
  FOutputStates[ channelNr ] = on;
  if( changed && FOutputEnabled )
   {
    std::cout << TimeStamp() << " Switch " << FConfig[ channelNr ].Id << ": "
              << ( on ? "ON" : "OFF" ) << '\n';
    std::cout.flush();
   }
  error.clear();
  return true;
 }

bool TIHardware::TImplementation::GetPin( unsigned channelNr, bool & active, TGNString & error )
 {
  std::lock_guard<std::mutex> lock( FStateMutex );
  if( channelNr >= FConfig.size() )
   {
    error = "Invalid channel number";
    return false;
   }
  if( !FConfig[ channelNr ].InputPin )
   {
    error = "Channel has no InputPin: " + FConfig[ channelNr ].Id;
    return false;
   }
  active = FInputStates[ channelNr ] != 0;
  error.clear();
  return true;
 }

bool TIHardware::TImplementation::GetOutputState( unsigned channelNr, bool & on, TGNString & error )
 {
  std::lock_guard<std::mutex> lock( FStateMutex );
  if( channelNr >= FConfig.size() || !FConfig[ channelNr ].OutputPin )
   {
    error = "Output not available";
    return false;
   }
  on = FOutputStates[ channelNr ] != 0;
  error.clear();
  return true;
 }

bool TIHardware::TImplementation::RequestShutdown( TGNString & error )
 {
  std::cout << TimeStamp() << " Dummy shutdown requested\n";
  error.clear();
  return true;
 }

bool TIHardware::TImplementation::CancelShutdown( TGNString & error )
 {
  std::cout << TimeStamp() << " Dummy shutdown cancelled\n";
  error.clear();
  return true;
 }

void TIHardware::TImplementation::EnableRuntimeOutput(void)
 {
  std::lock_guard<std::mutex> lock( FStateMutex );
  FOutputEnabled = true;
 }

int TIHardware::TImplementation::FindChannel( const TGNString & value ) const
 {
  try
   {
    size_t used = 0;
    const int id = std::stoi( value, &used );
    if( used == value.size() && id >= 0 && static_cast<size_t>( id ) < FConfig.size() )
      return id;
   }
  catch( ... )
   {
   }
  const auto item = std::find_if( FConfig.begin(), FConfig.end(), [&]( const TChannelConfig & channel )
   {
    return channel.Id == value;
   } );
  return item == FConfig.end() ? -1 : static_cast<int>( std::distance( FConfig.begin(), item ) );
 }

void TIHardware::TImplementation::ConsoleLoop(void)
 {
  TGNString line;
  while( std::getline( std::cin, line ) )
   {
    std::istringstream input( line );
    TGNString command;
    TGNString channelText;
    unsigned durationMs = 0;
    input >> command >> channelText >> durationMs;
    if( command != "press" || channelText.empty() || durationMs == 0 )
     {
      std::cerr << "Usage: press <ChannelName|ChannelId> <milliseconds>\n";
      continue;
     }
    const int channel = FindChannel( channelText );
    if( channel < 0 || !FConfig[ channel ].InputPin )
     {
      std::cerr << "Invalid input channel: " << channelText << '\n';
      continue;
     }
     {
      std::lock_guard<std::mutex> lock( FStateMutex );
      FInputStates[ channel ] = 1;
     }
    std::cout << TimeStamp() << " Press " << FConfig[ channel ].Id << ": " << durationMs << " ms\n";
    std::cout.flush();
    std::this_thread::sleep_for( std::chrono::milliseconds( durationMs ) );
     {
      std::lock_guard<std::mutex> lock( FStateMutex );
      FInputStates[ channel ] = 0;
     }
   }
 }

bool TIHardware::TImplementation::Setup( const TChannelConfigs & channels, TGNString & error )
 {
  FConfig = channels;
  FOutputStates.assign( channels.size(), 0 );
  FInputStates.assign( channels.size(), 0 );
  std::thread( [this](void) { ConsoleLoop(); } ).detach();
  error.clear();
  return true;
 }

TIHardware::TIHardware(void)
: FImplementation( std::make_unique<TImplementation>() )
 {
 }

TIHardware::~TIHardware(void) = default;

bool TIHardware::Setup( const TChannelConfigs & channels, TGNString & error )
 {
  return FImplementation->Setup( channels, error );
 }

bool TIHardware::SetPin( unsigned channelNr, bool on, TGNString & error, bool firstTime )
 {
  return FImplementation->SetPin( channelNr, on, error, firstTime );
 }

bool TIHardware::GetPin( unsigned channelNr, bool & active, TGNString & error )
 {
  return FImplementation->GetPin( channelNr, active, error );
 }

bool TIHardware::GetOutputState( unsigned channelNr, bool & on, TGNString & error )
 {
  return FImplementation->GetOutputState( channelNr, on, error );
 }

bool TIHardware::RequestShutdown( TGNString & error )
 {
  return FImplementation->RequestShutdown( error );
 }

bool TIHardware::CancelShutdown( TGNString & error )
 {
  return FImplementation->CancelShutdown( error );
 }

void TIHardware::Loop(void)
 {
  FImplementation->Loop();
 }

void TIHardware::EnableRuntimeOutput(void)
 {
  FImplementation->EnableRuntimeOutput();
 }
#endif
