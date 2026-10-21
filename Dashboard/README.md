<!-- README.md, Version: 2.13 -->
# Aquarium Dashboard

[Deutsch](README.DE.md)

Dashboard provides a shared overview and control interface for the aquarium devices. The overview groups switching outputs and measurements by aquarium. Shared outputs can appear in more than one aquarium group.

Devices broadcast current states and measurements such as temperature, voltage, current and power over UDP. DashboardServer receives the data and sends updates directly to browsers. Browsers therefore do not need to retrieve measurements from the devices at regular intervals.

## Hardware and documentation status

Dashboard and the MultiTimer program run on the same Raspberry Pi. MultiTimer controls the connected eight-channel relay board. Photos and a description of the relay board, power supply and enclosure are in [MultiTimer – hardware](../MultiTimer/Hw/README.md).

**Coming soon: the hardware description is being completed.**

Device-specific configurations, credentials and runtime data are not included in the public repository. Supply your own configuration and signing material as described in [build and private configuration](../BUILD-SECRETS.md). **Coming soon:** complete initial-setup instructions and generic configuration examples.

## Overview and operation

The section selector opens the overview, schedules and device configuration pages. The last selected section is saved in the browser and reopened after reloading. If it is unavailable, the overview appears. Dashboard's language is selected independently of embedded device pages.

The overview shows devices with their connection status, assigned measurements and outputs. An unreachable device is shown as **Offline**; stale measurements are hidden. The **System** group contains shared devices and status displays, such as the Raspberry Pi and a registered laptop.

The layout adapts to PCs, tablets and phones. On narrow screens, schedules appear as cards. Device configuration pages are scaled to fit the available space; horizontal scrolling is available at the minimum size. Scaling and reloading controls are beside the section selector.

### Operate switching outputs

- Tap **ON** or **OFF** to trigger the device's short button action.
- Tap the channel name to open its detail view.
- The detail view offers short and, where supported by the device, long button actions.
- A long button action requires a second confirmation within four seconds.

The action corresponds to operating the device's button. Consult the device instructions for the effects of short and long presses.

### Use tables

Click a column heading to sort in ascending or descending order. Sorting does not change the stored order. In the schedule table, **#** restores the manual order.

Columns fit their widths to their contents. Drag a column boundary to resize it; double-click to restore automatic width. Left/Right adjusts the width by keyboard, and Enter restores automatic sizing. Manual widths are saved in the browser.

## Schedules and profiles

The **Times** page edits switching times, weekdays, channels, states and intervals. Feeding times are also managed centrally. Changes are distributed to the affected devices. Unreachable devices receive pending changes when their connection is restored.

The **Heat profiles** page manages central profiles and their assignment to device channels. A profile can only be deleted when no channel refers to it. Roles and profiles can be changed without changing the device's hardware assignments.

A profile includes:

- **Source:** the measurement sender, such as a temperature source or `LaptopBattery`.
- **Measurement:** the field to evaluate, such as `Temperature` or `BatteryPercent`. An empty field means `Temperature`.
- The profile's switching thresholds and settings for missing measurements.

Source and measurement are separate settings: `LaptopBattery` names the sender, while `BatteryPercent` identifies its battery level. The measurement field belongs to the profile, not the channel assignment. After editing, select **Save / distribute**.

The revision timestamp identifies the last profile change. It is not a running clock and uses a neutral color. Synchronized participants are green; pending synchronization and errors are red. Save/distribution notices disappear after successful completion.

Central data is held on the Raspberry Pi. Each device receives its own schedules and channel assignments together with the central profiles. A channel-assignment change affects only the corresponding device. Central profile changes are distributed to all participating switching devices. Saving unchanged content does not trigger another distribution.

## Configure devices

`Devices.json` contains the initial configuration for devices, addresses and display settings. Adjust addresses and assignments for your own installation.

| Setting | Meaning |
| --- | --- |
| `Name` | Device display name. |
| `BaseUrl` | Device address, including a port where needed. |
| `Group` | Aquarium group, or `System` for shared displays. |
| `GroupViews` | Filtered views of one device in several aquarium groups. |
| `OverviewOutputOrder` | Output and measurement order per aquarium. Missing configured entries retain an empty row. |
| `StatusOnly` | Status-only display without a configuration page or channel controls. |
| `ConfigUrl` | Path to the device configuration page. |
| `FrameWidth`, `MinimumScale` | Display width and minimum automatic scaling of embedded device pages. |
| `PowerLabel` | Power-display label, for example for lighting or heating. |
| `Enabled` | Set to `false` to disable an entry. |
| `SyncOnlyDevices` | Include devices in profile distribution without an overview card. |

