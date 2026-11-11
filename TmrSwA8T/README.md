<!-- README.md, Version: 1.31 -->
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

`EspRescueCompile.sh` sources the shared `../Common/EspRescue/_EspRescueCompileEsp32Common.sh` helper. This helper is not executable and is not a direct build entry point; invoking it directly through `bash` is also rejected.

## Project and output layout

- `Sw/`: project firmware sources and headers, including configuration definitions; `Version.inc.h` and `DeviceType.inc.h` are generated here.
- `Sw/main/`: IDF Main component; CMake project and SDK/partition settings remain in the project root.
- `EspBoardConfig.sh`: board/FQBN, binary subdirectory and partition sizes for all builds.
- `DevState/<Device>/`: persistent device configuration and build source.
- `generated/build/main/`: Main cache, generated headers and prepared web sources. `esp32/idf/` holds the IDF compiler cache, `MainDependencies/` the prepared Arduino component and libraries, and `IdfProject/` the generated CMake project, SDK settings, component management and dependency lock file.
- `generated/build/rescue/`: Rescue build inputs and ESP-IDF cache.
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

Main uses ESP-IDF 5.5.1 and Arduino-ESP32 3.3.11 as a component built from source. The setup/loop task runs on core 1 with an 8192-byte stack. Rescue builds directly with ESP-IDF 5.5.1 and uses the partition table at `0x8000`. Binary files are under `generated/bin/esp32_Arduino2/`.

`ESP_IDF_ENABLE_SCRIPT` loads the local IDF environment. `ESP_IDF_COMPILER_BIN`, `ARDUINO_PORTABLE_DIR` and `ARDUINO_CORE_DIR` override tool paths. The build entry point is `./EspCompile.sh`. For firmware only, without LittleFS:

```bash
TMR_SW_ENABLE_LITTLEFS=false ./EspCompile.sh
```

Generic IDF flash targets are blocked: they would write Main to factory address `0x10000`, overwriting Rescue. Main belongs in `app0` at `0xE0000` and is installed using the signed Rescue upload path. The build checks ESP32 image, version, Main size and actual SDK partition table before signing. A successful build does not replace a hardware test.

## Device identity and function swaps

