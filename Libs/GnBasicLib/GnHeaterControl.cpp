// GnHeaterControl.cpp, Version: 1.03
#include <GnBasics.h>
#include "GnHeaterControl.h"
#include <GnUdpStatus.h>
#include <GnDeviceIdentity.h>
#include <algorithm>

TGnHeaterControl::TGnHeaterControl( const THeaterProfiles & Profiles )
: FProfiles( &Profiles )
 {
 }

const THeaterProfile * TGnHeaterControl::FindProfile( const THeaterProfiles & Profiles, const TGNString & Id )
 {
  const auto Profile = std::find_if( Profiles.begin(), Profiles.end(), [&]( const THeaterProfile & Item )
   {
    return Item.Id == Id;
   } );
  return Profile == Profiles.end() ? nullptr : &( *Profile );
 }

bool TGnHeaterControl::ParseRole( const TGNString & Text, THeaterRole & Role )
 {
  if( Text == "Timer" )
    Role = THeaterRole::Timer;
  else if( Text == "HeaterMain" )
    Role = THeaterRole::HeaterMain;
  else if( Text == "HeaterFloor" )
    Role = THeaterRole::HeaterFloor;
  else
    return false;
  return true;
 }

const char * TGnHeaterControl::RoleName( THeaterRole Role )
 {
  switch( Role )
   {
    case THeaterRole::Timer:
      return "Timer";
    case THeaterRole::HeaterMain:
      return "HeaterMain";
    case THeaterRole::HeaterFloor:
      return "HeaterFloor";
   }
  return "Timer";
 }

bool TGnHeaterControl::AutomaticState( THeaterRole Role, const TGNString & ProfileId,
                                       bool SchedulePresent, bool ScheduleState,
                                       bool & HysteresisStateKnown, bool & HysteresisState ) const
 {
  if( Role == THeaterRole::Timer )
    return ScheduleState;

  if( SchedulePresent && !ScheduleState )
    return false;

  const THeaterProfile * Profile = FProfiles == nullptr ? nullptr : FindProfile( *FProfiles, ProfileId );
  if( Profile == nullptr )
    return false;

  float Temperature = 0.0F;
  if( !GnUdpStatus.GetMeasurement( GnDeviceIdentity.ResolveTemperatureSource( Profile->TempId ), Profile->Measurement, Temperature ) )
    return Profile->NoTempOn;

  const bool Floor = Role == THeaterRole::HeaterFloor;
  const float OnTemperature = Floor ? Profile->FloorOn : Profile->MainOn;
  const float OffTemperature = Floor ? Profile->FloorOff : Profile->MainOff;

  if( Temperature <= OnTemperature )
   {
    HysteresisState = true;
    HysteresisStateKnown = true;
   }
  else if( Temperature >= OffTemperature )
   {
    HysteresisState = false;
    HysteresisStateKnown = true;
   }
  else if( !HysteresisStateKnown )
   {
    HysteresisState = SchedulePresent ? ScheduleState : Profile->NoTempOn;
    HysteresisStateKnown = true;
   }

  return HysteresisState;
 }