Group filters and output order use technical channel names from the status field `Name`. Actions use the numeric channel ID. An output such as a shared air supply can be displayed in several aquarium groups.

`HardwareNames` maps MAC addresses to physical device labels. Reserve functions with `UseHardwareName` show that device label; assigned aquarium functions show their function name.

## Device identity and function swaps

Each physical device is identified by its MAC address and service ID. Its assignment to a function, such as feeding or lighting, can be changed in **Device swap**. Schedules and channel roles remain assigned to their function.

**Before a swap, disconnect consumers from both affected devices; leave the controllers powered.** A device can only be replaced by a functionally compatible device.

When swapping whole devices, the function hostname and AP/FTP credentials follow the function. Hardware settings remain with the physical device. Both affected ESPs restart. The operation is complete when both devices have confirmed the transfer. Interrupted operations resume after a restart.

Further details are in [device identity and function swaps](../Libs/GnBasicLib/DeviceIdentity.md).

### Hardware endpoints and channel swaps

A hardware endpoint is an individual switching output or temperature source. **Device swap** can exchange the functions of two functionally compatible endpoints, for example two MultiTimer outputs, or a MultiTimer output and a NOUS/Shelly socket. Switching outputs can only be swapped with switching outputs, and temperature sources with temperature sources.

The table shows endpoints with their current functions. MultiTimer output numbers correspond to sockets 1–8. Feeding and temperature sensing on a feeder are separate functions; swapping a temperature source leaves feeding schedules unchanged.

After disconnecting the consumers, select the two endpoints and confirm the swap. Further assignment changes and central schedule/profile changes are blocked during the transfer. The operation is complete when all participating devices have confirmed the new assignment.

Function name, role, profile, switching times and intervals are assigned to the new hardware. Aquarium groups and temperature-source references follow the function. Hardware settings, calibration, device hostname and credentials stay with the physical device. A temporary manual override is cleared when the function changes; the output state is calculated from the new function. HeatHistory records heating functions at their assigned device channels.

After the swap completes, open or reload the MultiTimer page. Returning to or reactivating it checks the configuration and reloads changed assignments and schedules. Unsaved edits are retained with a change notice.

### Synchronize PC data after a swap

A function swap alone does not require a filesystem upload. If a filesystem is to be uploaded later, synchronize the PC data with the current device assignment before creating the image.

Run the following command from the AquariumControl repository root:

```bash
python3 tools/ReadDeviceStates.py --apply
```

Dashboard itself cannot access PC files. Previously created filesystem images may still contain the assignment from before the swap.

### Device editor

**Device assignment → Device swap → Device editor** opens the JSON editor for adding/removing devices and editing their metadata. It includes function, MAC address, IP address, port, device type, hardware label, aquarium group and activation status.

Existing device identities, functions and hardware types are protected; function changes use the swap dialog. Pi services cannot be changed in the device editor. Invalid or duplicate identities and changes during a pending assignment are rejected.

New active devices require suitable firmware, a configured filesystem and a fixed IP address. The server checks their actual identity and device type. `Active: false` allows entries to be prepared without contacting the device.

Removal or deactivation is only possible when no schedules or temperature-source/profile assignments use the device. A removed device must be decommissioned separately; removing it from the list does not erase data on the device.

The runtime configuration is saved in `data/DeviceAssignments.json` and preserved during source transfer. `Devices.json` supplies the initial values.

## External measurement sources and laptop display

Additional senders without device/configuration APIs are registered under `ExternalSources` in `data/DeviceAssignments.json`. An entry maps a function name to an identity of the form `MAC@3`. Only registered senders are evaluated.

Edit this table with Dashboard stopped. Changes require a new assignment revision and distribution to participating devices; the device editor does not edit this table. See [DeviceIdentity.md](../Libs/GnBasicLib/DeviceIdentity.md) for data-format details.

