// TmrSwA8THardware.h, Version: 1.02
#pragma once

#define TmrSwA8THardware_A8T       0
#define TmrSwA8THardware_TestNode  1

#ifndef TmrSwA8THardwareSelect
  #define TmrSwA8THardwareSelect TmrSwA8THardware_A8T
#endif

namespace TmrSwA8THardware
 {
constexpr int KeyPin = 4;
constexpr int KeyActiveLevel = LOW;
constexpr int RelayPin = 13;
constexpr int RelayActiveLevel = HIGH;
constexpr int StatusLedPin = 2;

#if TmrSwA8THardwareSelect == TmrSwA8THardware_A8T
  constexpr int StatusLedActiveLevel = LOW;
  constexpr bool PowerMeterAvailable = true;
  constexpr int MeterSelPin = 14;       // A8T selector polarity retained from the calibrated code: HIGH = voltage
  constexpr int MeterCf1Pin = 26;       // HLWBL CF1
  constexpr int MeterCfPin = 27;        // BL0937/HJL-01 CF power
 #elif TmrSwA8THardwareSelect == TmrSwA8THardware_TestNode
  constexpr int StatusLedActiveLevel = HIGH;
  constexpr bool PowerMeterAvailable = false;
  constexpr int MeterSelPin = -1;
  constexpr int MeterCf1Pin = -1;
  constexpr int MeterCfPin = -1;
 #else
  #error Invalid TmrSwA8THardwareSelect
#endif
 }

namespace EspRescueHardware
 {
constexpr int KeyPin = TmrSwA8THardware::KeyPin;
constexpr int KeyActiveLevel = TmrSwA8THardware::KeyActiveLevel;
constexpr int StatusLedPin = TmrSwA8THardware::StatusLedPin;
constexpr int StatusLedActiveLevel = TmrSwA8THardware::StatusLedActiveLevel;
 }
