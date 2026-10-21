<!-- README.md, Version: 1.52 -->
# TmrSwShelly

[Deutsch](README.DE.md)

## Initial takeover

Initial takeover of Nous A8T and Shelly Plug M Gen3 is described in the separate [ShellyNousTakeover](https://github.com/gneiss15/ShellyNousTakeover) repository. This project belongs to [AquariumControl](../README.md) and supplies the Main application after takeover.

Production project for Shelly Plug M Gen3: shared TmrSw Main with timer/heating roles, device-specific LittleFS and independent production Rescue. Product versions are in `Versions.txt`. Supply your own device configuration and signing material as described in [build and private configuration](../BUILD-SECRETS.md).

[Hardware documentation](Hw/README.md)

## Active sources

- `Sw/`, `Sw/main/`: Main, hardware abstraction and BL0942 power measurement.
- `partitions.csv`: active production partition layout.
- `Sw/Rescue/`: standalone native production Rescue, including runtime, geometry/activation checks, compatibility layer and local verification tests.
- `DevState/<Device>/`: persistent device-specific build source.

## Build and outputs

From the project directory:

```bash
./EspCompile.sh
./EspRescueCompile.sh
```

Main uses Arduino-ESP32 3.3.11 as a source component with ESP-IDF 5.5.1. Rescue uses a native IDF build. The scripts generate/sign artifacts; they do not upload or flash. Main is built once per project; without a selection, LittleFS is built for all DevState devices. For example, `--Device Shelly1` selects one instance.

Before every LittleFS build, read back current device files and resolve configuration differences (local data on the left, download on the right):

```bash
./tools/DownloadLittleFS.sh Shelly1
./tools/DownloadLittleFS.sh Shelly2
./EspCompile.sh
```

- Normal invocation: incremental.
- `--build`: delete this build's cache and perform a full rebuild.
- `--clean`: delete only this build's cache and exit.
- `--full-clean`: also remove this build's published artifacts.
- Main supports `--status` with phases and a compiler heartbeat.

`EspBoardConfig.sh` defines board, partition sizes and binary subdirectory. Main cache, Arduino dependencies and IDF metadata are under `generated/build/main/`; Rescue inputs and cache are under `generated/build/rescue/`. Shared binary output is under `generated/bin/esp32c3_Arduino2/`:

- `TmrSwShelly.ino.bin_signed`: Main.
- `<Device>.ino.littlefs_signed`: LittleFS.
- `TmrSwShelly.Rescue.rescue_signed`: Rescue.

Device LittleFS, readable copies and FTP readbacks are under `generated/Instances/<Device>/`. Main verification results are in `main-build-verification.json` there; Rescue results are in its build directory. Generated device readbacks may contain private data and are not in the public repository.

## Layout and hardware

The SDK partition-table offset is `0x10000`; Main is at `0x3A0000/0x2A0000`, LittleFS at `0x2C0000/0xE0000`, and Rescue at `0x640000/0x180000`. DeviceType is `TmrSwShellyC3V1`. Generic Main IDF flash targets are blocked because their factory address would overwrite Rescue. Signed updates use the Rescue web path.

Main checks chip/revision, flash/security state and actual SDK geometry before initializing networking, NVS or outputs. The active-high relay on GPIO0 starts OFF; the active-low button on GPIO7 uses INPUT_PULLUP. RGB is active-low: red GPIO18, green GPIO10, blue GPIO19. WLAN/NTP waiting blinks blue; normal operation shows relay state in green. Rescue signals SOS in red.

BL0942 uses UART1, RX5/TX4 at 9600 baud. Calibration comes exclusively from the actual device's factory record. After a warning, calibration overwrites only validated fields; remaining data, protection state and readback are checked. Power failure during a sector write remains a risk.

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

Relay, button and RGB LED GPIO preparation use the shared GnBasics interface. The OFF output level is set before enabling the relay; relay state is considered known only after successful initialization. LED brightness is controlled through LEDC.
