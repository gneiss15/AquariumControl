<!-- README.md, Version: 1.47 -->
# TmrSwShelly

## Übernahme der Geräte

Die Erstübernahme von NOUS A8T und Shelly Plug M Gen3 wird im eigenständigen Repository [ShellyNousTakeover](https://github.com/gneiss15/ShellyNousTakeover) beschrieben. Dieses Projekt gehört zu [AquariumControl](../../README.md) und stellt die Main-Anwendung nach der Übernahme bereit.

Produktionsprojekt für Shelly Plug M Gen3: gemeinsame TmrSw-Main mit Timer-/
Heizungsrollen, gerätespezifischem LittleFS und unabhängigem Produktions-Rescue.
Produktversionen stehen in `Versions.txt`, verbindliche Regeln in `PrjRegeln.md`.

## Aktive Quellen

- `Sw/`, `Sw/main/`: Main, Hardwareabstraktion und BL0942-Leistungsmessung.
- `partitions.csv`: unveränderte Produktionsaufteilung und einzige aktive CSV.
- `Sw/Rescue/`: eigenständiger nativer Produktions-Rescue samt benötigter Laufzeit,
  Geometrie-/Aktivierungsprüfungen, Kompatibilitätsschicht und lokalen Verifikationstests.
- `hw_intern/PogoAdapter/`: bleibt ausschließlich intern für den späteren Pogo-Adapter-Versuch erhalten; nicht veröffentlichen.
- `DevState/<Gerät>/`: persistente gerätespezifische Buildquelle.
- `Old.zip`: historische Migration, alte Projektdokumentation und früherer
  OneWayMigration-Einstieg. Separate Beschreibung: `Aq/TmrSwShelly/README-Old.md`
  im ZIP. Kein aktueller Build hängt davon ab.

Das eigenständige aktuelle Shelly-/NOUS-Takeover liegt in `../../ShellyNousTakeover/`.
Die bereits abgeschlossene Migration ist keine Voraussetzung für einen erneuten
Main-/Rescue-Build. Noch benötigte Schutzfunktionen wurden übernommen; es wurde
kein Stock-Rollback wieder freigegeben.

## Build und Ausgaben

Aus dem Projektverzeichnis:

```bash
./EspCompile.sh
./EspRescueCompile.sh
```

Main verwendet Arduino-ESP32 3.3.11 als Quellkomponente mit ESP-IDF 5.5.1.
Rescue bleibt ein nativer IDF-Build. Die Skripte erzeugen/signieren Artefakte;
sie führen keinen Upload oder Flash-Aufruf aus. Main wird einmal je Projekt
gebaut, LittleFS ohne Auswahl für alle DevState-Geräte (Shelly1 und Shelly2).
`--Device Shelly1` bzw. der frühere Positionsparameter wählen eine Instanz.

Vor jedem LittleFS-Build die aktuellen Gerätedateien zurücklesen und Config-
Abweichungen klären (lokaler Stand links, Download rechts):

```bash
./tools/DownloadLittleFS.sh Shelly1
./tools/DownloadLittleFS.sh Shelly2
./EspCompile.sh
```

- Normaler Aufruf: inkrementell.
- `--build`: eigenen Cache über Trash entfernen und vollständig neu bauen.
- `--clean`: nur eigenen Cache über Trash entfernen und beenden.
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
Historische Geräte-Captures/Ergebnisse bleiben unter `generated/History/ShellyMigration/`
und bisherige generierte Migrationsausgaben unter `generated/` erhalten.
Sie können private Gerätedaten enthalten und sind nicht im öffentlichen Quellarchiv.

## Layout und Hardware

SDK-Partitionstabellenoffset ist `0x10000`; Main liegt bei `0x3A0000/0x2A0000`,
LittleFS bei `0x2C0000/0xE0000`, Rescue bei `0x640000/0x180000`.
DeviceType ist `TmrSwShellyC3V1`. Generische Main-IDF-Flash-Ziele sind gesperrt,
weil ihre Factory-Adresse Rescue treffen würde. Signierte Updates verwenden
weiterhin den vorhandenen Rescue-Webpfad.

Main prüft Chip/Revision, Flash-/Sicherheitszustand und tatsächliche SDK-Geometrie
vor Netzwerk-/NVS-/Ausgangsinitialisierung. Relais GPIO0 active HIGH startet AUS;
Taster GPIO7 active LOW verwendet INPUT_PULLUP. RGB ist active LOW: Rot GPIO18,
Grün GPIO10, Blau GPIO19. WLAN-/NTP-Warten blinkt blau, Normalbetrieb zeigt den
Relaiszustand grün. Rescue signalisiert SOS rot.

BL0942 verwendet UART1, RX5/TX4, 9600 Baud. Kalibrierwerte stammen ausschließlich
aus dem tatsächlichen Geräte-Factory-Datensatz. Eine Kalibrierung überschreibt nach
Warnung dessen validierte Felder; Restdaten, Schutzstatus und Rücklesen werden geprüft.
Stromausfall während eines Sektorwrites bleibt ein Risiko. Die Archivierung ändert
weder die Mess-/Schaltlogik noch diese Kalibrierungsregeln.

Historische Inbetriebnahme-/Migrationsnachweise stehen im Archiv. Aktuelle lokale
Build-/Mock-Ergebnisse ersetzen keinen neuen Hardwaretest und keinen Geräte-Upload.

## Geräteidentität und Funktionstausch

Die aktuelle Firmware unterstützt persistierte MAC/Dienst-IDs und Funktionstausch
über das Dashboard. Ein Firmware-Upload alleine aktiviert keinen Tausch.
Zuerst Dashboard und alle Teilnehmer aktualisieren, dann im Bereich
Gerätezuordnung die MAC-Zuordnung ausdrücklich aktivieren. Für ESP genügt Firmware;
kein LittleFS-/Rescue-Upload erforderlich. Hardwarekonfiguration und Kalibrierung
bleiben beim physischen Gerät. Reihenfolge, Voraussetzungen und Prüfungen stehen in
[DeviceIdentity.md](../Libs/GnBasicLib/DeviceIdentity.md).

## Gerätezuordnung und PC-Dateien

Die Netzwerkidentität ist MAC@ServiceId; Funktionsnamen bleiben für Menschen und Zeitpläne erhalten. Bedienung und aktueller Versionsstand: [DeviceIdentity.md](../Libs/GnBasicLib/DeviceIdentity.md).
Nach einem Funktionstausch sind frühere FS-Binaries veraltet. Dashboard kann PC-Dateien nicht aktualisieren. Vom Prjs-Stamm `python3 Aq/tools/ReadDeviceStates.py --apply` ausdrücklich starten: vollständiger ESP-Readback, Übernahme persistenter Daten in DevState/<aktuelle Funktion> im passenden Hardwareprojekt. Danach betroffene FS-Abbilder neu bauen; keine Abbilder zwischen Hardwaretypen austauschen.

JSON-Editoren: Objektarrays (Profile/Kanäle) werden mit einer ausgerichteten Zeile pro Datensatz dargestellt; Zusatzfelder bleiben erhalten. Channels.json nutzt die verfügbare Seitenbreite. Aktueller Webstand: LittleFS 1.12; Firmware unverändert.

Signierte FS-Uploaddateien tragen den Hostnamen aus der Config: konkrete Funktion → Funktionsname, Reservegerät → Hardware-Name. DevState- und generated/Instances-Verzeichnisse behalten interne Funktionsschlüssel. Beispielsweise DevState/Shelly1 mit Hostname Nous1 erzeugt Nous1.ino.littlefs_signed. Erfolgreiche FS-Builds bereinigen nicht mehr aktuelle Hostnamen-Artefakte via Trash.

Channels.json besitzt wie Sources.json eine umrahmte Box mit innerem Textfeld. Sources.json ist initial zugeklappt und wird über den Button Sources.json … angezeigt bzw. wieder verborgen. Die Dashboard-Erreichbarkeitssperre bleibt serverseitig und im Editor bestehen.

## HW-Endpunkt-Tausch

Mit Dashboard 1.23 wird der einzelne Schaltausgang als MAC@ServiceId/0 adressiert (Fähigkeit 1). Er kann mit einem NS-Ausgang oder einem MT-Kanal identischer Fähigkeit tauschen. Channels-Id, Rolle/Profil und Plan folgen der Funktion; GPIO, Kalibrierung, Hostname und Zugangsdaten bleiben. Vor dem Tausch Verbraucher trennen und vor Wiederanschluss bestätigte Übernahme abwarten.

Vollständige zentrale Channels-/Zeit-/Intervall-Bundles werden gemeinsam geprüft, über TimeConfig.transaction.json gespeichert und nach Abbruch vor Hardwareinitialisierung wiederhergestellt. Der neue Kanaltausch benötigt aktuelle Firmware mit EndpointAssignments=1; ein FS-Upload ist allein für diesen Firmwarestand nicht nötig. Nach tatsächlichem Tausch vor einem späteren FS-Upload vom PC ReadDeviceStates.py --apply ausführen und das gerätespezifische FS neu bauen.

Der gemeinsame Transaktionshelfer verwendet ab Firmware 1.16 die kurze Endung
`.new`, damit seine Dateinamen auch zu bestehenden Nous-Dateisystemen passen.
Shelly 1.15 unterstützt die neuen Profilfelder bereits; dieses Wartungsupdate
benötigt kein neues LittleFS.

Firmware 1.17 sendet Status und Requests als UDP-Version 2. Der Senderwechsel ist
auf allen aktiven Geräten bestätigt. Kein neues
LittleFS erforderlich; Quellen, Profile und Kalibrierung bleiben erhalten.

Firmware 1.20 ist signiert gebaut und entfernt die alte Version-1-Empfangslogik.
Relais, Taster und die GPIO-Vorbereitung der RGB-LED verwenden die gemeinsame
GnBasics-Schnittstelle mit Registerzugriffen und nativer IDF-Pinkonfiguration.
Das Relais startet mit vorgelegtem AUS-Latch; der Status gilt erst nach
geprüfter Initialisierung als bekannt. Die bestehende LEDC-Helligkeitssteuerung
bleibt erhalten. Kein neues LittleFS und kein Rescue-Upload erforderlich.
Build und GPIO-Modelltests sind bestanden; der Hardwaretest steht noch aus.
