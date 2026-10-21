<!-- README.md, Version: 1.35 -->
# FeederSimple

[Deutsch](README.DE.md)

## Hardware documentation status

**Coming soon – incomplete.** Photos and a description of the purchased-feeder conversion are under [hardware](Hw/README.md). Complete assembly instructions and a bill of materials will follow.

FeederSimple shares its schedule, web, API, WebSocket, WLAN, OTA and temperature core with `Feeder`.

Shared firmware and readable web sources are in `../Libs/Common/Feeder`. Motor, home input, local button, status LED and Dallas diagnostic page are project-specific.

## Hardware

The original controller is not used. The PCB stays in the device for mechanical reasons and provides components and connection points:

- Button directly on the GPIO input, active-low with internal pull-up.
- LED through R2 on the GPIO output, active-high.
- Motor through R4 and Q1 on the GPIO output, active-high.
- Home signal through R3 on the GPIO input.
- Shared 3.3 V supply for the PCB and ESP.

U1 pin assignments and connection points are recorded in local documents under `doc/`; public hardware documentation is being completed. HIGH switches the motor on, LOW switches it off. Firmware uses a normal push-pull output. HOME is active-low; no internal HOME pull-up is used.

ESP pin assignments, debounce times and motor/homing timeouts come exclusively from `Sw/FeederConfig.inc.h`, from which `Config.json` and `ConfigEntries.json` are generated.

## Home input and temperature diagnostics

HOME uses `FeederSimpleHomeActiveLow=1` and `FeederSimpleHomeUsePullup=0`; timeout values are `HomeLeaveTimeoutMs=1000`, `RotationTimeoutMs=7500` and `HomingTimeoutMs=7500`. `DallasPresent=true` enables temperature measurement and display in Dashboard.

The normal OneWire path expects exactly one DS18B20 and does not require ROM discovery: every 10 seconds, `RESET -> SKIP ROM -> CONVERT T`, followed 1000 ms later by `RESET -> SKIP ROM -> READ SCRATCHPAD`. CRC failures allow up to 19 immediate read retries. `http://<FeederSimple-IP>/Test` provides an independent Dallas/OneWire stability test with Presence, SEARCH ROM, READ ROM, SKIP ROM and MATCH ROM. For mechanical/contact tests, it can also run a cancellable continuous test with a short pause between attempts and the last 100 attempts shown.

A feed delays motor startup by one loop iteration, allowing a web manual-feed response to be sent first. The status LED stays on while feeding.

## Status LED and local button

The active-high status LED is fixed to `StatusLedPin` = GPIO13/D7 (`#define` in `Sw/HwConfig.FeederSimple.h`). In a normal boot, the GPIO is not initialized as an output until `FeederDrive.Setup()`; a bright flash or dim glow immediately after reset is not a normal operating status. At startup level 0, the recovery path initializes it directly and blinks about every 150 ms. After LittleFS, `Config.json`, `Index.html` and `FeedTimes.json` load successfully, `FeederDrive.Setup()` first switches it OFF.

Afterwards:

- No STA WLAN connection: LED toggles every 500 ms.
- WLAN connected, time/NTP invalid: LED toggles every second.
- WLAN connected and time valid: LED OFF.
- Feeding: LED continuously ON.

If startup fails before `FeederDrive.Setup()`, firmware does not yet control the LED and the local button is not initialized.

The active-low local button is on GPIO0/D3 and is evaluated on release:

- Press shorter than `ButtonLongPressMs` (currently 5 s): one portion.
- At `ButtonLongPressMs`: LED blinks rapidly. Releasing now reconnects WLAN.
- Keep holding until `ButtonResetPressMs` (currently 10 s): LED turns off and temporary AP+STA access starts immediately. Release triggers neither feeding nor another reconnect.
- The temporary AP uses the configured AP SSID/password. The normal web interface is available there, usually at `http://192.168.4.1/`. The saved WLAN mode is unchanged; successful STA connection restores the configured mode. Reboot also ends temporary mode.
- `ButtonResetPressMs` is the configuration name for the second threshold that enables AP access.

GPIO0 is also an ESP8266 boot-strap pin. Do not hold the button during reset or power-on, as that starts the serial bootloader.

## Project and output layout

