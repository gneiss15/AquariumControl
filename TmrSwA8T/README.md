<!-- README.md, Version: 1.27 -->
# TmrSwA8T

[Deutsch](README.DE.md)

## Initial takeover

Initial takeover of Nous A8T and Shelly Plug M Gen3 is described in the separate [ShellyNousTakeover](https://github.com/gneiss15/ShellyNousTakeover) repository. This project belongs to [AquariumControl](../README.md) and supplies the Main application after takeover.

Nous A8T project with Main, Rescue and device-specific LittleFS. Product versions are in `Versions.txt`. Supply your own device configuration and signing material as described in [build and private configuration](../BUILD-SECRETS.md).

[Hardware documentation](Hw/README.md)

## Hardware power measurement

Power measurement uses hardware pulse counters (PCNT) and two timed measurement gates (LEDC). GPIO25 measures CF1 with a nominal 2 s measurement and 0.5 s pause, alternating between voltage and current. GPIO32 measures CF with a nominal 4 s measurement and 1 s pause. Each quantity is updated once per nominal 5 s cycle; initial values require a complete measurement window.

Reading, measurement selection and counter clearing take place with the gate closed. Conversion accounts for actual gate duration. An accumulator extends the hardware counter; both accumulator and remaining count are included when reading. Details are in the architecture comment in `Sw/TmrSwA8TPowerMeter.cpp`.

Current and active power are calculated and displayed from actual counter values even when the relay is logically OFF. A relay transition discards the affected measurement windows; new values become available after complete windows. Unexpected consumption while OFF remains visible. The display alone does not automatically diagnose contact faults.

Device-specific calibration values are held in NVS. Diagnostic counters record pulses from accepted measurement windows.

## Power-meter calibration

A Nous A8T can be calibrated using a Shelly as the reference. [Instructions](../tools/CalibrateA8T.md) · [Script](../tools/CalibrateA8T.py). Two measurement runs reverse the order of the devices. The script temporarily switches on both relays; calibration values are written only to the A8T, with `--apply` and additional confirmation. Shelly calibration is unchanged.

## Build entry points

- `./EspCompile.sh`: ESP-IDF Main with Arduino component and LittleFS for devices under `DevState/`.
- `./EspRescueCompile.sh`: Rescue firmware.

`EspRescueCompile.sh` sources the shared `../Libs/Common/EspRescue/_EspRescueCompileEsp32Common.sh` helper. This helper is not executable and is not a direct build entry point; invoking it directly through `bash` is also rejected.

## Project and output layout

- `Sw/`: project firmware sources and headers, including configuration definitions; `Version.inc.h` and `DeviceType.inc.h` are generated here.
- `Sw/main/`: IDF Main component; CMake project and SDK/partition settings remain in the project root.
- `EspBoardConfig.sh`: board/FQBN, binary subdirectory and partition sizes for all builds.
- `DevState/<Device>/`: persistent device configuration and build source.
- `generated/build/main/`: Main cache, generated headers and prepared web sources. `esp32/idf/` holds the IDF compiler cache, `MainDependencies/` the prepared Arduino component and libraries, and `IdfProject/` the generated CMake project, SDK settings, component management and dependency lock file.
- `generated/build/rescue/`: Rescue cache and generated Rescue sketch.
- `generated/bin/esp32_Arduino2/`: shared output for Main, Rescue and LittleFS artifacts.
- `generated/Instances/<Device>/LittleFS/`: complete device-specific LittleFS.
- `generated/Instances/<Device>/LittleFS-readable/`: readable build comparison copy.
- The same instance directory contains `ftp/`, `ftp-readable/`, `ftp-devstate-readable/` and any additional `ftp-current/` snapshots.
- `generated/Logs/`: power-measurement CSV files.
- `generated/data/Versions.txt`: generated fallback version metadata.

LittleFS build copies contain empty schedules/intervals, `Profiles: []` and, where a sync state exists, an initial timestamp with `Synced: false`. Channel assignments are retained; Dashboard synchronization supplies central runtime data. Files under `DevState/` are not changed for this preparation.

Main and Rescue use incremental builds. `--clean` deletes only that build's cache; `--full-clean` also deletes its artifacts in the shared binary directory. Other builds are retained.

Before a LittleFS build, download device data, compare local data on the left with the device download on the right in BCO, and resolve configuration differences. Example from the A8T project directory:

```bash
./tools/DownloadLittleFS.sh LightTimerAq3
./EspCompile.sh
```

Signed upload files are under `generated/bin/esp32_Arduino2/`:

- Main: `TmrSwA8T.ino.bin_signed`
- LittleFS: `<Device>.ino.littlefs_signed`
- Rescue: `TmrSwA8T.Rescue.rescue_signed`

The partition table is a verification file at `generated/build/main/esp32/TmrSwA8T.ino.partitions.bin`; it is not copied into the upload directory. Firmware-only builds retain the last LittleFS images. `--full-clean` explicitly removes them.

## ESP-IDF Main

Main uses ESP-IDF 5.5.1 and Arduino-ESP32 3.3.11 as a component built from source. The setup/loop task runs on core 1 with an 8192-byte stack. Rescue uses an Arduino CLI build. Binary files are under `generated/bin/esp32_Arduino2/`.

`ESP_IDF_ENABLE_SCRIPT` loads the local IDF environment. `ESP_IDF_COMPILER_BIN`, `ARDUINO_PORTABLE_DIR` and `ARDUINO_CORE_DIR` override tool paths. The build entry point is `./EspCompile.sh`. For firmware only, without LittleFS:

```bash
TMR_SW_ENABLE_LITTLEFS=false ./EspCompile.sh
```

Generic IDF flash targets are blocked: they would write Main to factory address `0x10000`, overwriting Rescue. Main belongs in `app0` at `0xE0000` and is installed using the signed Rescue upload path. The build checks ESP32 image, version, Main size and actual SDK partition table before signing. A successful build does not replace a hardware test.

## Device identity and function swaps

Each physical device is identified by its MAC address and service ID. Dashboard can swap functions between functionally compatible devices or individual switching outputs. Hardware configuration and calibration stay with the physical device. See [Dashboard – device identity and function swaps](../Dashboard/README.md#device-identity-and-function-swaps) for operation and [DeviceIdentity.md](../Libs/GnBasicLib/DeviceIdentity.md) for technical details.

## Device assignments and PC files

A function swap alone does not require a filesystem upload. If a filesystem is to be uploaded later, synchronize PC data with the current device assignment before creating the image. Run the following command from the AquariumControl repository root:

```bash
python3 tools/ReadDeviceStates.py --apply
```

Dashboard itself cannot access PC files. After synchronization, rebuild the affected device-specific filesystem images. Images are not interchangeable between different hardware types.

JSON editors display profile/channel object arrays with one aligned line per record; extra fields are preserved. `Channels.json` uses the available page width. The `Sources.json` editor opens through its button and is enabled only while Dashboard is unreachable. The server checks this again when saving.

Signed filesystem images use the hostname from configuration: the function name for assigned functions, or the hardware name for reserve devices. `DevState/` and `generated/Instances/` use internal function keys.

## Channel swaps through Dashboard

The single switching output can exchange its function with a functionally compatible Nous/Shelly output or MultiTimer channel. Channel name, role, profile and schedule follow the function; GPIOs, calibration, hostname and credentials stay with the physical device.

Disconnect consumers before swapping and wait for confirmed completion before reconnecting. Channel assignments, switching times and intervals are validated and saved together. Interrupted changes are completed at startup before hardware initialization. The new assignment is confirmed only afterwards.

## Measurements and status

Status messages and measurement requests use UDP version 2. Measurements are named numeric fields in the `Status` object. Profiles select a field through `Measurement`; an empty field means `Temperature`.

Relay, LED, button and measurement selection use the shared GnBasics GPIO interface. Inactive output levels are set before outputs are enabled. Relay state is considered known only after successful initialization.
