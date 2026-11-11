# HeatHistory

[Deutsch](README.DE.md)

Document version: 1.32
Updated: 2026-11-02

HeatHistory runs on a PC or Raspberry Pi. It passively observes aquarium heating control and records CSV histories per profile on that computer. It does not change configuration.

## Profile and status validation

HeatHistory receives central profiles, channel assignments and device mapping through Dashboard SSE at `/api/recording-events`. The separate one-pass diagnostic uses `/api/heatprofiles`, `/api/times` and `/api/device-registry`. `TempId` identifies the logical temperature source. The unique `HeaterFloor` and `HeaterMain` channels are found across all devices by their profile assignment. The physical temperature source follows endpoint assignment when sensors are swapped.

Temperature and switching states are read directly from device UDP status packets on port 44444, without querying device web pages or `/api/v1/status`. Each packet contains device ID and status; the feeder supplies temperature in Celsius. By default, HeatHistory waits up to eight seconds for current packets and accepts status up to 15 seconds old. These limits and Dashboard address are in `config.json`. The recording computer must be in the same broadcast network as the devices; its firewall must allow incoming UDP port 44444.

Missing central profiles/assignments, required UDP packets, valid temperature or switching states pause only the affected aquarium. Errors include a UTC timestamp. Recording resumes automatically after successful validation. Dashboard HTTP errors also show the duration of the failed request.

JSON formatting changes do not count as profile changes. A semantic profile change creates a snapshot and a new CSV. Its first data row contains the state at the time the log starts.

The CSV header and column order are fixed:

```text
Time;Temp;Floor;Main
```

A row is written when temperature, Floor or Main state changes from the previous record.

## Event-driven recording

At startup HeatHistory opens UDP sockets and an SSE connection to Dashboard
(`/api/recording-events`, DashboardServer 1.46 or newer). Dashboard sends one
complete configuration snapshot, then only changed/removed profiles, channel
groups and, when necessary, device assignments. Profiles and assignments are not
polled over HTTP. Occasional keepalive comments keep the connection open without
sending periodic configuration copies.

`Run()` blocks waiting for UDP or Dashboard events. `OnDashboardEvent()` dispatches
to `OnProfileChange()`, `OnChannelChange()` and `OnRegistryChange()`;
`ApplyConfiguration()` updates recording assignments. `UdpDescriptions.OnUdpPacket()`
parses and validates UDP packets centrally, then dispatches to `OnInfoPacket()` or
`OnStatusPacket()`. Complete status data reaches the recorder through its
`OnStatusPacket()` handler. Descriptions retain the original sample receive times. CSV files remain open and complete rows are flushed for
concurrent browser readers. A segment change closes only affected writers.
`Start()` and `Close()` manage the lifecycle.

On connection loss, recording continues with the last complete valid configuration.
Reconnection provides a full snapshot to recover missed changes. Recording cannot
start without an initial valid snapshot. A pending device reassignment is not
accepted as valid until distribution completes, as before.
Retention runs once at service startup; the scheduled daily Pi reboot also
triggers daily cleanup. No additional retention timer is used.
Old `PollIntervalSeconds` settings are no longer used.

## Data directory layout

Each aquarium has its own profile directory under
`~/.local/share/HeatHistory/data/`, for example `HeatProfileAq1/`:

| Location / file type | Contents |
|---|---|
| `HeatProfileAqX/*.csv` | Temperature and reported Floor/Main switching states. A row is added when these values change. |
| Matching `*.json` | Snapshot of the heating profile associated with that CSV, preserving its historical meaning after later profile changes. |
| `state.json` | Current file assignment, system boot identity and last recorded state used to resume temperature/switch recording. |
| `Energy/*.energy.csv` | Reported power, water/room temperatures, switching states, integrated energy and valid, held, interpolated or unknown interval information. |
| `Energy/*.energy.json` | Device/channel assignments, profile, fixed floor power, factors and integration description for that energy segment. |
| `Pi/*.pi.csv` | PI-controller diagnostics: temperature, temperature error, P/I/heat-loss terms, requested power, switching requests and learning state. `Pi` refers to the PI controller. |
| `Pi/*.pi.json` | Associated controller configuration including setpoint, mode, device identity and boot identifiers. |

**Measurement series remain CSV.** JSON files contain supporting metadata or
resume state, not another copy of the measurement series. Keep them with their
CSVs; they support historical interpretation and recording recovery.

UTC filename timestamps identify segment starts. Depending on the recording type,
a new segment starts after a recording-host system boot, a profile/assignment
change or a controller restart. A HeatHistory service restart resumes compatible
files from the same system boot.

## Browser charts

The web service returns static HTML/JavaScript. The browser requests a CSV bundle with a small JSON manifest
from `/api/history.csv?range=1day` and renders the charts with Plotly. Reloading
does not generate an HTML document containing all recorded series.

