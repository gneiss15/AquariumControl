<!-- README.md, Version: 2.11 -->
# Aquarium Dashboard

[Deutsch](README.DE.md)

A shared overview and control interface for compatible aquarium devices. Outputs are grouped by aquarium; shared outputs can appear in more than one group. Devices broadcast current states and measurements such as temperature, voltage, current and power to DashboardServer over UDP. The server forwards the data to browsers, which do not query devices directly for these updates.

## Hardware and documentation status

Dashboard and the MultiTimer program run on the same Raspberry Pi. MultiTimer controls the connected eight-channel relay board. Photos and the available description of the relay board, power supply and enclosure are in [MultiTimer – hardware](../MultiTimer/Hw/README.md). **Coming soon: the hardware description is being completed.**

## Build and run

The native server requires Python 3.10 or later for the version generator, a C++20 compiler, Make and the json-c development library. Run these commands in the Dashboard directory on the Raspberry Pi:

```bash
make
./DashboardServer 8090 .
```

Open `http://PI-HOST:8090/`, replacing `PI-HOST` with your Pi's hostname or address. Use HTTP consistently for Dashboard and device pages: browsers restrict HTTP content embedded in an HTTPS page.

`Versions.txt` is the version source. The Makefile generates `Version.inc.h` and `data/Version.json`. Server-version or server-source changes require a rebuild and service restart. A Data-version change takes effect through `data/Version.json` without rebuilding the server.

A Python static HTTP server can display static pages, but does not provide the Dashboard APIs or central-data editing.

### systemd service

The default Pi installation directory is `/home/pi/Prjs/Dashboard`. Set `RASPI_PRJS_DIR` when using a different parent directory. Build before installing the service:

```bash
export RASPI_PRJS_DIR="${RASPI_PRJS_DIR:-/home/pi/Prjs}"
cd "$RASPI_PRJS_DIR/Dashboard"
make
sudo ./Install.sh
```

`Install.sh` installs and enables `DashboardServer.service`. It also removes old Dashboard startup/watch entries from `/etc/crontab` and replaces a previously installed `aquarium-dashboard.service`.

```bash
systemctl status DashboardServer.service
journalctl -u DashboardServer.service -f
```

After subsequent server changes, rebuild in that directory and restart:

```bash
make
sudo systemctl restart DashboardServer.service
```

## Configuration and files

Device-specific configuration and runtime data are not distributed with the public repository; see [build/private configuration](../BUILD-SECRETS.md). **Coming soon:** complete initial-setup instructions and generic configuration examples.

| File | Purpose |
| --- | --- |
| `Devices.json` | Initial device list, addresses, display settings, grouping and output order. Supply your own configuration. |
| `data/DeviceAssignments.json` | Authoritative runtime device/function and endpoint assignments; also retains device-editor settings and external-source identities. |
| `data/Times.json` | Central schedules, intervals and per-device channel assignments. |
| `data/HeatProfiles.json` | Central profile definitions and their global revision timestamp. |
| `data/HeatProfiles.sync.json` | Runtime profile synchronization status. |
| `data/FeedbackMonitors.json` | Runtime feedback-monitor rules. The root `FeedbackMonitors.json` supplies initial defaults only. |
| `data/Version.json` | Generated Data version, read at runtime. |
| `data/Layouts.json` | Retained Pi runtime file; the current web interface does not use it. |
| `index.html`, `Index.css`, `Dashboard.js` | Responsive browser interface. |
| `StatusClient.js` | Device initialization and live status delivery from DashboardServer. |
| `DashboardServer.cpp` and related C++ sources | Native web server, central stores, assignments and monitoring. |
| `Translations.json` | English/German interface text. |

### Device display settings

`Devices.json` supports:

- `Name`: display name; changing it does not require a device firmware update.
- `BaseUrl`: device address. Use your own fixed addresses/ports.
- `Group`: aquarium group, or `System` for shared status sources.
- `GroupViews`: filtered views of one device in several aquarium groups. Filters use technical channel names from status `Name`, not numeric action IDs.
- `OverviewOutputOrder`: output order within each aquarium; missing configured entries retain an empty row, followed by additional outputs.
- `StatusOnly: true`: status display without a configuration page or channel controls.
- `ConfigUrl`: configuration page available before the device API responds; the device API's value takes precedence afterwards.
- `FrameWidth` and `MinimumScale`: embedded device-page width and minimum automatic scaling; smaller screens can scroll horizontally.
- `PowerLabel`: translation key for the power row, such as `PowerLight` or `PowerHeater`.
- `Enabled: false`: temporarily disable an entry.
- `SyncOnlyDevices`: include devices in profile distribution without an overview card.

`HardwareNames` maps physical MAC addresses to enclosure labels. `UseHardwareName` displays that label for reserve functions. Concrete aquarium functions retain their functional names. Display names do not change physical identities, GPIO assignments or calibration.

The overview adapts to desktop, tablet and phone screens. Channel labels omit the aquarium name already shown in the group title. The selected section is retained in the browser's `MNr` cookie; unavailable sections fall back to the overview. Dashboard's language selection is separate from embedded device pages.

Tables support ascending/descending sorting without changing stored order. In the schedule table, `#` restores manual order. Column widths fit their contents; drag a header boundary to resize, double-click to restore automatic width. Left/Right and Enter provide keyboard equivalents. Manual widths are retained in the browser; narrow screens use cards for schedules and horizontal scrolling for assignment tables.

## Schedules, profiles and synchronization

The Pi holds the central runtime data. The schedule page edits times, weekdays, channels, states and intervals. Only devices whose schedule content changes receive a new timestamp and update. Saving unchanged content does not distribute another revision.

Profiles live in `data/HeatProfiles.json` with one global UTC `TimeStamp`. Per-device channel assignments live separately in `data/Times.json`, each with its own timestamp. A channel-assignment update applies only to that device; a global profile change is distributed to all schedule participants with each device's own channel assignments. Empty or incomplete profile lists do not delete central profiles. A profile cannot be deleted while a channel still refers to it.

Devices continue to receive their combined `Channels.json` structure with `Profiles` and `Channels`. `/api/times/sync` accepts it with schedule data; device updates use `/api/v1/times`. No separate profile message type is needed. Older TmrSw payloads without intervals leave central intervals unchanged.

The profile editor changes a channel's role and profile without changing its hardware structure or identity. The profile's **Source** (`TempId`) identifies the sender. **Measurement** identifies a numeric field in its status, such as `Temperature` or `BatteryPercent`; an empty measurement field means `Temperature`. For example, a laptop-charging profile uses source `LaptopBattery` and measurement `BatteryPercent`. With `NoTempOn: false`, a missing valid measurement keeps the output off. Save/distribute after editing.

The revision timestamp identifies the last profile change, not the current time. Synchronized participants are green. Only unsynchronized participants and actual errors are red; the revision display itself is neutral.

Validate a profile file from the Dashboard directory:

```bash
./tools/ValidateHeatProfiles.py data/HeatProfiles.json
```

This checks structure, types, required fields, hysteresis limits, unique IDs and the optional timestamp. Formatting changes alone do not create a new semantic profile revision.

## Transfer to the Raspberry Pi

Use `CopyToRasPi.sh` from an interactive terminal. From the AquariumControl repository root:

```bash
./Dashboard/CopyToRasPi.sh
```

The script resolves local paths from its own location. Override `RASPI_HOST` and `RASPI_PRJS_DIR` for a different target; the default target is `pi@gn-pi:/home/pi/Prjs/Dashboard/`.

Before uploading, it downloads the full Pi `data/` directory to `generated/raspi/`. It compares readable copies of **only** `Times.json` and `HeatProfiles.json` using Beyond Compare (BCO): the local build is on the left, the Pi download on the right. Newer Pi timestamps can be accepted automatically only when the associated content is otherwise identical.

Copy desired Pi changes to the left build side in BCO. Close BCO and press Enter. The script validates the result and saves it to local `data/`, backing up the previous local data under `generated/data-before-sync/` and retaining at most three backups. It then asks for explicit upload confirmation. Download or validation failures stop the upload. Declining leaves Pi files unchanged; confirmed local edits remain in local `data/`.