- `Sw/`: project firmware sources/headers and configuration definitions; `Version.inc.h` and `DeviceType.inc.h` are generated here.
- `EspBoardConfig.sh`: board, Arduino FQBN and LittleFS geometry for Main, Rescue and flashing scripts.
- `DevState/<Device>/`: authoritative persistent device data.
- `doc/`: local hardware assignments and technical documents.
- `generated/build/main/`, `generated/build/rescue/`: separate disposable build caches.
- `generated/bin/<BoardId>_Arduino2/`: Main, Rescue, LittleFS and combined artifacts for this Arduino target. Main/Rescue cleanup removes only its own files.
- `generated/Instances/<Device>/LittleFS/`: complete LittleFS upload image generated from `../Libs/Common/Feeder/WebSrc` and DevState; `LittleFS-readable/` is its readable comparison copy.
- The same instance directory contains FTP readbacks `ftp/`, `ftp-readable/` and `ftp-devstate-readable/`.
- FeederSimple does not use `ServoSteps.json`; instances contain configuration, feeding schedules and other shared web data.

## Build and update

`EspBoardConfig.sh` targets a NodeMCU ESP8266 board with 4 MB flash and `4M2M` layout. Adjust `FQBN`, `BoardId` and LittleFS settings before building for another board or flash layout.

Before every LittleFS build for an existing device, download and compare its current files from the project directory:

```sh
./tools/DownloadLittleFS.sh FeederAq2
```

Compare:

```text
generated/Instances/FeederAq2/ftp-readable/
  against generated/Instances/FeederAq2/LittleFS-readable/
generated/Instances/FeederAq2/ftp-devstate-readable/
  against DevState/FeederAq2/
```

Resolve `Config.json` differences before building and copy desired device data to `DevState/FeederAq2/`. Feeding times are synchronized centrally.

Signing uses the central pair `../../../PrjsPrivate/private.key` and `../../../PrjsPrivate/public.key`. The private key stays local and is not archived.

Build Main and instance-specific LittleFS packages together:

```sh
./EspCompile.sh
```

The combined package for FeederAq2 is:

```text
generated/bin/nodemcuv2_Arduino2/FeederAq2.ino.update.signed
```

On the device's firmware-update page, select it under **Update package** and start the update.

Main and LittleFS can alternatively be uploaded separately:

```text
generated/bin/nodemcuv2_Arduino2/FeederSimple.ino.bin.signed
generated/bin/nodemcuv2_Arduino2/FeederAq2.ino.littlefs.signed
```

Separate upload requires the new firmware to start with the existing LittleFS. Use the combined package for incompatible file-format changes.

Rescue is built separately with the same board profile and binary directory:

```sh
./EspRescueCompile.sh
```

The recommended Rescue artifact is `generated/bin/nodemcuv2_Arduino2/FeederSimple.Rescue.ino.bin.gz.signed`.

## Long-term OneWire diagnostics

The normal measurement cycle records boot and only `OK`/`NOK` state transitions in binary `/OneWireDiag.bin`. Counters distinguish success on scratchpad read 1/2/3, final read failure, conversion/read resets, CRC and invalid temperature. `/OneWireDiag/` shows statistics and log, offers CSV export and a reset before an overnight test. The manual Dallas test under `/Test` pauses the normal cycle and is excluded from these statistics.

## Device identity and function swaps

Each physical device is identified by its MAC address and service ID. Dashboard can swap functions between functionally compatible devices. Hardware configuration and calibration remain with the physical device. See [Dashboard – device identity and function swaps](../Dashboard/README.md#device-identity-and-function-swaps) for operation and [DeviceIdentity.md](../Libs/GnBasicLib/DeviceIdentity.md) for technical details.

## Device assignments and PC files

A function swap alone does not require a filesystem upload. If a filesystem is to be uploaded later, synchronize PC data with the current device assignment before creating the image. Run the following command from the AquariumControl repository root:

```bash
python3 tools/ReadDeviceStates.py --apply
```

Dashboard cannot access PC files. Afterwards, rebuild affected filesystem images; images from different hardware types are not interchangeable.

UDP status messages use version 2 with optional temperature in `Status.Temperature`: `null` for an invalid measurement, or no field when the sensor is disabled. Receivers must support version 2.
