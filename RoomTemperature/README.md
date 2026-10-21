<!-- README.md, Version: 1.01 -->
# Raumtemperatur-Sender für den Raspberry Pi

Eigenständiger DS18B20-Sender mit Python 3 ohne Zusatzpakete. Das Programm liest
über den Linux-OneWire-Treiber und sendet die Raumtemperatur alle fünf Sekunden
per UDP-Broadcast. Es läuft unabhängig von MultiTimer und Dashboard als eigener
Systemdienst. Produktversion: Versions.txt.

## Hardware und Vorbereitung

Am vorhandenen Raspberry Pi 3 Model B wurde GPIO4 lesend geprüft: unbelegt,
Eingang, kein Consumer/OneWire-Overlay. Das frühere OLED ist entfernt. Sensor
außerhalb des Gehäuses platzieren, mit Abstand zu Pi/Netzteil und Warmluft.

Bei ausgeschaltetem Pi anschließen, Sensor-Pinbelegung anhand seines Datenblatts
prüfen (Kabelfarben nicht als Belegung voraussetzen):

- VDD an 3,3 V, beispielsweise physischer Header-Pin 1.
- GND an Masse, beispielsweise physischer Pin 6.
- DQ an BCM GPIO4, physischer Pin 7.
- 4,7-kΩ-Pull-up zwischen DQ und 3,3 V; kein Parasite-Power-Betrieb.

Auf dem Pi in /boot/firmware/config.txt im passenden [all]-Abschnitt ergänzen,
sofern kein entsprechender Eintrag existiert:

```ini
dtoverlay=w1-gpio,gpiopin=4
```

Danach ist ein Pi-Neustart nötig. Weder Sender noch Installer ändern diese Datei
oder starten den Pi neu. Während des Reboots stehen dessen Schaltkanäle nicht
zur Verfügung. Die später geplante autonome PI-Regelung ist noch nicht umgesetzt.

## Übertragen und vor dem Dienststart prüfen

Am PC:

```bash
cd /mnt/SD_256GB/Prjs/Aq/RoomTemperature
./CopyToRasPi.sh
```

Am Pi, nach Anschluss und Treiberaktivierung:

```bash
cd ~/Prjs/RoomTemperature
python3 RoomTemperature.py --list
python3 RoomTemperature.py --once
```

--once gibt ein Telegramm aus, sendet aber kein UDP. Exitcode 0 bedeutet gültiger
Wert; 2 bedeutet ungültiger Sensorwert. Ohne Sensor sind null/Healthy=false korrekt.
Die gefundene 28-…-Adresse in data/Config.json als SensorId eintragen. null erlaubt
vorläufig automatische Auswahl bei genau einem gefundenen DS18B20. Mehrere
Sensoren werden ohne explizite Auswahl abgelehnt. Der Bereich -20..60 °C ist für
Raumtemperatur gewählt und verwirft insbesondere den 85-°C-Einschaltwert.

Nach plausibler Einzelmessung:

```bash
./Install.sh
journalctl -u RoomTemperature.service -n 30 --no-pager
```

Install.sh verwendet sudo, installiert/aktiviert nur RoomTemperature.service und
führt ihn als Projektbesitzer (auf dem Pi normalerweise pi) aus, nicht als root.
ROOM_TEMPERATURE_USER überschreibt den Dienstbenutzer. Bei späterem Quellupdate
CopyToRasPi.sh erneut ausführen und sudo systemctl restart RoomTemperature.service.
Der Transfer überschreibt die vorhandene Pi-Config nicht. Kein Compiler nötig.

## UDP und Systemintegration

```json
{"Version":2,"Type":"Status","ID":"B8:27:EB:7B:96:D7@3","Status":{"Kind":"RoomTemperature","Temperature":22.375}}
```

Das tatsächliche Paket enthält zusätzlich SensorId, AppVersion, CurrentTime,
Healthy, Errors und Measurements für die Anzeige. Fehler werden als
Temperature:null/Valid:false publiziert, nicht als letzter gültiger Wert.
Version-2-Requests mit ID:[] oder der eigenen MAC@3 lösen eine frische Messung
aus (höchstens einmal pro Sekunde). Zielport ist 44444. BroadcastAddress kann
bei Bedarf auf die passende Subnetz-Broadcastadresse gesetzt werden.

Die MAC ist dieselbe feste Pi-MAC wie bisher; @3 unterscheidet den Sensor vom
MultiTimer @1 und Dashboard @2. Eine neue Registry-Revision mit ExternalSources
RoomTemperature → B8:27:EB:7B:96:D7@3 muss nach dem Sensortest noch gezielt
angelegt/verteilt werden. Diese Vorbereitung ändert keine laufende Zuordnung oder
Heizprofile. Die Raumtemperatur ist bislang keine Eingangsgröße der vorhandenen
Heizungsregelung; die autonome PI-Regelung im Nous/Shelly ist ein nächster Schritt.

## Prüfumfang

Sieben Tests bestanden: sechs Hosttests für Messwert-/CRC-Verarbeitung,
Verlust/Wiederkehr, Auswahl, Bereichsfehler, Protokoll und Requests; ein echter
Senderprozess mit simuliertem sysfs über ausschließlich Loopback-UDP-Broadcast
für gültigen Wert, CRC-Fehler/null und Wiederkehr auf Request. Die tatsächlich
vom Installer erzeugte Dienstdatei besteht systemd-analyze verify; Shellsyntax
geprüft. Dies ersetzt keinen Test mit angeschlossenem DS18B20. Installer und
Quelltransfer sind nicht produktiv ausgeführt.

Technische Quellen:
[Linux w1_therm](https://docs.kernel.org/w1/slaves/w1_therm.html),
[Raspberry-Pi-Overlays](https://github.com/raspberrypi/firmware/blob/master/boot/overlays/README).