Every page load starts with **1 day**. **1 h**, **6 h**, **7 days**, and **All**
request the corresponding range for both aquariums. **All** means available
retained history, starting with active PI operation and limited to the configured
view span (30 days by default). Optional `ViewFromUTC` sets an explicit start;
otherwise the first active PI record or active energy-profile snapshot is used.
Older PC data remains unchanged and is excluded from the web view.
**Custom…** accepts local start and end times, for example 9 Nov 2026 11:00 to
10 Nov 2026 11:00. The browser converts them to UTC; the request remains bounded
by PI activation, the retention limit, and the current time. The effective UTC
range is shown after loading.

Water, Room, and historical Setpoint share the temperature axis. Floor, Main,
Power, Energy, and optional PI demand/PI components appear in aligned rows.
Energy adds recorded intervals and their corrections **within the selected range**;
held/interpolated intervals remain estimates. Floor uses measured constant power and reported state. Main
uses the corrected final value without applying another factor. The historical
Setpoint remains available from energy-profile snapshots without PI debug.

Recording and measurement transport remain CSV; only the small manifest is JSON. Plotly.js still
loads from its CDN. The legend moves below the chart on narrow screens. The API
accepts known ranges or a bounded custom interval and never serves configuration
or arbitrary files.

CSV files are sorted by their UTC filename prefix within each profile and recording
type. The next filename bounds the preceding segment; non-overlapping files are
excluded before opening them. Start skips records before the requested time,
Mid sends complete records without parsing each timestamp, and End stops after
the upper time bound. This requires chronological, non-overlapping recording
segments; filename normalization, if needed, is a separate preparation step.
A fixed file-size snapshot excludes partial writer records and later appends.

The HTTP response streams in bounded chunks, directly through GZIP level 3 when
the client accepts `gzip`, otherwise directly without compression. The full
response is never assembled in RAM. CSV quoted fields and multiline records are
preserved; the browser still performs measurement conversion and plotting.

Optional `python3 CreateCharts.py` remains a separate offline export, including
older PC backups when explicitly selected.

## Move from the PC to the Raspberry Pi

Requirements: SSH access as `pi` to `192.168.99.51`, Python 3.10 or later,
port 8091 available, and no existing HeatHistory installation at the Pi destination.
Run in the PC terminal from this directory:

```bash
./MoveToRasPi.sh --allow-log-expiry
```

Enter the local sudo password, then the Pi sudo password over SSH when requested.
The script stops PC recording, exports complete rows from the start of active PI
control (at most 30 days), transfers sources and history, and installs the Pi service.
It disables the PC service only after both aquariums provide fresh energy rows.
On failure, PC recording is restarted. Existing PC data remains unchanged under
`~/.local/share/HeatHistory/data/`. The exported snapshot and timestamps are also
kept under `generated/PiMigration-<UTC>/`.

Pi runtime, configuration and data: `/home/pi/.local/share/HeatHistory/`.
Service: `HeatHistory.service`. Browser: <http://192.168.99.51:8091/>.
Charts are still calculated by the browser. For later updates, run
`./CopyToRasPi.sh` on the PC to transfer the project sources, then run
`./Install.sh` in the Pi source directory. Configuration and recording data are
preserved.

**`--allow-log-expiry` explicitly permits permanent removal of this application's
closed Pi log files** after 30 days or to stay within 512 MiB of managed history.
Expired closed segments are removed first. Then `du -sb` measures the remaining
size; further closed segments are removed oldest first if necessary. A CSV and its
JSON metadata remain a pair. Active files and `state.json` stay protected.
Precondition: the data directory contains only our recordings, with no unrelated
files or symlinks. Daily rotation keeps old segments closed. Consent is stored as `StorageDeleteExpired: true` in the Pi
configuration. Without consent, existing logs are not removed. The PC backup is
independent. Temperature, energy and PI files rotate on system boot (normally the nightly
00:01 local reboot) and when relevant profile/device settings change. No additional
UTC-midnight rotation occurs. A HeatHistory service restart resumes the latest
compatible files and energy totals from the same system boot, without integrating
across the recording gap. File and sample timestamps remain UTC. If active files
alone exceed the limit, recording stops with an error rather than deleting them.

## Install as a PC system service

Run from the project directory on the PC:

```bash
./Install.sh
```

The installer requires sudo and installs to `~/.local/share/HeatHistory` on the internal PC drive. Existing configuration and data there are preserved. The Python modules and `Versions.txt` are updated. The source project retains sources, installer, documentation and tests, without runtime data. `CreateCharts.py` is imported by the combined program and is required.

The system-wide `HeatHistory.service` starts at boot without login, but runs as the invoking user. Program and data do not require the project SSD. Run `Install.sh` again after source changes.

