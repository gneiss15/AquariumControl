<!-- README.md, Version: 1.19 -->
# MultiTimer

[Deutsch](README.DE.md)

MultiTimer is an eight-channel switched power strip.

## Hardware and documentation status

A Raspberry Pi with an eight-channel relay board and power supply in a Conrad IR Powermanager enclosure, with an (optional) room-temperature sensor.
Dashboard and MultiTimer run on the same Raspberry Pi.
MultiTimer controls the relay board's outputs using schedules and measurement-dependent profiles.
Its eight switching outputs perform the control tasks of eight individual Nous or Shelly smart plugs.

**Coming soon – hardware description incomplete.** [Photos and hardware documents](Hw/README.md) show the assembly. A bill of materials, wiring diagram and installation details will follow.

## Directories and data

- `src/`, `include/`: native and shared C++ sources.
- `WebSrc/`: readable web sources; PreBuild copies static web files to `data/`, from which they are served.
- `data/`: generated initial data and mutable runtime files.
- `generated/build/<target>/`: compiler cache for `dummy` or `raspi`.
- `generated/Instances/GN-Pi/raspi/`: raw Pi readbacks.
- `generated/Instances/GN-Pi/raspi-readable/`: readable counterparts.
- `generated/Instances/GN-Pi/local-readable/`: local comparison copies for BCO.
- `include/ConfigDefaults.inc.h`: configuration fields and defaults for the JSON configuration page.

`Config.json`, `Channels.json`, `SwitchTimes.json` and `SwitchTimes.sync.json` are runtime data. The root `Config.json` supplies the native program's name. The separate `data/Config.json` contains generated web configuration. `Channels.json` contains profiles and channel assignments; `SwitchTimes.json` contains switching times and intervals.
Dashboard synchronization supplies the central values. Source/web updates must not overwrite runtime device configuration, channel assignments or schedules. `data/` may contain private data and must not be included in source archives.

The application resolves `data/` and `Log/MultiTimer.log` relative to its executable.
`PreBuild.sh` generates initial files from WebSrc and the configuration definitions. It is not a readback or backup tool for live Pi data. Running PreBuild again replaces local `data/` with initial data. `tools/DownloadData.sh` does not automatically protect against this. It reads back the entire Pi `data/` directory and the native `Config.json`, formats the copies and opens Beyond Compare (BCO): local files on the left, Pi download on the right. It does not copy files back into local build/runtime sources.
In the comparison, `NativeConfig.json` represents the root `Config.json`; `Config.json` within the download directory comes from `data/`.
The Pi code upload explicitly excludes runtime JSON files.

## Build

The PC test mode simulates hardware without connected relays.
Requirements are Python 3.10 or later, a C++20-capable g++, make, libjson-c and pthreads. Raspberry Pi additionally requires libgpiod 2.x for GPIO support. The Makefile detects a Raspberry Pi through `/proc/device-tree/model` and selects the Pi target automatically.

```bash
make
```

Explicit targets:

```bash
make dummy
make raspi
```

`make build` clears compiler caches and performs a full rebuild.
`make clean` deletes compiler caches and retains the `MultiTimer` executable; `make full-clean` also deletes the executable. Sources, configuration, runtime data and Pi readbacks are retained.

The executable is called `MultiTimer`. `make TARGET_SYSTEM=dummy` or `make TARGET_SYSTEM=raspi` also selects the target explicitly. When setting up without runtime data, `./PreBuild.sh` generates the initial data. Back up existing runtime data or reconcile it with the Pi data before running PreBuild.

## Start and service

```bash
./MultiTimer
./MultiTimer --port 8092
```

Defaults are HTTP 8080 and WebSocket 8081. With `--port`, WebSocket uses the next port; the HTTP port must be below 65535. `--log-start` logs the first start, and `--ReStart` a restart.

The default Pi directory is `/home/pi/Prjs/MultiTimer`.
The service is called `MultiTimer.service`.
A restart requested through the web interface terminates MultiTimer; systemd restarts it through `Restart=always`. The web process does not receive sudo privileges.

Start the PC transfer in an interactive terminal from the repository root:

```bash
./MultiTimer/CopyToRasPi.sh
```

`RASPI_HOST` and `RASPI_PRJS_DIR` override the target and path.
The transfer requires confirmation. Runtime Pi data is preserved during source updates.

After transferring, build on the Pi with `make` in the MultiTimer directory.
`Install.sh` installs and starts the systemd service and maintenance jobs.
For an existing installation, restart the service after rebuilding with `sudo systemctl restart MultiTimer.service`.

