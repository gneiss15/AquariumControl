// ButtonController.cpp, Version: 2.43
#include <GnBasics.h>
#include "ButtonController.h"
//?#include <chrono>

namespace
 {
constexpr TGnCoopTimer_t MinimumPressMs = 500;
constexpr TGnCoopTimer_t Channel0LongPressMs = 5000;
constexpr TGnCoopTimer_t OtherLongPressMs = 5000;
constexpr auto TemporaryToggleDuration = std::chrono::minutes( 10 );
 } // namespace

TButtonController::TButtonController( TIHardware & hardware, TScheduler & timer, const TChannelConfigs & channels )
: Hardware( hardware )
, Timer( timer )
, Channels( channels )
, States( channels.size() )
 {
  States[ 0 ].LongPressMs = Channel0LongPressMs;
  for( size_t i = 1;  i < channels.size();  ++i )
    States[ i ].LongPressMs = OtherLongPressMs;
 }

bool TButtonController::ValidateActionChannel( unsigned channelNr, TGNString & error ) const
 {
  if ( channelNr >= Channels.size() )
   {
    error = "Invalid channel number";
    return false;
   }
  if ( !Channels[channelNr].HasOutput() )
   {
    error = "Channel has no output: " + Channels[channelNr].Id;
    return false;
   }
  error.clear();
  return true;
 }

void TButtonController::HandlePressStartEffect(void)
 {
  if ( !ShutdownPending )
    return;

  TGNString error;
  if ( Hardware.CancelShutdown( error ) )
   {
    ShutdownPending = false;
    Logger.Write( TLogLevel::Info, "Pending shutdown cancelled by button" );
   }
   else
   {
    Logger.Write( TLogLevel::Error, error );
   }
 }

bool TButtonController::ExecuteShortPress( unsigned channelNr, TGNString & error )
 {
  return Timer.ToggleChannel( static_cast<TChannelId>( channelNr ), error );
 }

bool TButtonController::ExecuteLongPress( unsigned channelNr, TGNString & error )
 {
  if ( channelNr == 0 )
   {
    if ( Hardware.RequestShutdown( error ) )
     {
      ShutdownPending = true;
      Logger.Write( TLogLevel::Info, "Shutdown requested by channel 0 long press" );
      error.clear();
      return true;
     }
    return false;
   }

  return Timer.ToggleChannel( static_cast<TChannelId>( channelNr ), TemporaryToggleDuration, error );
 }

bool TButtonController::SimulatePress( unsigned channelNr, bool longPress, TGNString & error )
 {
  if ( !ValidateActionChannel( channelNr, error ) )
    return false;

  HandlePressStartEffect();
  const bool success = longPress ? ExecuteLongPress( channelNr, error ) : ExecuteShortPress( channelNr, error );
  if ( !success )
   {
    Logger.Write( TLogLevel::Error, error );
    return false;
   }

  Logger.Write( TLogLevel::Info, TGNString( "Simulated " ) + ( longPress ? "long" : "short" ) +
                        " button press: " + Channels[channelNr].Id );
  error.clear();
  return true;
 }

void TButtonController::HandlePressed( unsigned channelNr )
 {
  TButtonState & state = States[channelNr];
  state.PressTimer.Start();
  state.LongPressHandled = false;
  HandlePressStartEffect();
 }

void TButtonController::HandleReleased( unsigned channelNr, TGnCoopTimer_t pressDurationMs )
 {
  TButtonState & state = States[ channelNr ];
  if ( state.LongPressHandled || pressDurationMs < MinimumPressMs || pressDurationMs >= state.LongPressMs )
    return;

  TGNString error;
  if ( !ExecuteShortPress( channelNr, error ) )
    Logger.Write( TLogLevel::Error, error );
 }

void TButtonController::HandleLongPress( unsigned channelNr )
 {
  States[channelNr].LongPressHandled = true;

  TGNString error;
  if ( !ExecuteLongPress( channelNr, error ) )
    Logger.Write( TLogLevel::Error, error );
 }

void TButtonController::Loop(void)
 {
  for ( unsigned channelNr = 0; channelNr < Channels.size(); ++channelNr )
   {
    if ( !Channels[channelNr].InputPin )
      continue;

    bool pressed = false;
    TGNString error;
    if ( !Hardware.GetPin( channelNr, pressed, error ) )
     {
      Logger.Write( TLogLevel::Error, error );
      continue;
     }

    TButtonState & state = States[channelNr];
    if ( !state.Initialized )
     {
      state.Initialized = true;
      state.Pressed = pressed;
      if ( pressed )
        HandlePressed( channelNr );
      continue;
     }

    if ( pressed != state.Pressed )
     {
      state.Pressed = pressed;
      if ( pressed )
        HandlePressed( channelNr );
       else
        HandleReleased( channelNr, state.PressTimer.Elapsed() );
     }

    if ( !state.Pressed || state.LongPressHandled )
      continue;
    if ( state.PressTimer.Elapsed() >= state.LongPressMs )
      HandleLongPress( channelNr );
   }
 }
