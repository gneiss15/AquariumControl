<!-- README.md, Version: 1.01 -->
# LaptopBattery

[Deutsch](README.DE.md)

Linux publisher for battery percentage, AC power and charging state. It broadcasts UDP status every ten seconds on port 44444. Dashboard displays the measurements; a switching output assigned to the charging profile can control the laptop power supply.

## Install on the laptop

Requires Python 3.10 or later, Bash and systemd; no third-party Python packages.

1. Copy this project directory to the laptop.
2. Check the paths in `BatteryUdpSender.py` on the laptop:

   | Input | Default |
   | --- | --- |
   | Battery percentage | `/sys/class/power_supply/BAT1/capacity` |
   | AC power | `/sys/class/power_supply/ACAD/online` |
   | Battery state | `/sys/class/power_supply/BAT1/status` |
   | Wi-Fi MAC | `/sys/class/net/wlp3s0/address` |

   Broadcast address: `192.168.99.255`. Adapt these settings before installation if hardware or network differs.

3. Run from this directory as the normal laptop user, **without prefixing sudo**:

   ```bash
   ./Install.sh
   ```

The installer checks hardware paths, uses sudo for system installation and starts `BatteryUdpSender.service` as the invoking user. The service name and sender path stay the same. `Install.sh` directly replaces existing program files and the unit; no prior uninstall is needed. Existing learning state is retained. Use the same command for initial installation and updates.

Installed files are `/usr/local/bin/BatteryUdpSender.py`, `ChargeLearning.py`, `ChargeLearning.sh`, the unit under `/etc/systemd/system/`, and `Versions.txt` under `/usr/local/share/LaptopBattery/`.

```bash
systemctl status BatteryUdpSender.service
journalctl -u BatteryUdpSender.service -n 30 --no-pager
```

Receivers require boot-counter support: Nous firmware 1.29 or later, Shelly 1.25 or later, MultiTimer 2.67 or later.

## Learning switch

Use the same user account as the service, **without sudo**:

```bash
ChargeLearning.sh status
ChargeLearning.sh off
ChargeLearning.sh on
```

`aus` and `ein` are aliases; `toggle` reverses the current setting. From the source directory, use `./ChargeLearning.sh ...`.

Learning initially defaults to ON. OFF persists across reboots. Switching also works away from home without networking; the next UDP packet reports the setting within about ten seconds while the service runs. OFF discards an active learning measurement or one awaiting a returning battery report. Normal run-on continues using the existing factor.

After manually disabling learning, enable it again only on the home network: reconnect with learning still OFF and wait at least one sending interval so the controller discards the old cycle, then switch ON. The learning switch itself does not increment a counter.

## Boot counter and learning

The sender persists the last Linux boot identifier and a `uint32` boot counter. Each new system boot increments the counter exactly once. Restarting the sender service, AC transitions and operating the learning switch do not increment it. Suspend and hibernation are not new system boots. The service must also start automatically away from home to observe each boot.

The controller records the counter from the last battery report before run-on and checks it when reports return:

- Difference exactly **1**: the expected next boot; the measurement may be evaluated with learning permission enabled.
- Difference **0**, **2 or more**: an unsuitable boot sequence; discard the learning measurement.

Differences use modulo `2^32` arithmetic. `4294967295 → 0` gives 1 and is accepted; `4294967294 → 0` gives 2 and discards the measurement. Zero is a valid counter value.

**Limitations:** External charging while the laptop remains powered OFF cannot be detected. In that case, boot it at least once while away and shut it down again before the normal boot at home, or leave learning manually OFF. A single boot away followed by returning while still running is also indistinguishable from a normal returning boot; use the learning switch OFF for that case. The counter detects additional boots, not location or every charging event.

## Persistence and UDP

Permission, boot counter and last boot identifier are stored in `~/.local/state/LaptopBattery/ChargeLearning.json`. Sender and control script share this file. Writes use a file lock and atomic replacement; regular transmissions do not write. The boot identifier stays local. Preserve the state file during updates and reinstallation. If it is corrupt or unreadable, battery reporting continues with learning disabled and a journal error; the damaged file is not replaced automatically.

```json
{"Version":2,"Type":"Status","ID":"<WLAN-MAC>@3","Status":{"Kind":"LaptopPower","BatteryPercent":63,"AcOnline":true,"BatteryState":"Charging","ChargeLearningAllowed":true,"ChargeBootCounter":17}}
```

The MAC is read at runtime and converted to uppercase. Battery percentage and AC power become `null` on read errors; battery state is `Charging`, `Discharging`, `Idle` or `Unknown`. Compatible receivers do not learn without valid permission and a boot counter.

## Tests

```bash
python3 tests/test_sender.py
python3 tests/test_install.py
```

Tests use temporary state, simulated sensor files and actual UDP exclusively over loopback. They cover boot changes, service restarts, wraparound, persistent OFF/ON, concurrent commands, corrupt state and transmitted fields. Testing on the actual laptop is still required.