```bash
systemctl status HeatHistory.service
journalctl -u HeatHistory.service -f
sudo systemctl restart HeatHistory.service
```

Browser: `http://localhost:8091/` (tablet: `http://<PC-IP>:8091/`).
Configuration: `~/.local/share/HeatHistory/config.json`.
CSV, profile snapshots and state files default to `~/.local/share/HeatHistory/data/`.
HeatHistory records only `HeatProfileAq<number>` with `Measurement: Temperature`; battery profiles such as `LaptopCharge` are not aquarium heating histories.

## Start

Python 3.10 or later is required. HeatHistory uses only the Python standard library.

From the repository root:

```bash
cd HeatHistory
cp config.example.json config.json
python3 HeatHistory.py
```

`config.json` sets Dashboard address, UDP wait/validity limits and output directory. Relative paths refer to the HeatHistory project directory. `Ctrl+C` stops the logger.

Logs and profile snapshots default to `data/<Profile-ID>/`. This PC runtime data is not written into firmware or device filesystems.

## Features

- Interactive CSV chart report with synchronized zoom: `python3 CreateCharts.py`.
- Continuous event-driven recording with open CSV files.
- Per-profile error handling and automatic recovery.
- Semantic profile revisions and an initial row in every new log.
- No profile synchronization.

`Versions.txt` separates the HeatHistory application version (`App`) from the
recording format version (`Data`). `Data` identifies the CSV and JSON metadata
layout; it changes only when that stored format changes, not as records are added.

## MAC assignments

HeatHistory resolves required functions to physical UDP IDs through MAC identities in the central device registry. Profile/CSV keys remain function-based. Pending swaps or missing assignments pause recording. Device web pages are not queried.

UDP version 2 reads the named numeric status field selected by the profile's `Measurement` (default `Temperature`). Missing, invalid or stale values pause recording. Explicitly selecting the default field does not change the profile signature; selecting a different measurement does.

External sources in the Dashboard registry are also resolved. A missing source blocks only its affected profile. The CSV format is designed for aquarium heating profiles `HeatProfileAq<number>` with floor/main heating, so `LaptopCharge` is not recorded as an aquarium history.

## Power, energy and room temperature

In addition to the unchanged temperature/switching CSV logs, HeatHistory writes per-profile `Energy/*.energy.csv` files. These retain reported main-heater watts, a separate correction factor and corrected watts, physical device IDs, source times and boot counters, water/room temperatures, integrated Wh and coverage information. Every received main/floor status is processed; cached observations are not repeatedly counted as new measurements.

Main energy uses trapezoidal integration between adjacent valid power observations. This estimates energy from reported watts rather than reading a hardware energy counter; switching within a publication period and meter-window alignment cannot be reconstructed exactly. Valid zero watts mean zero energy. Missing/invalid power samples are held provisionally for up to 60 seconds and corrected by linear interpolation on recovery within that limit. Device/clock discontinuities and time beyond the limit remain unknown. Floor energy is estimated from its reported switching state and configured rated power.

Optional configuration:

```json
{
  "FloorPowerWatts": {"HeatProfileAq1": 60, "HeatProfileAq2": 40},
  "RoomTemperatureSource": "GN-Pi",
  "PowerFactors": {},
  "EnergyMaxIntervalSeconds": 15
}
```

`PowerFactors` maps a physical `MAC@ServiceId` to a positive factor. Original reported watts remain untouched; applied factors and corrected values are separate. Calibration is initially labelled provisional. Calibration coefficients captured when starting the series are retained in session metadata. After device calibration, apply corrections to the appropriate time period: correct older uncalibrated readings without applying the same correction again to readings already using the new device calibration.

Room temperature comes from the assigned Pi's `RoomTemperature` measurement. An unavailable room reading is unknown and does not stop other logging. Browser charts include room temperature, reported/corrected main power, estimated floor power and energy.

### Start a new series and archive existing data

```bash
./Install.sh --new-series
```

The installer stops the service, moves the original `data/` tree unchanged to `~/.local/share/HeatHistory/archive/<UTC timestamp>/data/`, saves the previous configuration and starts with an empty `data/` directory. The browser then shows only the new series. Without `--new-series`, existing logs continue. Archived data remain viewable explicitly with `CreateCharts.py --data-dir <archive path>`.

24 hours is an initial review period, not an automatic stop: the service continues recording. PI initialization requires enough actual heating cycles, documented calibration factors and adequate measurement coverage.

Displayed energy totals carry forward across all sessions for the aquarium in the active data directory. Restarting the service therefore does not reset the chart totals; unknown energy during recording gaps is not added.

At window widths up to 900 pixels, the legend appears below the chart; wider windows keep it on the right. Rotating the tablet updates its position automatically.

## PI observation mode

