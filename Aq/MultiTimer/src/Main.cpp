// Main.cpp, Version: 2.54
#include <GnBasics.h>
#include <GnUdpStatus.h>
#include <GnDeviceIdentity.h>
#include "BuildConfig.h"
#include "ButtonController.h"
#include "ConfigStore.h"
#include "ConfigurationTransaction.h"
#if defined( SYSTEM_ESP )
  #include "EspConfig.h"
  #include <Arduino.h>
#endif
#include "IHardware.h"
#include "IWebServer.h"
#include "Logger.h"
#include "Scheduler.h"
#include "TimeSync.h"
#include "TimeConfigSync.h"
#include "WebApplication.h"
#include "WebSocketServer.h"
#if !defined( SYSTEM_ESP )
  #include <filesystem>
  #include <iostream>
  #include <json-c/json.h>
  #include <unistd.h>
#endif
#include <memory>
#include <utility>

class TMultiTimerApplication : public TSingleton<TMultiTimerApplication>
 {
  friend class TSingleton<TMultiTimerApplication>;

 private:
                                     TMultiTimerApplication(void) = default;
  static int                         OptionIndex( int argc, char ** argv, const TGNString & option );
  static TGNString                   DataDir( const char * argv0 );
  static TGNString                   ChannelsFile( const TGNString & dataDir );
  static TGNString                   ScheduleFile( const TGNString & dataDir );
  static TGNString                   LogFile( const char * argv0 );
  static TGNString                   DeviceId(void);
  static bool                        DeviceName( const char * argv0, TGNString & name, TGNString & error );
  static unsigned                    WebPort( int argc, char ** argv );
  #if !defined( SYSTEM_ESP )
    static std::filesystem::path     ExecutableDir( const char * argv0 );
  #endif

 public:
  bool                               Setup( int argc, char ** argv, TGNString & error );
  bool                               Ready(void) const { return FReady; }
  #if !defined( SYSTEM_ESP )
    bool                             ExitRequested(void) const { return FApplication && FApplication->ExitRequested(); }
  #endif

 private:
  TChannelConfigs                    FChannels;
  THeaterProfiles                    FHeaterProfiles;
  TSwitchTimes                       FSwitchTimes;
  TSwitchIntervals                   FIntervals;
  std::unique_ptr<TScheduler>        FScheduler;
  std::unique_ptr<TButtonController> FButtons;
  std::unique_ptr<TStatusApi>        FStatusApi;
  std::unique_ptr<TTimeConfigSync>   FTimeConfigSync;
  std::unique_ptr<TWebApplication>   FApplication;
  bool                               FReady = false;
 };
#define MultiTimerApplication TMultiTimerApplication::Instance()

int TMultiTimerApplication::OptionIndex( int argc, char ** argv, const TGNString & option )
 {
  for( int index = 1;  index < argc;  ++index )
    if( argv[ index ] == option )
      return index;
  return -1;
 }

#if defined( SYSTEM_ESP )
TGNString TMultiTimerApplication::DataDir( const char * )
 {
  return "/";
 }

TGNString TMultiTimerApplication::ChannelsFile( const TGNString & )
 {
  return "/Channels.json";
 }

TGNString TMultiTimerApplication::ScheduleFile( const TGNString & )
 {
  return "/EspSwitchTimes.json";
 }

TGNString TMultiTimerApplication::LogFile( const char * )
 {
  return "/MultiTimer.log";
 }

TGNString TMultiTimerApplication::DeviceId(void)
 {
  return EspConfig.Hostname;
 }

bool TMultiTimerApplication::DeviceName( const char *, TGNString & name, TGNString & error )
 {
  error.clear();
  name = EspConfig.Hostname;
  return true;
 }

unsigned TMultiTimerApplication::WebPort( int, char ** )
 {
  return 8080;
 }
#else
std::filesystem::path TMultiTimerApplication::ExecutableDir( const char * argv0 )
 {
  std::error_code error;
  std::filesystem::path executable = std::filesystem::canonical( "/proc/self/exe", error );
  if( error )
    executable = std::filesystem::absolute( argv0, error );
  return executable.parent_path();
 }

TGNString TMultiTimerApplication::DataDir( const char * argv0 )
 {
  return ( ExecutableDir( argv0 ) / "data" ).string();
 }