The laptop battery sender runs on the laptop and sends its battery level as `Status.BatteryPercent`, in percent. It also reports AC power through `Status.AcOnline` and battery state through `Status.BatteryState`. A registered `LaptopBattery` sender appears under **System** as a laptop. Invalid percentages are shown as “—”. After 20 seconds without a valid status packet, the sender is Offline and stale measurements are hidden.

For example, a charging profile can use source `LaptopBattery`, measurement `BatteryPercent` and thresholds 40/80. With `NoTempOn: false`, the assigned output stays OFF without a valid measurement. The associated switching output `LaptopSw` is shown as a separate function.

## Feedback monitoring and warning sound

Feedback monitoring compares the reported switching state with independent feedback. It does not switch outputs itself. Rules are edited in the JSON editor under **Device assignment → Feedback monitoring**.

| Field | Meaning |
| --- | --- |
| `SwitchFunction` | Switching function to monitor. |
| `Source` | Independent feedback source. |
| `StatusKind` | Identifier of the expected status message. |
| `FeedbackField` | Field containing boolean feedback. |
| `GraceSeconds` | Allowed mismatch duration, from 1 to 3600 seconds. |

For example, `LaptopSw` can be compared with AC-power feedback `Status.AcOnline` from `LaptopBattery`. `StatusKind` is `LaptopPower`; a possible grace period is 60 seconds. Whether the battery is charging is not sufficient feedback for AC power, as a charge limit can prevent charging.

Only continuously different, valid values no older than 20 seconds trigger a fault after the grace period. Agreement, direction changes, missing or invalid values and a new assignment reset the timer. The monitor also accounts for manual output overrides and follows channel swaps.

The display distinguishes **State confirmed**, **Checking state**, **State not confirmed**, **Feedback missing** and **Incorrect feedback**. Missing or unusable feedback is not treated as OFF and does not trigger a mismatch warning sound.

Rules are saved in `data/FeedbackMonitors.json`. The editor detects concurrent changes. `FeedbackMonitors.json` in the project root supplies the initial configuration when no runtime file exists.

### Operate the warning sound

**Enable warning sound** releases browser audio and plays a test tone. A confirmed, unacknowledged mismatch produces three short tones every 30 seconds. **Acknowledge warning sound** retains the visual fault and silences the sound until recovery or a new fault type. **Disable warning sound** turns sound off in this browser. Sound is not repeated without current Dashboard data.

The ON/OFF choice is saved in the browser for one year. With a saved ON choice, the page attempts to resume audio silently after loading. If the browser blocks this, **Allow warning sound** appears; tap it once. OFF remains off after reloading. Set volume and audio permissions on the tablet or in the browser. Clearing cookies or using private browsing may discard the saved choice.

Cookie lifetime is checked on loading and daily. When at most 30 days remain, the existing choice is renewed for one year. Automatic resumption does not play a test tone; explicit enabling or releasing does.

**Check on your own device:** enable sound, check the test tone, reload the page and check audio permission. Then disable sound and reload again; the page must remain silent.

## Live data and connection status

Data path: **device → UDP → DashboardServer → SSE → browser**. SSE is a persistent HTTP connection through which the server sends changes directly to the browser page.

Devices publish status at least every five seconds and on relevant changes, using UDP port 44444. One shared `/api/events` connection supplies the browser page with status data from all devices and external measurement senders. The server explicitly requests missing or stale device data.

When the push connection is interrupted, the browser queries DashboardServer's status cache once per second over HTTP. The push connection is restored automatically; a connection silent for 15 seconds is rebuilt. After reconnection, the browser receives the current data. Device data older than 20 seconds is treated as Offline.

On a WLAN or server failure, Dashboard displays a status bar with automatic retries and a Reload button. Waking or showing the page again triggers an immediate connection check. Changed device configurations are taken into account when reloading.

Device communication uses UDP version 2. Measurements are named fields in the `Status` object, such as `Temperature` or `BatteryPercent`. Technical interfaces are described in [DeviceIdentity.md](../Libs/GnBasicLib/DeviceIdentity.md).

### Raspberry Pi system status

The Pi reports CPU temperature and system load for 1, 5 and 15 minutes. Dashboard also shows undervoltage, frequency limiting, throttling and temperature limits from `vcgencmd get_throttled`. Both current conditions and conditions recorded since startup are included. An error block appears only when corresponding conditions are reported.

## Transfer and data backups

`CopyToRasPi.sh` reads back the full Pi data before a transfer. Start the script in an interactive terminal from the repository root:

