# HeatHistory

[Deutsch](README.DE.md)

Document version: 1.19
Updated: 2026-10-21

HeatHistory is a standalone PC application. It passively observes aquarium heating control and records CSV histories per profile on the PC. It does not change configuration.

## Profile and status validation

HeatHistory reads central profiles and channel assignments from Dashboard through `/api/heatprofiles`, `/api/times` and `/api/device-registry`. `TempId` identifies the logical temperature source. The unique `HeaterFloor` and `HeaterMain` channels are found across all devices by their profile assignment. The physical temperature source follows endpoint assignment when sensors are swapped.

Temperature and switching states are read directly from device UDP status packets on port 44444, without querying device web pages or `/api/v1/status`. Each packet contains device ID and status; the feeder supplies temperature in Celsius. By default, HeatHistory waits up to eight seconds for current packets and accepts status up to 15 seconds old. These limits and Dashboard address are in `config.json`. The PC must be in the same broadcast network as the devices; its firewall must allow incoming UDP port 44444.

Missing central profiles/assignments, required UDP packets, valid temperature or switching states pause only the affected aquarium. Errors include a UTC timestamp. Recording resumes automatically after successful validation. Dashboard HTTP errors also show the duration of the failed request.

JSON formatting changes do not count as profile changes. A semantic profile change creates a snapshot and a new CSV. Its first data row contains the state at the time the log starts.

The CSV header and column order are fixed:

```text
Time;Temp;Floor;Main
```

A row is written when temperature, Floor or Main state changes from the previous record.

## Generate charts

The chart generator reads all CSV logs under `data/`, parses ISO timestamps as UTC and creates one combined chart per log. Temperature occupies the large upper panel; binary `Floor` and `Main` states each have a compact panel below. All panels share a time axis and each has its own left axis. State axes show the German labels `Aus` (Off) and `Ein` (On). Mouse-wheel or box zoom, panning and presets (1/6 hours, 1/7 days, full range) control all three panels. Double-click resets the shared time range.

CSV files remain unchanged; the report embeds their measurements. The browser loads Plotly.js 4.1.1 from the Plotly CDN to render interactive charts, so opening a report requires Internet access. The Python generator requires no extra packages.

```bash
python3 CreateCharts.py
```

The interactive HTML report defaults to `data/HeatHistoryCharts.html`. Open or print it in a browser; the chart toolbar also offers export functions. Override paths with:

```bash
python3 CreateCharts.py --data-dir data --output data/HeatHistoryCharts.html
```

The time axis is UTC. Since CSV rows are written only when temperature or state changes, the temperature chart connects recorded measurements; state curves hold their last value until the next change.

## Charts directly in the browser

`python3 HeatHistory.py` starts the logger and chart server in one process. At `http://localhost:8091/`, every request/reload generates charts from the current CSV data without a preceding HTML build. A separate HTTP thread works independently of Dashboard/UDP waits. `Ctrl+C` stops both.

For tablet access, use `python3 HeatHistory.py --host 0.0.0.0` and open `http://<PC-IP>:8091/`; allow TCP port 8091 through the PC firewall if needed. `--port` selects a port. The data directory comes from logger configuration. `--no-server` runs only the logger; `--once` starts no server. The separate `python3 CreateCharts.py --serve` is also available, but must not use the combined process's port. Missing/invalid logs return HTTP 503; reload after resolving the issue. Raw files/configuration are not served. Plotly requires the CDN.

## Install as a PC system service

Run from the project directory on the PC:

```bash
./Install.sh
```

The installer requires sudo and installs to `~/.local/share/HeatHistory` on the internal PC drive. Existing configuration and data there are preserved. Only the two Python modules and `Versions.txt` are updated. The source project retains sources, installer, documentation and tests, without runtime data. `CreateCharts.py` is imported by the combined program and is required.

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

`config.json` sets Dashboard address, polling interval, UDP wait/validity limits and output directory. Relative paths refer to the HeatHistory project directory. `Ctrl+C` stops the logger.

Logs and profile snapshots default to `data/<Profile-ID>/`. This PC runtime data is not written into firmware or device filesystems.

## Features

- One profile/status pass: `python3 HeatHistory.py --once`.
- Interactive CSV chart report with synchronized zoom: `python3 CreateCharts.py`.
- Continuous recording with configurable polling interval.
- Per-profile error handling and automatic recovery.
- Semantic profile revisions and an initial row in every new log.
- No profile synchronization.

## MAC assignments

HeatHistory resolves required functions to physical UDP IDs through MAC identities in the central device registry. Profile/CSV keys remain function-based. Pending swaps or missing assignments pause recording. Device web pages are not queried.

UDP version 2 reads the named numeric status field selected by the profile's `Measurement` (default `Temperature`). Missing, invalid or stale values pause recording. Explicitly selecting the default field does not change the profile signature; selecting a different measurement does.

External sources in the Dashboard registry are also resolved. A missing source blocks only its affected profile. The CSV format is designed for aquarium heating profiles `HeatProfileAq<number>` with floor/main heating, so `LaptopCharge` is not recorded as an aquarium history.
