<!-- README.md, Version: 1.02 -->
# Raspberry Pi room-temperature sender

[Deutsch](README.DE.md)

Standalone DS18B20 sender using Python 3 without extra packages. It reads through the Linux OneWire driver and broadcasts room temperature over UDP every five seconds. It runs independently of MultiTimer and Dashboard as its own system service. Product version: `Versions.txt`.

## Hardware and preparation

Before connecting, ensure GPIO4 is free and no other driver or consumer uses it. Place the sensor outside the enclosure, away from the Pi, power supply and warm airflow.

With the Pi powered off, connect as follows. Check the sensor pinout against its datasheet; do not assume wire colors identify pins:

- VDD to 3.3 V, for example physical header pin 1.
- GND to ground, for example physical pin 6.
- DQ to BCM GPIO4, physical pin 7.
- A 4.7 kΩ pull-up between DQ and 3.3 V; no parasite-power operation.

On the Pi, add this in the appropriate `[all]` section of `/boot/firmware/config.txt` unless an equivalent entry already exists:

```ini
dtoverlay=w1-gpio,gpiopin=4
```

A Pi reboot is required afterwards. Neither sender nor installer edits this file or reboots the Pi. Its switching channels are unavailable during reboot. Autonomous PI control is not implemented.

## Transfer and check before starting the service

On the PC, from the repository root:

```bash
cd RoomTemperature
./CopyToRasPi.sh
```

On the Pi, after connection and driver activation:

```bash
cd ~/Prjs/RoomTemperature
python3 RoomTemperature.py --list
python3 RoomTemperature.py --once
```

`--once` prints a telegram without sending UDP. Exit code 0 means a valid measurement; 2 means an invalid sensor value. Without a sensor, null/Healthy=false is expected. Set the discovered `28-…` address as `SensorId` in `data/Config.json`. `null` permits automatic selection when exactly one DS18B20 is found. Multiple sensors without explicit selection are rejected. The room-temperature range −20…60 °C rejects the 85 °C power-on value in particular.

After a plausible single measurement:

```bash
./Install.sh
journalctl -u RoomTemperature.service -n 30 --no-pager
```

`Install.sh` uses sudo and installs/enables only `RoomTemperature.service`. It runs as the project owner (normally pi), not root. `ROOM_TEMPERATURE_USER` overrides the service user. For later source updates, run `CopyToRasPi.sh` again and then `sudo systemctl restart RoomTemperature.service`. Transfer preserves existing Pi configuration. No compiler is needed.

## UDP and system integration

```json
{"Version":2,"Type":"Status","ID":"B8:27:EB:7B:96:D7@3","Status":{"Kind":"RoomTemperature","Temperature":22.375}}
```

The actual packet also contains SensorId, AppVersion, CurrentTime, Healthy, Errors and Measurements for display. Errors publish Temperature:null/Valid:false, not the last valid value. Version-2 requests with ID:[] or the sender's MAC@3 trigger a fresh measurement, at most once per second. Target port is 44444. Set BroadcastAddress to the appropriate subnet broadcast address if needed.

Identity uses the fixed Pi MAC; @3 distinguishes the sensor from MultiTimer @1 and Dashboard @2. After a plausible sensor measurement, register it under `ExternalSources` in the Dashboard registry, for example RoomTemperature → B8:27:EB:7B:96:D7@3. The assignment change requires a new registry revision and distribution. Room temperature is not an input to the existing heating control; autonomous PI control in Nous/Shelly is not implemented.

## Check on your own device

After connecting the sensor and enabling the driver, run `--list` and `--once` and check that the measurement is plausible. Then start the service and inspect its messages. Failed or missing readings must appear as invalid; an old value must not be sent as a current measurement.

Technical references:
[Linux w1_therm](https://docs.kernel.org/w1/slaves/w1_therm.html),
[Raspberry Pi overlays](https://github.com/raspberrypi/firmware/blob/master/boot/overlays/README).
