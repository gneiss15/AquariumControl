// GnHeaterControl.cpp, Version: 1.11
#define Do_Device_RX
#include <GnBasics.h>
#include "GnHeaterControl.h"
#include <GnUdpStatus.h>
#include <GnDeviceIdentity.h>
#include <GnHeatPiPeer.h>
#include <algorithm>

TGnHeaterControl::TGnHeaterControl( const THeaterProfiles & Profiles )
: FProfiles( Profiles )
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
  else if( Text == "MainSw" || Text == "HeaterMain" )
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
      return "MainSw";
    case THeaterRole::HeaterFloor:
      return "HeaterFloor";
   }
  return "Timer";
 }

bool TGnHeaterControl::AutomaticState( THeaterRole Role, const TGNString & ProfileId,
                                       bool SchedulePresent, bool ScheduleState,
                                       bool & HysteresisStateKnown, bool & HysteresisState,
                                       TGnChargeRunOn * Charge, bool OutputOn, bool Automatic, TGNString const & Channel ) const
 {
  if( Role == THeaterRole::Timer )
   { if( Charge ) Charge->Reset(); return ScheduleState; }

  if( SchedulePresent && !ScheduleState )
   { if( Charge ) Charge->Reset(); return false; }

  const THeaterProfile * Profile = FindProfile( FProfiles, ProfileId );
  if( Profile == nullptr )
   { if( Charge ) Charge->Reset(); return false; }

  if( Role==THeaterRole::HeaterFloor && Profile->Pi.Active )
   {
    if( GnDeviceIdentity.WireId()!=Profile->Pi.FloorDevice || Channel!=Profile->Pi.FloorChannel )return false;
    TGNString Peer;bool On=false,AwaitingInitialStatus=false;
    if( GnUdpStatus.GetHeatPiPeer(Profile->Pi.MainDevice,Peer,AwaitingInitialStatus) &&
        GnHeatPiFloorRequest(Peer,ProfileId,Profile->Pi.FloorDevice,Channel,On) )return On;
    if(AwaitingInitialStatus)return OutputOn;
    return true; // Main unavailable: the low-power floor heater maintains minimum heat.
   }
  float Temperature = 0.0F;
  const TGNString Source = GnDeviceIdentity.ResolveTemperatureSource( Profile->TempId );
  const bool Valid = GnUdpStatus.GetMeasurement( Source, Profile->Measurement, Temperature );
  const bool Charging = Charge && Profile->ChargeRunOn && Profile->Measurement == "BatteryPercent";
  if( Charge && !Charging ) Charge->Reset();
  if( !Valid && !Charging ) return Profile->NoTempOn;

  // Water heating is handled exclusively by PI and the floor request lease.
  // The remaining hysteresis belongs to battery charging only.
  if(Profile->Measurement!="BatteryPercent")return false;
  const float OnTemperature=Profile->ChargeStartPercent;
  const float OffTemperature=Profile->ChargeStopPercent;

  if( Valid && Temperature <= OnTemperature )
   {
    HysteresisState = true;
    HysteresisStateKnown = true;
   }
  else if( Valid && Temperature >= OffTemperature )
   {
    HysteresisState = false;
    HysteresisStateKnown = true;
   }
  else if( Valid && !HysteresisStateKnown )
   {
    HysteresisState = SchedulePresent ? ScheduleState : Profile->NoTempOn;
    HysteresisStateKnown = true;
   }

  if( Charging )
   {
    const auto Learning = GnUdpStatus.ChargeLearningPolicy( Source );
    const uint32_t Age = GnUdpStatus.MeasurementAgeMs( Source, Profile->Measurement );
    return Charge->Update( Profile->Id + "\n" + Source + "\n" + RoleName( Role ) + "\n" + std::to_string( OnTemperature ),
                           OffTemperature, Profile->ChargeSecondsPerPercent, GnMillis64(),
                           Valid, Age > GnUdpTemperatureMaxAgeMs, Temperature, Age,
                           OutputOn, HysteresisState, Automatic, Learning.Allowed, Learning.BootCounter, Learning.Available );
   }
  return HysteresisState;
 }