The normal upload transfers sources/static files, the two central JSON files and generated `data/Version.json`. It preserves Pi runtime files such as assignments, `Layouts.json` and `HeatProfiles.sync.json`. The locally compiled executable is not transferred. Rebuild on the Pi and restart the service after server changes.

For sources/static files and the generated version only, preserving central times and profiles:

```bash
./Dashboard/CopyToRasPi.sh --sources-only
```

This still downloads a Pi snapshot first, but skips the central-data comparison and upload. Existing assignments remain intact. Reload the browser after web changes; use Ctrl+F5 if it retains an older interface.

## Device and endpoint assignment

Physical identity is `MAC@ServiceId`; an endpoint adds a fixed channel number: `MAC@ServiceId/ChannelId`. Function names, physical identities and display labels are separate. See the [identity and swap contract](../Libs/GnBasicLib/DeviceIdentity.md).

The expandable device-assignment area provides whole-device and endpoint swaps. **Disconnect consumers from both affected devices before swapping; leave the controllers powered.** A device can only be replaced by a functionally compatible device. Function hostname and AP/FTP credentials follow the feeder function; hardware parameters remain with the physical device. Both ESPs restart.

A hardware endpoint is an individual switching output or temperature source. Swap functions only between functionally compatible endpoints: outputs with outputs, temperature sources with temperature sources. For example, a MultiTimer output can exchange its function with another MultiTimer output or a NOUS/Shelly socket. The table shows each endpoint and its current function; MultiTimer output numbers correspond to sockets 1–8. Swapping a feeder temperature source leaves its feeding schedules unchanged.

Names, roles, profiles, schedules and intervals follow the endpoint function. GPIOs, buttons, polarity, physical position, calibration, device hostname and credentials stay with the hardware. A stale temporary override is discarded when the function changes. Feeder feeding and temperature sensing remain distinct functions.

Select the two endpoints and confirm the swap. Further assignments and central schedule/profile edits are blocked until all participating devices confirm the new assignment. An interrupted transfer resumes after a restart. HeatHistory records heating functions at their assigned device channels.

After completion, open or reload the MultiTimer page. Returning to the page checks for configuration changes and reloads changed assignments and schedules. Unsaved edits are retained with a change notice.

After a function swap, previous PC filesystem images are stale. Dashboard cannot update PC files. From the repository root, explicitly synchronize the device state before rebuilding the affected filesystem images:

```bash
python3 tools/ReadDeviceStates.py --apply
```

The separate device JSON editor retains existing identities, functions, hardware types and Pi services. New active devices require live identity checks. Removal/deactivation is blocked while schedules or temperature/profile references still use the hardware. Editor settings are saved with runtime assignments, and source transfer preserves them. JSON uses aligned formatting and does not escape `/` unnecessarily.

## Live status and actions

Data path: device → UDP version 2 → DashboardServer cache → SSE → browser.

Devices publish at least every five seconds and on relevant changes, using broadcast port 44444. DashboardServer requests all publishers at startup and individual devices when cache data is absent or stale. Device metadata comes from `/api/v1/device`; the browser shares one `/api/events` EventSource connection for live updates. `/api/device-status/<ID>` provides the initial state and HTTP-polling fallback. Browsers do not open individual device WebSockets or poll devices for status.

On a stream failure the browser polls the server cache once per second. EventSource reconnects automatically; a silent connection is also renewed after 15 seconds. Reconnection delivers a snapshot; late polling responses cannot overwrite newer pushed data. After 20 seconds without valid device UDP, the device is offline and stale measurements are hidden. Missing feedback is unknown, never an assumed OFF state. Server limits of 32 streams and 256 KiB pending data per stream prevent slow browsers from blocking other work.

Only UDP version 2 is accepted. Measurements are named fields in `Status`; old root-level `Value` and `Temperature` are not read. Device capabilities and source identity remain independent of the selected measurement field.