An optional profile `Pi` object with `Mode: "Shadow"` enables a controller that calculates every ten seconds while the existing hysteresis controller continues switching. Heat demand is allocated to the floor first, with remaining demand allocated to the main heater. Reported relay requests are hypothetical and never sent to hardware or peers. Missing `Pi` or `Mode: "Off"` disables observation. Active operation is explicitly selected with Mode Active, as described below.

The integral is bounded, conditionally integrated at saturation and updated solely from temperature error using Ki = Kp / Ti. Unbridgeable measurement loss, manual overrides, disabled schedules, gaps and peer restarts discard learning windows. A missing floor peer removes floor power from allocation and prevents learning; missing room temperature disables model learning/feed-forward without stopping PI calculation.

Slow learning uses actual measured main watts and measured constant floor watts with confirmed floor state. Valid zero watts remain zero. Continuous one-hour windows weakly adjust effective heat loss at nearly equal initial/final water temperature; excursions of at least 0.25 K permit bounded capacity/gain adaptation. Each accepted estimate contributes 5 %, and estimates must remain within half to twice the configured seed. Configured delay aligns past energy with subsequent temperature for learning. Learned values remain in RAM in both Shadow and Active modes and are recorded as telemetry; restart/configuration changes restore profile seeds. Ti remains the configured slow seed. This is not a validated closed-loop plant identification.

HTTP/UDP `HeatPi` status reports requested and actual watts, P/I/feed-forward, hypothetical floor/main requests, model parameters and learning state. Observation telemetry does not trigger additional UDP broadcasts. The floor peer is identified explicitly by MAC@Service-ID and channel name; update this binding after a channel/device swap.

HeatHistory records separate `Pi/*.pi.csv` observations and JSON configuration metadata per aquarium. Only the assigned main device is accepted, and unchanged PI sample counters are deduplicated. Existing energy/temperature CSV formats stay unchanged. Charts add two aligned panels for requested versus actual power and P/I/feed-forward. Missing data/restarts remain gaps. `./Install.sh` installs the logger without resetting the recording series; do not use `--new-series`.

Compact UDP messages are expanded using requested descriptions. Description replies do not count as fresh measurements; see the [protocol](../Libs/GnAqPrjsLib/UdpStatus.md).

### Active PI control

`Pi.Mode: "Active"` controls the main heater from calculated demand and requires `MainDevice` (physical MAC with `@4`). The main switch sends floor requests with its regular UDP status. The floor endpoint accepts only the configured device, profile and channel binding. Without a valid request for 60 seconds, floor heating falls back to ON; schedules and manual overrides retain precedence. If the floor status is unavailable, demand is assigned to the main heater alone. Missing or invalid temperature/power inputs are held until the last valid sample reaches 60 seconds of age (`measurement-held`); integral updates and learning continue using the held readings. Without a prior valid sample, or after expiry, the main heater uses `NoTempOn`. Physical heater thermostats remain unchanged.

HeatHistory records Active and Shadow in separate PI files. Learned model values remain in main-switch RAM; a restart restores profile values.

Chart labels use English. Water and Room share the temperature row. Power shows Main (measured power) and Floor (measured fixed floor power × switch state). Energy shows integrated Wh for Main and Floor. PI demand contains Total and Act (current delivered total power). PI components contains Proportional, Integral and Heat-loss estimate; expand the explanation above the charts for details.

A red dashed **Setpoint** line shows the applicable PI target in the temperature plot. Target changes appear as steps. Each PI session uses its recorded configuration; periods without a recorded target remain blank.

HeatHistory accepts both `MainSw` and `HeaterMain` for the main role. Changing that spelling does not change assignment or energy integration.

During `measurement-held`, learning uses approximate energy from the last valid power readings up to the 60-second limit. These periods are identified by the diagnostic reason and are not additional actual measurements.

The browser receives selected CSV records unchanged from `/api/history.csv`. A small JSON manifest supplies only the range, profile/segment mapping and setpoints. Numeric conversion, validity, gaps and energy calculations run in the browser. `/api/history` remains compatible for existing diagnostic/migration tools. SQLite is benchmarked separately with copies; live CSV recording remains unchanged.

Missing or rejected temperature samples are omitted; the chart connects valid
points without creating additional measurements. The feeder rejects changes above
2 °C relative to the last accepted value. Two consecutive readings within 2 °C of
each other confirm a new level; an intervening read failure cancels confirmation.
The DS18B20 power-on value of 85 °C is always rejected.

Missing power readings are held provisionally for at most 60 seconds (`held`). A
valid endpoint within that limit corrects the interval by linear interpolation
(`interpolated`). The correction is appended as signed interval energy; an individual
interval can therefore be negative while correcting earlier provisional energy.
Raw readings and validity flags are preserved. Device/clock discontinuities and
time beyond the limit remain unknown. Valid watts continue to count when only
temperature is missing. Provisional anchors are not restored after a service reboot.