TGNString TMultiTimerApplication::ChannelsFile( const TGNString & dataDir )
 {
  return dataDir + "/Channels.json";
 }

TGNString TMultiTimerApplication::ScheduleFile( const TGNString & dataDir )
 {
  return dataDir + "/SwitchTimes.json";
 }

TGNString TMultiTimerApplication::LogFile( const char * argv0 )
 {
  std::error_code error;
  const std::filesystem::path logDir = ExecutableDir( argv0 ) / "Log";
  std::filesystem::create_directories( logDir, error );
  return ( logDir / "MultiTimer.log" ).string();
 }

TGNString TMultiTimerApplication::DeviceId(void)
 {
  // Bootstrap function label, never a hostname-based wire identity.
  return "GN-Pi";
 }

bool TMultiTimerApplication::DeviceName( const char * argv0, TGNString & name, TGNString & error )
 {
  error.clear();
  const std::filesystem::path configFile = ExecutableDir( argv0 ) / "Config.json";
  json_object * root = json_object_from_file( configFile.c_str() );
  json_object * value = nullptr;
  if( root == nullptr || !json_object_is_type( root, json_type_object ) ||
      !json_object_object_get_ex( root, "Name", &value ) || !json_object_is_type( value, json_type_string ) ||
      json_object_get_string( value ) == nullptr || json_object_get_string( value )[ 0 ] == '\0' )
   {
    if( root != nullptr )
      json_object_put( root );
    error = "Invalid Config.json: Name missing";
    return false;
   }
  name = json_object_get_string( value );
  json_object_put( root );
  return true;
 }

unsigned TMultiTimerApplication::WebPort( int argc, char ** argv )
 {
  const int index = OptionIndex( argc, argv, "--port" );
  if( index >= 0 && index + 1 < argc )
    return static_cast<unsigned>( std::stoul( argv[ index + 1 ] ) );
  return 8080;
 }
#endif

