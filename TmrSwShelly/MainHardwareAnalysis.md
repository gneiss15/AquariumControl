<!-- MainHardwareAnalysis.md, Version: 1.04 -->
# Main hardware evidence

Exact captured Stock PlugMG3 2.0.0 image:
generated/History/ShellyMigration/ShellyReadout/captures/20261003-V2-firstboot/stock-app0.bin.
This is read-only disassembly evidence, not a physical relay test.

- Key GPIO7 active-low with INPUT_PULLUP: see ShellyMigration/ButtonAnalysis.md inside Old.zip;
  the user physically confirmed key changes in both input modes.
- The single switch construction at 0x42008D4A allocates a 48-byte GPIO output
  object. At 0x42008DE2..0x42008DEA it passes output ID 0, GPIO 0 and active level
  1 into 0x420AD91A. That constructor stores a2 as the physical pin at +40 and
  a3 bit 0 as active level at +44 (0x420AD986..0x420AD994).
- Its output writer at 0x420AD7EE reads the same +40 pin, configures it as output
  using 0x40381F9E with mode 1 and writes the configured/inverted active level
  through 0x403821BA. The ON branch uses the active bit, OFF inverts it.
  Thus Main uses relay GPIO0 active-high; setup initializes OFF before OUTPUT.
- RGB construction at 0x42008F82..0x4200900A passes red 18, green 10,
  blue 19 to 0x4216B37C. PWM at 0x4202A7B6 uses inverted duty: active LOW.
  Main selects blue during WLAN/NTP waits and green for normal relay indication;
  native Rescue uses red for its unchanged SOS timing.
- BL0942 construction at 0x42009122..0x4200912C selects UART1, RX5/TX4;
  UART initialization uses 9600 baud. Stock register initialization and 23-byte
  checksummed packets establish the target driver protocol.
- Factory calibration is JSON at offset 0x1000 in the preserved shelly data
  partition (subtype 0x88, 0x7F0000/0x10000). Stock multiplies voltage_scale,
  current_scale and apower_scale by 1e-11 before applying raw readings.
  Main reads these device-specific values; missing or unsupported calibration
  disables measurements. The calibration endpoint can overwrite only these three
  scales after explicit confirmation. It retains `current_gain` and other JSON
  fields, verifies the full 4-KiB sector and attempts an immediate restore after
  a failed write.
- Main Shelly1 was actually started and reported valid BL0942 packets/readings
  around 225 V, 0 A and 0 W with relay OFF. Main-to-Rescue and back also passed.
  Physical LED/button/relay effects and measurement accuracy against an external
  meter remain untested.

The main application is built for ESP32-C3 revision 4 / 8 MiB / SDK table 0x10000,
Main 0x3A0000/0x2A0000, LittleFS 0x2C0000/0xE0000, Rescue 0x640000/0x180000,
NVS 0x7C0000/0x20000 and standard otadata 0x7E0000/0x2000.
