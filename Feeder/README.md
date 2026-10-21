<!-- Feeder/README.md, Version: 1.27 -->
# Feeder

[Deutsch](README.DE.md)

Feeder is a DIY automatic fish feeder with an OLED display and rotary encoder.

Scheduling, configuration, Wi-Fi/web server, status API, temperature sensing and WebSocket support are shared with `FeederSimple` through `../Libs/Common/Feeder`. Servo, OLED and rotary-encoder implementations remain specific to this project.

The shared web interface is in `../Libs/Common/Feeder/WebSrc`. `ServoSteps.json` and its configuration page are used only by this servo-based feeder.

## Hardware and documentation

[Hardware overview](Hw/README.md) · [Interactive exploded view](Hw/ExplodedView.md) · [Bill of materials](Hw/BOM.md) · [Wiring diagram](Hw/Schematics/Wiring.md)

The mechanical model is stored in `Hw/CAD/Feeder.FCStd`. The designed [FeederV2](Hw/CAD/FeederV2.FCStd) variant adds an O-ring in `Top` and an updated CAD exploded view; it has not yet been physically tested. See the [hardware overview](Hw/README.md) for details. The electronics use purchased modules; custom PCB designs are not planned.

## Project and output layout

- `Hw/`: hardware documentation, CAD, images and wiring diagrams.
- `doc/`: local working documents, excluded from Git by the project-specific `.gitignore`. Existing files remain on disk.
- `Sw/`: project-local firmware sources and headers, including configuration definitions. `Version.inc.h` and `DeviceType.inc.h` are generated here.
- `EspBoardConfig.sh` defines the board, Arduino FQBN, LittleFS geometry and shared binary subdirectory for main firmware and rescue.
- `DevState/<device-name>/` contains the authoritative persistent device data.
- `generated/build/main/` and `generated/build/rescue/` contain separate disposable build caches.
- `generated/bin/<BoardId>_Arduino2/` contains main, rescue, LittleFS and combined artifacts for this Arduino target. Main and rescue clean operations remove only their own files.
- `generated/Instances/<device-name>/LittleFS/` is the complete LittleFS upload image generated from the shared `Libs/Common/Feeder/WebSrc` and `DevState`; `LittleFS-readable/` is its readable comparison copy.
- `generated/Instances/<device-name>/` also holds the FTP readbacks `ftp/`, `ftp-readable/` and `ftp-devstate-readable/`. `ftp/` is the raw device readback, `ftp-readable/` its readable form, and `ftp-devstate-readable/` the selected data for comparison with `DevState`.
- `generated/data/Versions.txt` is generated for the project-wide fallback data area; it is not an additional version source. Regular instance builds receive their `Versions.txt` in the corresponding `LittleFS/` directory.

`ServoSteps.json` stores each step as `[PositionGrad, SpeedMs, HoldMs, PositionTicks]`. `PositionGrad` is the valid user-entered degree value and is retained for readability. `PositionTicks` is the canonical machine target and is used directly when present, avoiding degree-to-tick drift when loading and saving again. Legacy three-field entries `[PositionGrad, SpeedMs, HoldMs]` are converted to ticks once during loading and then saved automatically in the four-field format.

A manual feed requested through the web interface only queues the servo start; execution begins in the next `TFeederServo::Loop()`. This lets the HTTP response leave the ESP before the Timer1/NMI-controlled servo run starts. `Running()` also accounts for a queued feed.

## OneWire diagnostics

The production Dallas measurement path runs on a fixed 10-second schedule using `TGnCoopTimerMs`: `RESET -> SKIP ROM -> CONVERT T`, followed 1000 ms later by `RESET -> SKIP ROM -> READ SCRATCHPAD`. An invalid scratchpad CRC triggers up to 19 immediate retries. Statistics distinguish success on reads 1 through 20 from final failure. A compact binary log, `/OneWireDiag.bin`, stores boot events and `OK`/`NOK` transitions only; polling and error counters remain in RAM. `/OneWireDiag/` provides statistics, state transitions, CSV export and reset for long-term testing. The raw FTP image retains the binary log as a fallback; `ftp-readable` omits it for Beyond Compare.