bool TMultiTimerApplication::Setup( int argc, char ** argv, TGNString & error )
 {
  if( FReady )
    return true;

  const char * argv0 = argc > 0 && argv != nullptr ? argv[ 0 ] : nullptr;
  #if defined( SYSTEM_ESP )
    if( !EspConfigStore.Begin( error ) )
      return false;
  #endif
  TTimeSyncState timeSyncState = TTimeSyncState::NotSynchronized;
  if( !TTimeSync::Wait( timeSyncState ) )
   {
    error = TTimeSync::Error( timeSyncState );
    return false;
   }

  const TGNString dataDir = DataDir( argv0 );
  const TGNString deviceId = DeviceId();
  GnDeviceIdentity.Setup( deviceId, 1, dataDir + "/Identity.json" );
  TGNString deviceName;
  if( !DeviceName( argv0, deviceName, error ) )
   {
    Logger.Init( LogFile( argv0 ) );
    Logger.Write( TLogLevel::Error, error );
    return false;
   }
  Logger.Init( LogFile( argv0 ) );
  if( OptionIndex( argc, argv, "--ReStart" ) >= 0 )
    Logger.Write( TLogLevel::Info, "MultiTimer restarted by watchdog" );
  else if( OptionIndex( argc, argv, "--log-start" ) >= 0 )
    Logger.Write( TLogLevel::Info, "MultiTimer started" );

  const unsigned webPort = WebPort( argc, argv );
  if( webPort >= 65535 )
   {
    error = "Web port must be lower than 65535 because WebSocket uses the following port";
    Logger.Write( TLogLevel::Error, error );
    return false;
   }
  const unsigned webSocketPort = webPort + 1;
  #if !defined( SYSTEM_ESP )
    if( !ConfigurationTransaction::Recover( dataDir, error ) )
     { Logger.Write( TLogLevel::Error, error ); return false; }
  #endif
  std::vector<TGNString> configErrors;
  if( !ConfigStore.LoadChannels( ChannelsFile( dataDir ), FChannels, FHeaterProfiles, error ) )
   {
    const TGNString configError = "Channels.json: " + error;
    Logger.Write( TLogLevel::Error, configError );
    configErrors.push_back( configError );
    FChannels.clear();
    FHeaterProfiles.clear();
   }
  else
   {
    if( !ConfigStore.LoadSchedule( ScheduleFile( dataDir ), FChannels, FSwitchTimes, FIntervals, configErrors, error ) )
     {
      const TGNString configError = "Schedule: " + error;
      Logger.Write( TLogLevel::Error, configError );
      configErrors.push_back( configError );
      FSwitchTimes.clear();
      FIntervals.clear();
     }
    else
     {
      for( const TGNString & configError : configErrors )
        Logger.Write( TLogLevel::Info, configError );
      if( !ConfigStore.CheckScheduleHeapSpace( FChannels, FSwitchTimes, FIntervals, error ) )
       {
        Logger.Write( TLogLevel::Error, error );
        return false;
       }
     }
   }
  error.clear();
  #if defined( SYSTEM_ESP )
    if( FChannels.size() > MaxEspChannels )
     {
      error = "ESP channel limit exceeded";
      Logger.Write( TLogLevel::Error, error );
      return false;
     }
  #endif

  FScheduler = std::make_unique<TScheduler>( IHardware, FChannels, FHeaterProfiles );
  FScheduler->SetSchedule( FSwitchTimes, FIntervals );
  if( !IHardware.Setup( FChannels, error ) )
   {
    Logger.Write( TLogLevel::Error, error );
    return false;
   }
  if( !FScheduler->ReconstructState( error, true ) )
   {
    Logger.Write( TLogLevel::Error, error );
    return false;
   }
  #if defined( SYSTEM_DUMMY )
    std::cout << "Initial output states:\n";
    for( unsigned channelNr = 0;  channelNr < FChannels.size();  ++channelNr )
     {
      if( !FChannels[ channelNr ].OutputPin )
        continue;
      bool on = false;
      if( IHardware.GetOutputState( channelNr, on, error ) )
        std::cout << "  " << FChannels[ channelNr ].Id << ": " << ( on ? "ON" : "OFF" ) << '\n';
      else
        std::cerr << "  " << FChannels[ channelNr ].Id << ": " << error << '\n';
     }
    std::cout.flush();
    IHardware.EnableRuntimeOutput();
  #endif

  FButtons = std::make_unique<TButtonController>( IHardware, *FScheduler, FChannels );
  FStatusApi = std::make_unique<TStatusApi>( IHardware, *FScheduler, FChannels, deviceId, deviceName,
                                               webSocketPort, std::move( configErrors ) );
  FTimeConfigSync = std::make_unique<TTimeConfigSync>( deviceId, dataDir );
  FApplication = std::make_unique<TWebApplication>( *FScheduler, *FButtons, *FStatusApi, FChannels,
                                                    FSwitchTimes, FIntervals, dataDir, *FTimeConfigSync );
  FTimeConfigSync->Application( FApplication.get() );
  if( !IWebServer.Setup( webPort, *FApplication, error ) )
   {
    Logger.Write( TLogLevel::Error, error );
    return false;
   }
  if( !WebSocketServer.Setup( webSocketPort, *FStatusApi, error ) )
   {
    Logger.Write( TLogLevel::Error, error );
    return false;
   }

  if( !GnUdpStatus.Setup( deviceId,
       [this]( uint64_t & Signature ) { return FStatusApi->StatusJson( Signature ); } ) )
    Logger.Write( TLogLevel::Error, GnUdpStatus.Error() );

  FReady = true;
  return true;
 }

#if defined( SYSTEM_ESP )
void setup(void)
 {
  Serial.begin( 115200 );
  DelayMs( 100 );

  TGNString error;
  if( !MultiTimerApplication.Setup( 0, nullptr, error ) )
   {
    Serial.print( "MultiTimer setup failed: " );
    Serial.println( error.c_str() );
   }
 }

void loop(void)
 {
  if( MultiTimerApplication.Ready() )
    LoopInstances.Loop();
  DelayMs( 50 );
 }
#else
int main( int argc, char ** argv )
 {
  TGNString error;
  if( !MultiTimerApplication.Setup( argc, argv, error ) )
   {
    std::cerr << error << '\n';
    return 1;
   }

  while( !MultiTimerApplication.ExitRequested() )
   {
    LoopInstances.Loop();
    DelayMs( 50 );
   }
  Logger.Write( TLogLevel::Info, "MultiTimer exiting for systemd restart" );
 }
#endif
