// TmrSwA8TPowerMeter.h, Version: 1.06
#pragma once

#include <Arduino.h>
#include <GnBasics.h>
#include <GnPowerMeterCycle.h>

class TTmrSwA8TPowerMeter : public TSingleton<TTmrSwA8TPowerMeter>, TLoopInstance
 {
  friend class TSingleton<TTmrSwA8TPowerMeter>;
 private:
                      TTmrSwA8TPowerMeter(void);

  bool                FHardwareReady;
  bool                FLoadEnabled;
  TGnPowerMeterCycle   FCycle;
  uint32_t            FVoltageIntervals;
  uint32_t            FCurrentIntervals;
  uint32_t            FVoltageCal;
  uint32_t            FCurrentCal;
  uint32_t            FPowerCal;

 protected:
  virtual void        Loop(void);

 private:
  void                LoadCalValues(void);

 public:
  void                Setup(void);
  void                LoadEnabled( bool Enabled );
  bool                Available(void) const;
  bool                HardwareReady(void) const                      { return FHardwareReady; }
  bool                Valid(void) const                              { return Available() && FHardwareReady && FCycle.Valid( millis() ); }
  uint32_t            AgeMs(void) const                             { return FCycle.AgeMs(millis()); }
  uint32_t            CfEdges(void) const;
  uint32_t            Cf1Edges(void) const;
  uint32_t            VoltageIntervals(void) const                   { return FVoltageIntervals; }
  uint32_t            CurrentIntervals(void) const                   { return FCurrentIntervals; }
  int                 SelectorLevel(void) const;
  bool                LoadEnabled(void) const                        { return FLoadEnabled; }
  float               Voltage(void) const                            { return Available() ? FCycle.Sample().Voltage : 0.0f; }
  float               Current(void) const                            { return Available() ? FCycle.Sample().Current : 0.0f; }
  float               ActivePower(void) const                        { return Available() ? FCycle.Sample().Power : 0.0f; }
  uint32_t            VoltageCal(void) const                          { return FVoltageCal; }
  uint32_t            CurrentCal(void) const                          { return FCurrentCal; }
  uint32_t            PowerCal(void) const                            { return FPowerCal; }
  bool                SetCalValues( uint32_t VoltageCal, uint32_t CurrentCal, uint32_t PowerCal );
 };

#define TmrSwA8TPowerMeter TTmrSwA8TPowerMeter::Instance()
