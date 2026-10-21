<!-- README.md, Version: 1.07 -->
# AquariumControl

[Deutsch](README.DE.md)

**Work in progress. Comments, test reports and suggestions are welcome.**

Source code for a locally operated aquarium system: feeding, lighting, temperature-dependent heating, a Raspberry Pi dashboard, and PC-based history charts.

The project’s own files use the [PolyForm Noncommercial License 1.0.0](LICENSE.md). Required Notice: Copyright 2026 Günter Neiß. Existing component licenses remain unchanged; see [THIRD_PARTY.md](THIRD_PARTY.md).

## Components

- [Dashboard](Dashboard/README.md): grouped aquarium overview, schedules, heating profiles, MAC-based device assignments and function swaps.
- [MultiTimer](MultiTimer/README.md): native multi-channel timer/controller.
- [NOUS A8T](TmrSwA8T/README.md) and [Shelly](TmrSwShelly/README.md): switch firmware with power measurement and heating-profile consumers.
- [Feeder](Feeder/README.md) and [FeederSimple](FeederSimple/README.md): feeding devices and temperature publishers.
- [HeatHistory](HeatHistory/README.md): passive CSV recorder and on-demand charts, optionally as a PC user service.
- `Libs/`: shared code; `tools/`: build, synchronization and calibration tools.

Takeover/initial migration is maintained separately in [ShellyNousTakeover](https://github.com/gneiss15/ShellyNousTakeover). Compatible Main applications are maintained here in AquariumControl.

## Power-meter calibration

NOUS A8T meters can be calibrated using a Shelly as the measurement reference. The [calibration script](tools/CalibrateA8T.py) and [instructions](tools/CalibrateA8T.md) are included. The script calculates corrections from two runs with the device order reversed; it writes calibration values only to the A8T, with `--apply` and additional confirmation. The Shelly calibration remains unchanged.

## Local configuration and builds

Python utilities require Python 3.10 or later. Native builds use the project Makefiles. ESP32 and ESP8266 projects use different toolchains; consult each project's README before building. Existing local toolchain paths are configurable. Dashboard and MultiTimer dummy/native builds passed from an isolated extracted export using the installed PC toolchain. Fresh ESP builds are still pending.

The distribution contains source and hardware documentation. Device-specific `DevState` configurations, WLAN credentials, private signing keys, Raspberry Pi runtime data, captures, logs and build products are not included. Supply your own configuration and signing material outside the public repository as described in [BUILD-SECRETS.md](BUILD-SECRETS.md). **Coming soon:** a complete first-time setup guide and generic device/dashboard configuration examples. This work-in-progress repository is not yet a turnkey installation package.

## Verification status

Current operation is confirmed for the existing aquarium installation. This is not a guarantee for different hardware. Adding/removing hardware in the new device editor has local automated coverage and awaits a real-device test.

## Further hardware READMEs

Hardware descriptions and their documentation status are provided in the project documents:

- [Feeder – hardware](Feeder/Hw/README.md)
- [FeederSimple – hardware](FeederSimple/Hw/README.md)
- [MultiTimer / Dashboard – Raspberry Pi hardware](MultiTimer/Hw/README.md)

## Feedback explicitly welcome

**Please report unclear instructions, missing information, problems and test results. Experiences from successful builds and suggestions for improvement are welcome too.**

[Submit feedback through GitHub Issues](https://github.com/gneiss15/AquariumControl/issues)