```bash
./Dashboard/CopyToRasPi.sh
```

The default target is `pi@gn-pi:/home/pi/Prjs/Dashboard/`. Set `RASPI_HOST` and `RASPI_PRJS_DIR` for other targets. The script resolves local project paths from its own location.

Only `Times.json` and `HeatProfiles.json` are compared using Beyond Compare (BCO): the local build is on the left, the Pi download on the right. Copy desired Pi changes to the left side. After closing BCO and pressing Enter, the edited files are validated and written back to local `data/`. Newer Pi timestamps can be accepted automatically when the associated content is otherwise identical.

Local data is backed up before changes are applied; the three newest backups are retained. The script then explicitly asks for upload approval. Download or validation failures stop the upload. Declining leaves Pi files unchanged; confirmed local edits remain local.

The full transfer uploads sources, web files, `Times.json`, `HeatProfiles.json` and generated `Version.json`. Runtime device assignments and Pi-specific runtime files are preserved. The locally compiled server executable is not transferred.

For source/web changes without changing central schedules and profiles:

```bash
./Dashboard/CopyToRasPi.sh --sources-only
```

This also reads back a Pi snapshot first. The central schedule/profile comparison and upload are skipped. Reload the browser after web changes; use Ctrl+F5 if the displayed version is stale.

| Directory | Contents |
| --- | --- |
| `data/` | Local central data. |
| `generated/raspi/` | Full, unmodified readback of Pi data. |
| `generated/build-readable/` | Readable local comparison copies for BCO. |
| `generated/raspi-readable/` | Readable Pi copies of `Times.json` and `HeatProfiles.json`. |
| `generated/data-before-sync/` | Local-data backups made before applying changes. |

Profile files can be checked separately from the Dashboard directory:

```bash
./tools/ValidateHeatProfiles.py data/HeatProfiles.json
```

## Build and run the server

Requirements are Python 3.10 or later, a C++20 compiler, Make and the json-c development library. On the Raspberry Pi, in the Dashboard directory:

```bash
make
./DashboardServer 8090 .
```

Open `http://PI-HOST:8090/` in the browser, replacing `PI-HOST` with the Pi's hostname or address. Use HTTP for Dashboard and embedded device pages, as browsers may block insecure HTTP content inside an HTTPS page.

`Versions.txt` is the version source. The build generates `Version.inc.h` and `data/Version.json`. Changes to server sources or the Server version require a rebuild and service restart. A Data-version change takes effect through `data/Version.json` without rebuilding the server. The header displays Server and Data versions.

### systemd service

The default Pi directory is `/home/pi/Prjs/Dashboard`. Set `RASPI_PRJS_DIR` for a different parent directory. Build the program before installing the service:

```bash
export RASPI_PRJS_DIR="${RASPI_PRJS_DIR:-/home/pi/Prjs}"
cd "$RASPI_PRJS_DIR/Dashboard"
make
sudo ./Install.sh
```

The service is called `DashboardServer.service`. Check status and logs with:

```bash
systemctl status DashboardServer.service
journalctl -u DashboardServer.service -f
```

After server changes, rebuild on the Pi and restart the service:

```bash
make
sudo systemctl restart DashboardServer.service
```

## Important files

| File | Purpose |
| --- | --- |
| `Devices.json` | Initial device and display configuration. |
| `data/DeviceAssignments.json` | Runtime device/function assignments, endpoints and editor settings. |
| `data/Times.json` | Central schedules, intervals and channel assignments. |
| `data/HeatProfiles.json` | Central profiles with revision timestamp. |
| `data/HeatProfiles.sync.json` | Runtime synchronization status. |
| `data/FeedbackMonitors.json` | Feedback-monitor rules. |
| `data/Version.json` | Generated Data version. |
| `data/Layouts.json` | Runtime file retained on the Pi; the interface does not use it. |
| `index.html`, `Index.css`, `Dashboard.js` | Browser interface. |
| `StatusClient.js` | Device initialization and reception of live status data. |
| `Translations.json` | English and German interface text. |
| `DashboardServer.cpp` and other C++ files | Web server, data stores, assignments and monitoring. |
| `CopyToRasPi.sh` | Comparison, readback and transfer to the Pi. |
| `Install.sh`, `DashboardServer.service` | Installation and operation as a systemd service. |