Each physical device is identified by its MAC address and service ID. Dashboard can swap functions between functionally compatible devices or individual switching outputs. Hardware configuration and calibration stay with the physical device. See [Dashboard – device identity and function swaps](../Dashboard/README.md#device-identity-and-function-swaps) for operation and [DeviceIdentity.md](../Libs/GnAqPrjsLib/DeviceIdentity.md) for technical details.

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

## Charge run-on for battery profiles

Enable **Charge run-on active** in the Dashboard profile and enter **Seconds per percentage point**. This requires `Measurement: "BatteryPercent"`. The editor initially suggests `15` seconds per percentage point; learning subsequently adjusts the same factor. Without active run-on, the existing missing-measurement behavior applies.

When reports stop and the output was automatically ON, the switching device calculates the remaining charging time:

`(cutoff percentage − last battery percentage) × seconds per percentage point`

Time already charged since the last report counts towards this budget. An output that was OFF is never switched ON by missing reports. Every manual switching command immediately discards the learning cycle, including returning to automatic control. An aborted cycle never updates the factor. A schedule switching OFF or changed profile parameters also cancels the current cycle. There is no additional maximum run-on duration.

The next valid battery report provides the gain used to calculate a factor from actual ON time. For a gain of at least one percentage point, the new estimate contributes 25 % and the previous factor contributes 75 %. Without a measurable gain, the factor stays unchanged. The new factor is rounded down to whole seconds. Changes below 5 % relative to the last saved factor stay in RAM and carry into the next learning cycle. Only a difference of at least 5 % saves the factor in the profile and synchronizes it with Dashboard. The active learning cycle exists only in RAM; after a restart, fresh battery reports and an automatically enabled output must arm a new cycle.

The laptop grants learning permission through `Status.ChargeLearningAllowed` (boolean) and publishes `Status.ChargeBootCounter`, a wrapping `uint32` boot counter including zero. A returning learning measurement may only be evaluated when the counter difference modulo `2^32` is exactly 1. Additional boots or missing/invalid permission discard the measurement; normal run-on remains available. The learning switch does not increment the counter. See [LaptopBattery](../LaptopBattery/README.md) for installation, controls and external-charging limitations.

The JSON fields are `ChargeRunOn` and `ChargeSecondsPerPercent`. Additional profile fields are allowed and preserved during editing and synchronization. `ChargeRunOn: true` requires a positive integer factor (`uint32_t`); inactive profiles need neither field.

## Status messages

Regular UDP status broadcasts are sent every ten seconds. Output/input states, overrides, errors and measurement validity changes are reported immediately, as are startup and reconnection. Measurement changes alone do not trigger additional broadcasts.
Voltage, current and active power are published together from the last complete measurement cycle. Partial results never replace individual fields of the published set. Power-meter diagnostics remain available on request through `/api/v1/status`; they are excluded from UDP broadcasts.

## PI observation mode

An optional profile `Pi` object with `Mode: "Shadow"` enables a controller that calculates every ten seconds while the existing hysteresis controller continues switching. Heat demand is allocated to the floor first, with remaining demand allocated to the main heater. Reported relay requests are hypothetical and never sent to hardware or peers. Missing `Pi` or `Mode: "Off"` disables observation. Active PI switching is not accepted yet.

The integral is bounded, conditionally integrated at saturation and updated solely from temperature error using Ki = Kp / Ti. Unbridgeable measurement loss, manual overrides, disabled schedules, gaps and peer restarts discard learning windows. A missing floor peer removes floor power from allocation and prevents learning; missing room temperature disables model learning/feed-forward without stopping PI calculation.

Slow learning uses actual measured main watts and measured constant floor watts with confirmed floor state. Valid zero watts remain zero. Continuous one-hour windows weakly adjust effective heat loss at nearly equal initial/final water temperature; excursions of at least 0.25 K permit bounded capacity/gain adaptation. Each accepted estimate contributes 5 %, and estimates must remain within half to twice the configured seed. Configured delay aligns past energy with subsequent temperature for learning. Learned values remain in RAM in both Shadow and Active modes and are recorded as telemetry; restart/configuration changes restore profile seeds. Ti remains the configured slow seed. This is not a validated closed-loop plant identification.

HTTP/UDP `HeatPi` status reports requested and actual watts, P/I/feed-forward, hypothetical floor/main requests, model parameters and learning state. Observation telemetry does not trigger additional UDP broadcasts. The floor peer is identified explicitly by MAC@Service-ID and channel name; update this binding after a channel/device swap.

## PI measurement gaps

For missing or invalid inputs, PI control holds the last valid temperature,
power and room-temperature readings until their actual sample age reaches
60 seconds. PWM, integral updates and learning continue using the held readings.
Diagnostics report `measurement-held`; held readings are not fresh measurements.
Without a previous valid reading, or after expiry, the configured fallback
applies. The last valid main request for floor heating also lasts 60 seconds;
an explicit received OFF request takes effect immediately. Restart or profile
changes discard the measurement cache.

During `measurement-held`, learning uses approximate energy from the last valid power readings up to the 60-second limit. These periods are identified by the diagnostic reason and are not additional actual measurements.

Active PI switching uses a fixed energy budget per PWM period. Delivered main energy is integrated from measured power; floor energy uses the measured constant floor power and reported state. Unmet energy carries into the next period with floor priority; sampled overdelivery reduces the next budget. Manual intervention, invalid measurements beyond the grace interval, configuration changes and observation gaps discard the pending budget. Budget and carry are RAM-only; metering and relay feedback resolution limit cutoff accuracy.
