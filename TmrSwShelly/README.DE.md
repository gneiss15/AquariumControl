<!-- README.DE.md, Version: 1.52 -->
# TmrSwShelly

[English](README.md)

## Übernahme der Geräte

Die Erstübernahme von Nous A8T und Shelly Plug M Gen3 wird im eigenständigen Repository [ShellyNousTakeover](https://github.com/gneiss15/ShellyNousTakeover) beschrieben. Dieses Projekt gehört zu [AquariumControl](../README.DE.md) und stellt die Main-Anwendung nach der Übernahme bereit.

Produktionsprojekt für Shelly Plug M Gen3: gemeinsame TmrSw-Main mit Timer-/
Heizungsrollen, gerätespezifischem LittleFS und unabhängigem Produktions-Rescue.
Produktversionen stehen in `Versions.txt`. Eigene Gerätekonfigurationen und
Signierdaten werden wie unter [Build und private Konfiguration](../BUILD-SECRETS.md)
beschrieben bereitgestellt.

[Hardwareunterlagen](Hw/README.DE.md)

## Aktive Quellen

- `Sw/`, `Sw/main/`: Main, Hardwareabstraktion und BL0942-Leistungsmessung.
- `partitions.csv`: aktive Produktionsaufteilung.
- `Sw/Rescue/`: eigenständiger nativer Produktions-Rescue samt benötigter Laufzeit,
  Geometrie-/Aktivierungsprüfungen, Kompatibilitätsschicht und lokalen Verifikationstests.
- `DevState/<Gerät>/`: persistente gerätespezifische Buildquelle.

## Build und Ausgaben

Aus dem Projektverzeichnis:

```bash
./EspCompile.sh
./EspRescueCompile.sh
```

Main verwendet Arduino-ESP32 3.3.11 als Quellkomponente mit ESP-IDF 5.5.1.
Rescue verwendet einen nativen IDF-Build. Die Skripte erzeugen/signieren Artefakte;
sie führen keinen Upload oder Flash-Aufruf aus. Main wird einmal je Projekt
gebaut, LittleFS ohne Auswahl für alle DevState-Geräte.
`--Device Shelly1` wählt beispielsweise eine einzelne Instanz.

Vor jedem LittleFS-Build die aktuellen Gerätedateien zurücklesen und Config-
Abweichungen klären (lokaler Stand links, Download rechts):

```bash
./tools/DownloadLittleFS.sh Shelly1
./tools/DownloadLittleFS.sh Shelly2
./EspCompile.sh
```

- Normaler Aufruf: inkrementell.
- `--build`: eigenen Cache löschen und vollständig neu bauen.
- `--clean`: nur eigenen Cache löschen und beenden.
- `--full-clean`: zusätzlich nur eigene veröffentlichte Artefakte entfernen.
- Main unterstützt `--status` mit Phasen und Compiler-Heartbeat.

`EspBoardConfig.sh` bündelt Board, Partitionsgrößen und Bin-Unterordner.
Main-Cache, Arduino-Abhängigkeiten und IDF-Metadaten liegen unter
`generated/build/main/`; Rescue-Eingaben und Cache unter `generated/build/rescue/`.
Die gemeinsame Binärausgabe liegt unter `generated/bin/esp32c3_Arduino2/`:

- `TmrSwShelly.ino.bin_signed`: Main.
- `<Gerät>.ino.littlefs_signed`: LittleFS.
- `TmrSwShelly.Rescue.rescue_signed`: Rescue.

Geräte-LittleFS, lesbare Kopien und FTP-Readbacks liegen unter
`generated/Instances/<Gerät>/`. Aktuelle Prüfergebnisse stehen dort in
`main-build-verification.json`; Rescue-Prüfergebnisse liegen in seinem Buildordner.
Generierte Geräte-Readbacks können private Daten enthalten und sind nicht im
öffentlichen Repository enthalten.

## Layout und Hardware

SDK-Partitionstabellenoffset ist `0x10000`; Main liegt bei `0x3A0000/0x2A0000`,
LittleFS bei `0x2C0000/0xE0000`, Rescue bei `0x640000/0x180000`.
DeviceType ist `TmrSwShellyC3V1`. Generische Main-IDF-Flash-Ziele sind gesperrt,
weil ihre Factory-Adresse Rescue treffen würde. Signierte Updates verwenden
den Rescue-Webpfad.

Main prüft Chip/Revision, Flash-/Sicherheitszustand und tatsächliche SDK-Geometrie
vor Netzwerk-/NVS-/Ausgangsinitialisierung. Relais GPIO0 active HIGH startet AUS;
Taster GPIO7 active LOW verwendet INPUT_PULLUP. RGB ist active LOW: Rot GPIO18,
Grün GPIO10, Blau GPIO19. WLAN-/NTP-Warten blinkt blau, Normalbetrieb zeigt den
Relaiszustand grün. Rescue signalisiert SOS rot.

BL0942 verwendet UART1, RX5/TX4, 9600 Baud. Kalibrierwerte stammen ausschließlich
aus dem tatsächlichen Geräte-Factory-Datensatz. Eine Kalibrierung überschreibt nach
Warnung dessen validierte Felder; Restdaten, Schutzstatus und Rücklesen werden geprüft.
Stromausfall während eines Sektorwrites bleibt ein Risiko.

## Geräteidentität und Funktionstausch

Jedes physische Gerät wird anhand seiner MAC-Adresse und Dienstkennung erkannt.
Im Dashboard lassen sich Funktionen zwischen funktionskompatiblen Geräten bzw.
einzelnen Schaltausgängen tauschen. Hardwarekonfiguration und Kalibrierung bleiben
beim physischen Gerät. Die Bedienung beschreibt
[Dashboard – Geräteidentität und Funktionstausch](../Dashboard/README.DE.md#geräteidentität-und-funktionstausch).
Technische Einzelheiten stehen in [DeviceIdentity.md](../Libs/GnBasicLib/DeviceIdentity.md).

## Gerätezuordnung und PC-Dateien

Ein Funktionstausch allein erfordert keinen Dateisystem-Upload. Soll später ein
Dateisystem neu aufgespielt werden, müssen vor dem Erstellen des Abbilds die
PC-Daten mit der aktuellen Gerätezuordnung abgeglichen werden. Den folgenden
Befehl im Stammverzeichnis des AquariumControl-Repositories ausführen:

```bash
python3 tools/ReadDeviceStates.py --apply
```

Dashboard selbst hat keinen Zugriff auf die PC-Dateien. Nach dem Abgleich die
betroffenen gerätespezifischen Dateisystem-Abbilder neu bauen. Abbilder sind nicht
zwischen verschiedenen Hardwaretypen austauschbar.

JSON-Editoren zeigen Objektarrays für Profile und Kanäle mit einer ausgerichteten
Zeile pro Datensatz; Zusatzfelder bleiben erhalten. `Channels.json` nutzt die
verfügbare Seitenbreite. Der `Sources.json`-Editor wird über seinen Button geöffnet
und ist nur bei nicht erreichbarem Dashboard freigegeben. Der Server prüft dies
beim Speichern erneut.

Signierte Dateisystem-Abbilder tragen den Hostnamen aus der Konfiguration:
Funktionsname bei konkreter Zuordnung, Hardware-Name bei Reservegeräten.
`DevState/` und `generated/Instances/` verwenden die internen Funktionsschlüssel.

## Kanaltausch aus dem Dashboard

Der einzelne Schaltausgang kann seine Funktion mit einem funktionskompatiblen
Nous-/Shelly-Ausgang oder MultiTimer-Kanal tauschen. Kanalname, Rolle, Profil und
Zeitplan folgen der Funktion; GPIO, Kalibrierung, Hostname und Zugangsdaten
bleiben beim physischen Gerät.

Vor dem Tausch Verbraucher trennen und vor dem Wiederanschluss die bestätigte
Übernahme abwarten. Kanalzuordnung, Schaltzeiten und Intervalle werden gemeinsam
geprüft und gespeichert. Unterbrochene Änderungen werden beim Neustart vor der
Hardwareinitialisierung abgeschlossen. Erst danach wird die neue Zuordnung
bestätigt.

## Messwerte und Status

Statusmeldungen und Messwertanforderungen verwenden UDP-Version 2. Messwerte
stehen als benannte numerische Felder im Objekt `Status`. Profile wählen mit
`Measurement` das auszuwertende Feld; ein leeres Feld bedeutet `Temperature`.

Relais, Taster und die GPIO-Vorbereitung der RGB-LED verwenden die gemeinsame
GnBasics-Schnittstelle. Vor dem Aktivieren des Relais wird der Ausgangspegel für
AUS gesetzt; der Relaisstatus gilt erst nach erfolgreicher Initialisierung als
bekannt. Die LED-Helligkeit wird über LEDC gesteuert.
