// HardwareRasPi.cpp, Version: 2.32
#include <GnBasics.h>
#include "BuildConfig.h"
#if defined( SYSTEM_RASPI )
  #include "IHardware.h"
  #include <cstdlib>
  #include <gpiod.h>
  #include <mutex>
  #include <vector>

class TIHardware::TImplementation
 {
 public:
                                    TImplementation(void) = default;
                                   ~TImplementation(void);
  bool                              Setup( const TChannelConfigs & channels, TGNString & error );
  bool                              SetPin( unsigned channelNr, bool on, TGNString & error, bool firstTime );
  bool                              GetPin( unsigned channelNr, bool & active, TGNString & error );
  bool                              GetOutputState( unsigned channelNr, bool & on, TGNString & error );
  bool                              RequestShutdown( TGNString & error );
  bool                              CancelShutdown( TGNString & error );
  void                              Loop(void) {}
  void                              EnableRuntimeOutput(void) {}

 private:
  static gpiod_line_request *       RequestLine( unsigned offset, gpiod_line_direction direction,
                                                 gpiod_line_value outputValue, bool inputPullup );
  static void                       ReleaseLines( std::vector<gpiod_line_request *> & lines );

  std::vector<gpiod_line_request *> FOutputRequests;
  std::vector<gpiod_line_request *> FInputRequests;
  TChannelConfigs                   FConfig;
  std::vector<bool>                 FOutputStates;
  std::mutex                        FStateMutex;
 };

TIHardware::TImplementation::~TImplementation(void)
 {
  ReleaseLines( FOutputRequests );
  ReleaseLines( FInputRequests );
 }

gpiod_line_request * TIHardware::TImplementation::RequestLine( unsigned offset,
                                                               gpiod_line_direction direction,
                                                               gpiod_line_value outputValue,
                                                               bool inputPullup )
 {
  gpiod_chip * chip = gpiod_chip_open( "/dev/gpiochip0" );
  if( !chip )
    return nullptr;

  gpiod_line_settings * settings = gpiod_line_settings_new();
  gpiod_line_config * lineConfig = gpiod_line_config_new();
  gpiod_request_config * requestConfig = gpiod_request_config_new();
  gpiod_line_request * request = nullptr;

  if( settings && lineConfig && requestConfig &&
      gpiod_line_settings_set_direction( settings, direction ) == 0 &&
      ( direction != GPIOD_LINE_DIRECTION_INPUT ||
        gpiod_line_settings_set_bias( settings, inputPullup ? GPIOD_LINE_BIAS_PULL_UP :
                                                              GPIOD_LINE_BIAS_DISABLED ) == 0 ) &&
      ( direction != GPIOD_LINE_DIRECTION_OUTPUT ||
        gpiod_line_settings_set_output_value( settings, outputValue ) == 0 ) &&
      gpiod_line_config_add_line_settings( lineConfig, &offset, 1, settings ) == 0 )
   {
    gpiod_request_config_set_consumer( requestConfig, "MultiTimer" );
    request = gpiod_chip_request_lines( chip, requestConfig, lineConfig );
   }

  if( requestConfig )
    gpiod_request_config_free( requestConfig );
  if( lineConfig )
    gpiod_line_config_free( lineConfig );
  if( settings )
    gpiod_line_settings_free( settings );
  gpiod_chip_close( chip );
  return request;
 }

void TIHardware::TImplementation::ReleaseLines( std::vector<gpiod_line_request *> & lines )
 {
  for( gpiod_line_request * request : lines )
    if( request )
      gpiod_line_request_release( request );
  lines.clear();
 }

bool TIHardware::TImplementation::Setup( const TChannelConfigs & channels, TGNString & error )
 {
  ReleaseLines( FOutputRequests );
  ReleaseLines( FInputRequests );

  FConfig = channels;
  FOutputStates.assign( channels.size(), false );
  FOutputRequests.assign( channels.size(), nullptr );
  FInputRequests.assign( channels.size(), nullptr );

  for( size_t index = 0;  index < channels.size();  ++index )
   {
    const TChannelConfig & channel = channels[ index ];
    if( !channel.InputPin )
      continue;
    FInputRequests[ index ] = RequestLine( *channel.InputPin, GPIOD_LINE_DIRECTION_INPUT,
                                           GPIOD_LINE_VALUE_INACTIVE, channel.InputPullup );
    if( FInputRequests[ index ] )
      continue;
    error = "Cannot configure input GPIO for " + channel.Id;
    ReleaseLines( FOutputRequests );
    ReleaseLines( FInputRequests );
    return false;
   }
  error.clear();
  return true;
 }

bool TIHardware::TImplementation::SetPin( unsigned channelNr, bool on, TGNString & error, bool firstTime )
 {
  std::lock_guard<std::mutex> lock( FStateMutex );
  if( channelNr >= FConfig.size() || !FConfig[ channelNr ].OutputPin )
   {
    error = "Output not available";
    return false;
   }

  const TChannelConfig & channel = FConfig[ channelNr ];
  const gpiod_line_value value = on == channel.OutputActiveHigh ?
                                 GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
  if( firstTime )
   {
    FOutputRequests[ channelNr ] = RequestLine( *channel.OutputPin, GPIOD_LINE_DIRECTION_OUTPUT, value, false );
    if( !FOutputRequests[ channelNr ] )
     {
      error = "Cannot configure output GPIO for " + channel.Id;
      return false;
     }
   }
  else
   {
    if( !FOutputRequests[ channelNr ] )
     {
      error = "Output not initialized";
      return false;
     }
    if( gpiod_line_request_set_value( FOutputRequests[ channelNr ], *channel.OutputPin, value ) < 0 )
     {
      error = "Cannot write output for " + channel.Id;
      return false;
     }
   }
  FOutputStates[ channelNr ] = on;
  error.clear();
  return true;
 }

bool TIHardware::TImplementation::GetPin( unsigned channelNr, bool & active, TGNString & error )
 {
  std::lock_guard<std::mutex> lock( FStateMutex );
  if( channelNr >= FConfig.size() || !FInputRequests[ channelNr ] )
   {
    error = "Input not available";
    return false;
   }

  const TChannelConfig & channel = FConfig[ channelNr ];
  const gpiod_line_value value = gpiod_line_request_get_value( FInputRequests[ channelNr ],
                                                               *channel.InputPin );
  if( value != GPIOD_LINE_VALUE_INACTIVE && value != GPIOD_LINE_VALUE_ACTIVE )
   {
    error = "Cannot read input for " + channel.Id;
    return false;
   }
  active = ( value == GPIOD_LINE_VALUE_ACTIVE ) == channel.InputActiveHigh;
  error.clear();
  return true;
 }

bool TIHardware::TImplementation::GetOutputState( unsigned channelNr, bool & on, TGNString & error )
 {
  std::lock_guard<std::mutex> lock( FStateMutex );
  if( channelNr >= FConfig.size() || !FOutputRequests[ channelNr ] )
   {
    error = "Output not available";
    return false;
   }
  on = FOutputStates[ channelNr ];
  error.clear();
  return true;
 }

bool TIHardware::TImplementation::RequestShutdown( TGNString & error )
 {
  if( std::system( "shutdown -P +1" ) != 0 )
   {
    error = "Cannot request system shutdown";
    return false;
   }
  error.clear();
  return true;
 }

bool TIHardware::TImplementation::CancelShutdown( TGNString & error )
 {
  if( std::system( "shutdown -c" ) != 0 )
   {
    error = "Cannot cancel system shutdown";
    return false;
   }
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