The local Pi status source reports CPU temperature, load averages for 1/5/15 minutes and `vcgencmd get_throttled` flags for current or past undervoltage, frequency limiting, throttling and temperature limits. An unavailable device needs only its Offline status, without a duplicate generic error message.

For channels with `HasOutput: true`, clicking ON/OFF invokes a short button action; clicking the channel name opens details. Details offer short and supported long presses, with long presses requiring a second confirmation within four seconds. Devices execute their button-action path rather than accepting a direct output state. Their `Actions.ButtonPress` metadata specifies the API path and supported press types.

Device requests time out after six seconds; unreachable devices are reinitialized every ten seconds. A server/network status bar supports automatic retry and manual retry. Waking the page triggers an immediate availability check. Device-list changes are detected and reload the browser page. Device `DataVersion` is shown and used to refresh embedded configuration pages.

## External measurement sources and laptop display

Registered senders without configuration APIs are listed under `ExternalSources` in authoritative `data/DeviceAssignments.json`, as function name → `MAC@3`. They are not managed or swapped as feeders. Unknown senders are excluded. Changes require a new registry revision and distribution to MultiTimer/TimerSw; edit this table with Dashboard stopped, as the device editor does not handle it.

An external laptop sender runs on the laptop. It sends `Status.BatteryPercent` (0–100), `Status.AcOnline` and `Status.BatteryState` in UDP version 2. Dashboard displays the registered `LaptopBattery` source as a laptop under System, including its battery percentage. Invalid percentages appear as “—”; after 20 seconds without a packet the source is Offline. No configuration API on the laptop is required.

`LaptopSw` remains a separate switching function. A charging profile may use `LaptopBattery` / `BatteryPercent` with 40/80 thresholds and `NoTempOn: false`, keeping the output off when the measurement is missing. This is an example configuration, not a mandatory aquarium component.

## Feedback monitoring and browser sound

The separate feedback-monitor JSON editor is under device assignment. Rules specify `SwitchFunction`, independent `Source`, `StatusKind`, boolean `FeedbackField` and `GraceSeconds` (1–3600). The actual reported channel state, including manual overrides, is compared with independent feedback. The monitor follows endpoint reassignment and does not switch anything itself.

For example, compare `LaptopSw` with `LaptopBattery`, `Status.Kind: LaptopPower`, field `Status.AcOnline`, with 60 seconds grace. Charging state alone is unsuitable: a laptop may have AC power without charging because of its charge limit.

Only continuously different, valid values no older than 20 seconds trigger a fault after the grace period. Agreement, direction changes, reassignment and missing/invalid values reset the timer. Missing or stale feedback is shown as missing; fresh but unusable feedback is incorrect. Neither is treated as false or triggers a mismatch sound. The charging profile's missing-measurement policy remains separate.

Rules are saved atomically to `data/FeedbackMonitors.json`. The editor loads them on opening or request and detects concurrent edits; it does not poll configuration continuously.

Enable the warning sound explicitly in the browser; this plays a test tone. An unacknowledged confirmed fault produces three short tones every 30 seconds. Acknowledge silences it until recovery or a new fault type, retaining the visual fault. Disable turns sound off for that browser. Stale Dashboard data suppresses repeated tones. Set volume on the tablet/browser; browser audio permission remains necessary.

The `AquariumDashboard.FeedbackSound` cookie stores the choice for one year (`Path=/`, `SameSite=Lax`). At page load and daily, a remaining lifetime of at most 30 days renews it for both ON and OFF. Legacy 0/1 values are retained; deleted/invalid cookies do not create a preference automatically.

With a saved ON choice, the page silently attempts audio resumption. If the browser blocks it, press **Enable warning sound** to release audio. An explicit enable/release plays the same test signal as the alarm; automatic resumption is silent. OFF remains off across reloads. Cookie preferences apply only to the same browser/host and may be lost in private browsing or after clearing cookies.

A real listening test in the Fully Kiosk Browser remains pending. Test enable/tone, reload/resumption, disable and reload/silence on the actual tablet; browser sound behavior cannot be established by automated checks alone.
