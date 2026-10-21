// HwConfig.FeederSimple.h, Version: 1.06
#pragma once

// Temporary Dallas/OneWire stability test. Remove after the hardware test.
#define FeederSimpleDallasTest  1

// Verified on the assembled feeder hardware.
#define FeederSimpleHomeActiveLow  1
#define FeederSimpleHomeUsePullup  0

// Fixed NodeMCU hardware status LED (D7 / GPIO13), active high.
#define StatusLedPin         13
#define StatusLedActiveHigh   1