```bash
systemctl status MultiTimer.service
journalctl -u MultiTimer.service -f
```

`StartService.sh` distinguishes the first start from restarts within the same boot.
`tools/DownloadData.sh` reads back Pi data.

## Device identity and function swaps

Each physical device is identified by its MAC address and service ID.
Dashboard can swap functions between functionally compatible devices or individual switching outputs. Hardware configuration and calibration stay with the physical device. Operation is described in [Dashboard – device identity and function swaps](../Dashboard/README.md#device-identity-and-function-swaps).
Technical details are in [DeviceIdentity.md](../Libs/GnAqPrjsLib/DeviceIdentity.md).

## Device assignments and PC files

A function swap alone does not require a filesystem upload. If an ESP filesystem is to be uploaded later, synchronize the PC data with the current device assignment before creating the image. Run the following command from the AquariumControl repository root:

```bash
python3 tools/ReadDeviceStates.py --apply
```

Dashboard itself cannot access PC files. For MultiTimer, use `tools/DownloadData.sh` to read back runtime Pi data; source transfer with `CopyToRasPi.sh` preserves that data on the Pi.

JSON editors display object arrays (profiles/channels) with one aligned line per record; additional fields are preserved. Channels.json uses the available page width.

Channels.json and Sources.json each have a framed box with an inner text field. Sources.json is initially collapsed; the Sources.json button shows or hides it. The local Sources editor is enabled only when Dashboard is unreachable; the server checks this again when saving.

## Channel swaps through Dashboard

During a channel swap, `PhysPos: "Main X"` identifies the fixed socket 1–8; Id, role, profile and schedules follow the function. Swapping heating roles applies the new role's thresholds to the same physical connection. GPIOs, buttons, polarity and array positions remain unchanged.

Channels and schedules are validated and saved together. Interrupted assignment changes are completed at startup before hardware initialization. MultiTimer confirms the new assignment only after successful application. Access to the affected configuration files is blocked while an update is incomplete.

Assignments and selection lists are derived from Channels.json. When activated, an open page queries `/api/v1/config-revision` once and reloads only if the revision has changed. Switching Dashboard sections sends `ConfigurationActivated`. Unsaved edits are retained; a change notice allows controlled reloading. There is no periodic configuration-polling timer.

## Measurements

MultiTimer receives measurements through UDP version 2 as named numeric fields in the `Status` object. Profiles select the field through `Measurement`, for example `Temperature` or `BatteryPercent`. An empty measurement field means `Temperature`.

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

## Optional room-temperature sensor

MultiTimer reads a DS18B20 on BCM GPIO4 (header pin 7) through the Linux 1-Wire driver. Connect the sensor to 3.3 V and GND, with a 4.7 kΩ pull-up between data and 3.3 V. Never pull the GPIO up to 5 V.

After installing the current MultiTimer build, run on the Raspberry Pi:

```sh
cd /home/pi/Prjs/MultiTimer
./EnableRoomTemperature.sh
```

The script requires `sudo`. It checks GPIO4 against the channel configuration, backs up the active boot configuration and `Config.json`, activates 1-Wire live and for future boots, enables the optional sensor and restarts MultiTimer. A Pi reboot is not required.

The native root `Config.json` accepts this optional object:

```json
"RoomTemperature": {"Enabled": true, "SensorId": ""}
```

An empty `SensorId` discovers exactly one DS18B20 and binds to it for the process lifetime. With multiple sensors, set its `28-xxxxxxxxxxxx` ID explicitly. An absent object or `Enabled: false` disables the sensor.

A worker starts one conversion every ten seconds; scheduler and network handling keep running. Only complete CRC-checked readings are published, without averaging. Missing, malformed or stale readings are invalid rather than zero. The UDP status contains the `RoomTemperature` measurement and `Temperature` in °C; Dashboard displays room temperature under **System**, separate from the aquarium relay views.

File diagnostics are disabled by default. Rebuild with `make build GN_DEBUG_FILE_LOG=1` to enable `Log/MultiTimer.log`; RAM error reporting remains active. The hourly `FreeCheck.sh` cron entry is commented out. For an explicit memory check use `GN_DEBUG_FREE_CHECK=1 ./FreeCheck.sh`. `RebootPi.sh` still performs the scheduled 00:01 system reboot; its optional file entry requires `GN_DEBUG_FILE_LOG=1`. Existing log files are retained.