## Dallas / Heltec WiFi Kit 8

The default pin for the optional DS18B20 is `DallasPin = 2` (GPIO2), with an external 4.7-kΩ pull-up to 3.3 V. GPIO4/D2 and GPIO5/D1 are used by the OLED. GPIO15/D8 requires LOW for boot strapping and is not intended for a OneWire line pulled up to 3.3 V.

## Status display

Unlike `FeederSimple`, the servo-based `Feeder` has no firmware-defined `StatusLedPin` status LED. A possible LED on the ESP8266/Heltec board is not driven as an operating-status indicator by Feeder code; its reset/boot behaviour should not be interpreted as a status signal.

Operating status is shown on the OLED. In the status menu:

- `StIp: connecting`: station Wi-Fi is not connected yet.
- `StIp: <address>`: station Wi-Fi is connected.
- `NTP: --`: time is not valid yet.
- `NTP: OK`: time is synchronised/valid.

For failures before OLED/menu initialisation, the board LED does not provide a reliable error indication; use the serial boot log.

## Building and updating

Before every LittleFS build for an existing device, download its current files and compare them:

```sh
./tools/DownloadLittleFS.sh FeederAq1
```

Compare:

```text
generated/Instances/FeederAq1/ftp-readable/
  with generated/Instances/FeederAq1/LittleFS-readable/
generated/Instances/FeederAq1/ftp-devstate-readable/
  with DevState/FeederAq1/
```

Resolve differences in `Config.json` and `ServoSteps.json` before building, and copy the intended device data to `DevState/FeederAq1/`.

Build main firmware and instance-specific LittleFS packages together:

```sh
./EspCompile.sh
```

The combined package for FeederAq1 is then located at:

```text
generated/bin/wifi_kit_8_Arduino2/FeederAq1.ino.update.signed
```

On the device web page, select this package under “Fw-Update” → “Update package” and start the update.

Alternatively, upload main firmware and LittleFS separately:

```text
generated/bin/wifi_kit_8_Arduino2/Feeder.ino.bin.signed
generated/bin/wifi_kit_8_Arduino2/FeederAq1.ino.littlefs.signed
```

Select these files under “Update Firmware and/or Filesystem”. Firmware is shared by all Feeder instances; LittleFS is device-specific.

Separate uploads require the new firmware to boot with the existing LittleFS data. Use the combined package for incompatible file-format changes.

Rescue is built separately, using the same board profile and binary directory:

```sh
./EspRescueCompile.sh
```

The recommended rescue artifact is `generated/bin/wifi_kit_8_Arduino2/Feeder.Rescue.ino.bin.gz.signed`.

**Rescue must be installed once over USB on a new node.**

## Device identity and function swaps

Each physical device is identified by its MAC address and service ID. Dashboard can swap functions between functionally compatible devices. Hardware configuration and calibration remain with the physical device. See [Dashboard – device identity and function swaps](../Dashboard/README.md#device-identity-and-function-swaps) for operation and [DeviceIdentity.md](../Libs/GnBasicLib/DeviceIdentity.md) for technical details.

## Device assignments and PC files

A function swap alone does not require a filesystem upload. If a filesystem is to be uploaded later, synchronize PC data with the current device assignment before creating the image. Run the following command from the AquariumControl repository root:

```bash
python3 tools/ReadDeviceStates.py --apply
```

Dashboard cannot access PC files. Afterwards, rebuild affected filesystem images; images from different hardware types are not interchangeable.

UDP status messages use version 2 and publish optional temperature as `Status.Temperature`: `null` for an invalid reading, omitted when the sensor is disabled.
