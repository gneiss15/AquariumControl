// TmrSwHw.h, Version: 3.04
#ifndef TmrSwHwH
#define TmrSwHwH

#include <GnBasics.h>
#include <GnCoopTimer.h>
#include "TmrSwIncludes.h"
#include "TmrSwConfig.h"
#include "TmrSwShellyHardware.h"

class TTmrSwHw : public TSingleton<TTmrSwHw>, TLoopInstance
 {
  friend class TSingleton<TTmrSwHw>;
 private:
                      TTmrSwHw(void);
  bool                FRelayStatus;
  bool                FRelayStatusKnown;
  bool                FStatusLedReady = false;
  bool                FStatusLedWaiting = true;
  bool                FWiFiLedStatus;
  bool                FPwrLedStatus;
  bool                FInpStatus;
  TGnElapsedTimerMs   FButtonPressTimer;
 protected:
  virtual void        Loop(void);
 private:
  bool                HandleShortButtonPress( bool Simulated );
 public:
  void                Setup(void);
  bool                HardwareReady(void) const                      { return TmrSwShellyHardware::RelayConfirmed; }
  bool                HasInput(void) const                           { return true; }
  bool                HasOutput(void) const                          { return TmrSwShellyHardware::RelayConfirmed; }
  bool                PowerMeterValid(void) const;
  uint32_t            PowerMeterCfEdges(void) const;
  uint32_t            PowerMeterCf1Edges(void) const;
  uint32_t            PowerMeterVoltageIntervals(void) const;
  uint32_t            PowerMeterCurrentIntervals(void) const;
  int                 PowerMeterSelectorLevel(void) const;
  bool                PowerMeterLoadEnabled(void) const;
  float               Voltage(void) const;
  float               Current(void) const;
  float               ActivePower(void) const;
  void                RelayOff(void);
  void                RelayOn(void);
  void                RelayToggle(void);
  bool                SimulateShortButtonPress(void);
  void                RelaySet( bool On );
  bool                RelayStatus(void) const                        { return FRelayStatus; }
  bool                RelayStatusKnown(void) const                   { return FRelayStatusKnown; }
  bool                ButtonPressed(void) const;
  void                StatusLedWaiting( bool Waiting );
  void                WiFiLedOff(void);
  void                WiFiLedOn(void);
  void                WiFiLedToggle(void);
  void                PwrLedOff(void);
  void                PwrLedOn(void);
  void                PwrLedToggle(void);
 };
#define TmrSwHw TTmrSwHw::Instance()

#endif
