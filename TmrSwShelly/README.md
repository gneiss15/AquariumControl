<!-- README.md, Version: 1.56 -->
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

Relay, button and RGB LED GPIO preparation use the shared GnBasics interface. The OFF output level is set before enabling the relay; relay state is considered known only after successful initialization. LED brightness is controlled through LEDC.

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
