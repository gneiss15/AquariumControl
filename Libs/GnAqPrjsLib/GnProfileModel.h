// GnProfileModel.h, Version: 1.04
#pragma once
#include <GnBasics.h>
#include <GnJsonSchema.h>
#include <GnHeatPi.h>
#include <AddressResolver.h>

inline bool GnApplyHeatConfig(TJsonHeatConfig const & Source,TGnHeatPiConfig & Target)
 {
  Target={};
  if(!Source.Active.Known())return false;
  Target.Enabled=Target.Active=Source.Active.Value;
  const auto Bind=[](TGnJsonField<TChannelId> const & Input,TGNString & Device,TGNString & Channel)
   {
    if(!Input.Known())return false;
    TChannelAddress Address;
    if(!TChannelAddress::Parse(Input.Value,Address) || !Address.HasService)return false;
    // TimerSwitch has exactly one fixed hardware output. MultiTimer needs an
    // explicit channel; a registry must resolve other abbreviated addresses first.
    if(!Address.HasChannel && Address.Service!=4)return false;
    if(Address.Service==4 && Address.HasChannel && Address.Channel!=0)return false;
    Device=ServiceAddress(Address);
    Channel=std::to_string(Address.HasChannel?Address.Channel:0);
    return true;
   };
  if(Source.MainEndpoint.Known() && !Bind(Source.MainEndpoint,Target.MainDevice,Target.MainChannel))return false;
  if(Source.FloorEndpoint.Known() && !Bind(Source.FloorEndpoint,Target.FloorDevice,Target.FloorChannel))return false;
  // Partial telemetry configuration is legal; an enabled control profile is not.
  if(Target.Active && (!Source.Setpoint.Known() || !Source.FloorW.Known() ||
     !Source.MainMaxW.Known() || !Source.CapacityWhPerK.Known() ||
     !Source.LossWPerK.Known() || !Source.Kp.Known() || !Source.TiSec.Known() ||
     !Source.WindowSec.Known() || !Source.DelaySec.Known()))return false;
  if(Target.Active && (Source.MainMaxW.Value<=0 || Source.FloorW.Value<0 ||
     Source.CapacityWhPerK.Value<=0 || Source.LossWPerK.Value<0 || Source.Kp.Value<0 ||
     Source.TiSec.Value==0 || Source.WindowSec.Value==0))return false;
  Target.Setpoint=Source.Setpoint.Value;
  Target.FloorWatts=Source.FloorW.Value;
  Target.MainMaxWatts=Source.MainMaxW.Value;
  Target.CapacityWhPerK=Source.CapacityWhPerK.Value;
  Target.LossWattsPerK=Source.LossWPerK.Value;
  Target.Kp=Source.Kp.Value;
  Target.TiSeconds=Source.TiSec.Value;
  Target.WindowSeconds=Source.WindowSec.Value;
  Target.DelaySeconds=Source.DelaySec.Value;
  return !Target.Active || (!Target.MainDevice.empty() &&
    (Target.FloorWatts<=0 || !Target.FloorDevice.empty()));
 }

template<class TProfile> bool GnApplyHeatProfile(TJsonProfile const & Source,TProfile & Target)
 {
  if(!Source.ProfName.Known() || !Source.MeasSrc.Known() || !Source.OnIfNoMeas.Known())return false;
  Target.Id=Source.ProfName.Value;Target.TempId=Source.MeasSrc.Value;
  Target.NoTempOn=Source.OnIfNoMeas.Value;
  if(Source.Meas.Known())Target.Measurement=Source.Meas.Value;
  if(Target.Measurement=="BatteryPercent")
   {
    if(!Source.ChargeStartPct.Known() || !Source.ChargeStopPct.Known() ||
       Source.ChargeStartPct.Value>=Source.ChargeStopPct.Value)return false;
    Target.ChargeStartPercent=Source.ChargeStartPct.Value;
    Target.ChargeStopPercent=Source.ChargeStopPct.Value;
   }
  Target.ChargeRunOn=Source.ChargeRunOn.Known() && Source.ChargeRunOn.Value;
  if(Target.ChargeRunOn && (Target.Measurement!="BatteryPercent" ||
     !Source.ChargeSecPerPct.Known() || !Source.ChargeSecPerPct.Value))return false;
  if(Source.ChargeSecPerPct.Known())Target.ChargeSecondsPerPercent=Source.ChargeSecPerPct.Value;
  if(Source.Heat.Known())return GnApplyHeatConfig(Source.Heat.Value,Target.Pi);
  return Target.Measurement=="BatteryPercent";
 }
