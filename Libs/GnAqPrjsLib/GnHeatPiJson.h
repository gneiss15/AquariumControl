// GnHeatPiJson.h, Version: 1.03
#pragma once
#include <GnBasics.h>
#include <GnProfileModel.h>

inline bool GnParseHeatPi( JsonVariant Value, TGnHeatPiConfig & Config )
 {
  if( Value.isUnbound() )return true;
  TJsonHeatConfig Contract;
  TGNString Error;
  if( !GnRead( TGnWireValue( Value ), Contract, Error ) )return false;
  return GnApplyHeatConfig( Contract, Config );
 }
