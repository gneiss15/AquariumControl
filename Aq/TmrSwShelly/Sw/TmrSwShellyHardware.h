// TmrSwShellyHardware.h, Version: 1.01
#pragma once
namespace TmrSwShellyHardware
 {
constexpr int KeyPin = 7;
constexpr int KeyActiveLevel = LOW;
// Exact PlugMG3 2.0.0: output constructor at 0x42008DEA uses pin 0, active level 1.
constexpr bool RelayConfirmed = true;
constexpr int RelayPin = 0;
constexpr int RelayActiveLevel = HIGH;
// Exact Stock PWM RGB constructor: red 18, green 10, blue 19; inverted duty.
constexpr bool StatusLedConfirmed = true;
constexpr int RedLedPin = 18;
constexpr int BlueLedPin = 19;
constexpr int StatusLedPin = 10;
constexpr int StatusLedOnDuty = 223;
constexpr int StatusLedOffDuty = 255;
constexpr int StatusLedActiveLevel = LOW;
 }
