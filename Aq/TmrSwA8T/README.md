<!-- README.md, Version: 1.22 -->
# TmrSwA8T

## Übernahme der Geräte

Die Erstübernahme von NOUS A8T und Shelly Plug M Gen3 wird im eigenständigen Repository [ShellyNousTakeover](https://github.com/gneiss15/ShellyNousTakeover) beschrieben. Dieses Projekt gehört zu [AquariumControl](../../README.md) und stellt die Main-Anwendung nach der Übernahme bereit.

NOUS-A8T-Projekt mit Main, Rescue und gerätespezifischem LittleFS. Verbindliche
Hardware-, Partitions- und Buildregeln stehen in `PrjRegeln.md`.

## Hardware-Leistungsmessung

Firmware 1.12 ersetzt die GPIO-Interrupt-/Periodenmessung vollständig durch
PCNT und zwei LEDC-Tore. GPIO25: CF1 mit nominal 2 s Messung/0,5 s Pause,
abwechselnd Spannung und Strom. GPIO32: CF mit nominal 4 s Messung/1 s Pause.
Jede Messgröße wird einmal pro 5-s-Zyklus aktualisiert; erste Werte benötigen
entsprechend ein vollständiges Messfenster. Auslesen, SEL-Wechsel und Löschen
finden bei geschlossenem Tor statt. Keine Interrupts pro Sensorimpuls.

LEDC-Perioden bestehen aus Zweierpotenzen von Zählschritten. Der konfigurierte
Teiler/Tastgrad ergibt geringfügig abweichende Torzeiten; die Umrechnung
verwendet deren tatsächliche Dauer. Der englische Architekturkommentar in
`TmrSwA8TPowerMeter.cpp` beschreibt Registerwerte, Zeitablauf und Skalierung.
NVS-Kalibrierwerte bleiben unverändert. PCNT-Grenzwert-Interrupts erweitern
den Hardwarezähler: Gesamtzahl = Akkumulator + Restzähler. Auslesen und Löschen
verwenden beide Teile. Messfenster mit geändertem Relaiszustand werden verworfen; bei ausgeschaltetem Relais bleiben A/W null.
Die Diagnose-Edge-Zähler summieren jetzt akzeptierte Messfenster, nicht sämtliche
CF/CF1-Flanken einschließlich der Pausen.

GPIO25/32 wurden am 2026-10-13 auf LightTimerAq3 mit abgezogener Beleuchtung
getestet: Pull-up=1/Pull-down=0, Ausgang LOW=0/HIGH=1, jeweils ESP_OK.
Die temporäre `/GpioTest/`-Seite und GPIO-Test-API wurden wieder entfernt.
Die neue PCNT-/LEDC-Messung ist damit noch nicht am Gerät bestätigt.
Für den ersten Test nur Main auf LightTimerAq3 aktualisieren; LittleFS und
Rescue benötigen kein Update. Firmware-only-Build:
`TMR_SW_ENABLE_LITTLEFS=false ./EspCompile.sh`.

## Build-Einstiege

- `./EspCompile.sh`: ESP-IDF-Main mit Arduino-Komponente und LittleFS für die Geräte unter `DevState/`.
- `./EspRescueCompile.sh`: Rescue-Firmware.

`EspRescueCompile.sh` bindet das gemeinsame
`../Libs/Common/EspRescue/_EspRescueCompileEsp32Common.sh` mit `source` ein.
Dieses Hilfsskript besitzt keine Ausführungsrechte und ist kein direkter
Buildeinstieg; auch ein direkter Aufruf über `bash` wird abgefangen.

## Projekt- und Ausgabestruktur

- `Sw/`: projektlokale Firmwarequellen und Header einschließlich Config-Definition; `Version.inc.h` und `DeviceType.inc.h` werden hier generiert.
- `Sw/main/`: IDF-Main-Komponente; CMake-Projekt und SDK-/Partitionsvorgaben bleiben im Stamm.
- `EspBoardConfig.sh`: Board/FQBN, Bin-Unterordner und Partitionsgrößen für alle Builds.
- `DevState/<Gerät>/`: persistente Gerätekonfiguration und Buildquelle.
- `generated/build/main/`: Main-Cache, generierte Header und aufbereitete Webquellen.
  `esp32/idf/` enthält den IDF-Compiler-Cache, `MainDependencies/` die aufbereitete
  Arduino-Komponente und Bibliotheken, `IdfProject/` die generierte CMake-Projektkopie,
  SDK-Vorgaben, Komponentenverwaltung und Dependency-Lockdatei.
- `generated/build/rescue/`: Rescue-Cache und generierter Rescue-Sketch.
- `generated/bin/esp32_Arduino2/`: gemeinsame Main-, Rescue- und LittleFS-Artefakte.
- `generated/Instances/<Gerät>/LittleFS/`: vollständiges gerätespezifisches LittleFS.
- `generated/Instances/<Gerät>/LittleFS-readable/`: lesbare Build-Vergleichskopie.
- Unter derselben Instanz: `ftp/`, `ftp-readable/` und `ftp-devstate-readable/`.
  Vorhandene zusätzliche `ftp-current/`-Snapshots bleiben ebenfalls bei der Instanz.
- `generated/Logs/`: vorhandene Leistungsmessungs-CSV-Dateien.
- `generated/data/Versions.txt`: generierte Fallback-Versionsmetadaten.

Die LittleFS-Buildkopien enthalten leere Schaltzeiten/Intervalle, `Profiles: []`
und bei vorhandenem Sync-State den Startzeitstempel mit `Synced: false`. Die
Kanalzuordnungen bleiben erhalten; der Dashboard-Sync liefert die zentralen
Laufzeitdaten. Die Dateien unter `DevState/` werden dafür nicht verändert.

Main und Rescue verwenden inkrementelle Builds. `--clean` verschiebt
nur den eigenen Cache in den Papierkorb; `--full-clean` zusätzlich nur die eigenen
Artefakte im gemeinsamen Bin-Ordner. Andere Builds bleiben erhalten.

Vor einem LittleFS-Build zuerst die Gerätedaten herunterladen, im BCO links den
lokalen Stand und rechts den Geräte-Download vergleichen und Config-Abweichungen
klären. Beispiel aus dem A8T-Projektverzeichnis:

```bash
./tools/DownloadLittleFS.sh LightTimerAq3
./EspCompile.sh
```

Signierte Uploaddateien liegen unter `generated/bin/esp32_Arduino2/`:

- Main: `TmrSwA8T.ino.bin_signed`
- LittleFS: `<Gerät>.ino.littlefs_signed`
- Rescue: `TmrSwA8T.Rescue.rescue_signed`

Die Partitionstabelle bleibt als Prüfdatei unter
`generated/build/main/esp32/TmrSwA8T.ino.partitions.bin`; sie wird nicht in
den Uploadordner kopiert. Firmware-only-Builds erhalten die letzten
LittleFS-Abbilder unverändert. `--full-clean` entfernt sie ausdrücklich.

Erstellte Snapshots sind in `../../SnapshotZips.md` mit Pfad, Anlass und Umfang
dokumentiert.

## Historisches Archiv

`Old.zip` enthält die archivierten Entwicklungsstände. Ihre Beschreibung liegt
als separate Datei `Aq/TmrSwA8T/README-Old.md` im ZIP. Die aktuellen Main-/Rescue-
Builds verwenden das Archiv nicht. Das aktuelle NOUS-Takeover liegt im
separaten Projekt `../../ShellyNousTakeover/`.

## ESP-IDF-Main

Main verwendet ESP-IDF 5.5.1 und Arduino-ESP32 3.3.11 als aus Quellen gebaute
Komponente. Die bestehende setup/loop-Aufgabe läuft auf Core 1 mit 8192 Byte Stack.
Die gemeinsame Timer-/Heizungslogik und die Partitionsaufteilung bleiben erhalten.
Rescue verwendet weiterhin seinen vorhandenen Arduino-CLI-Build.
Der historische Bin-Unterordner `esp32_Arduino2` und die signierten Dateinamen
bleiben zur Kompatibilität mit den vorhandenen Uploadabläufen erhalten.

Die lokale IDF-Umgebung wird über `ESP_IDF_ENABLE_SCRIPT` geladen;
`ESP_IDF_COMPILER_BIN`, `ARDUINO_PORTABLE_DIR` und `ARDUINO_CORE_DIR` erlauben
abweichende Werkzeugpfade. Der normale Einstieg bleibt `./EspCompile.sh`.
Für einen reinen Firmwarebuild ohne LittleFS:

```bash
TMR_SW_ENABLE_LITTLEFS=false ./EspCompile.sh
```

Generische IDF-Flash-Ziele sind gesperrt: Sie würden Main an die Factory-Adresse
`0x10000` schreiben und damit Rescue überschreiben. Main gehört nach `app0` bei
`0xE0000` und wird über den bestehenden signierten Rescue-Upload installiert.
Der Build prüft ESP32-Image, Version, Main-Größe und die tatsächliche SDK-Tabelle
vor dem Signieren. Ein erfolgreicher Build ersetzt keinen Hardwaretest.

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

Firmware 1.20 verkürzt die temporären Transaktions-Dateinamen auf höchstens
31 Zeichen. Bestehende LittleFS-Abbilder bleiben verwendbar; kein FS-Upload
notwendig. Der vollständige Channels-/Schaltzeiten-Abgleich bleibt atomar.

Firmware 1.21 sendet Status und Requests als UDP-Version 2. Der Senderwechsel ist
auf allen aktiven Geräten bestätigt. Kein neues
LittleFS erforderlich; Quellen, Profile und Kalibrierung bleiben erhalten.

Firmware 1.24 ist signiert gebaut und entfernt die alte Version-1-Empfangslogik.
Relais, LED, Taster und Messwert-SEL verwenden die gemeinsame GnBasics-GPIO-
Schnittstelle mit direkten Registerzugriffen und nativer IDF-Pinkonfiguration.
Die Torflanken-ISR verwendet weiterhin ausdrücklich IRAM-fähige IDF-Aufrufe. Vor Aktivierung der Ausgänge werden die inaktiven Latches gesetzt;
der Relaisstatus wird erst nach erfolgreicher Initialisierung als bekannt markiert.
Die Messzeiten und NVS-Kalibrierwerte bleiben unverändert. Kein neues LittleFS
und kein Rescue-Upload erforderlich. Build und GPIO-Modelltests sind bestanden;
der Neustarttest auf dem Gerät steht noch aus.

Strom und Wirkleistung werden auch bei logisch ausgeschaltetem Relais aus den
realen Zählerwerten berechnet und angezeigt. Ein Relaiswechsel invalidiert
weiterhin die betroffenen Messfenster; vollständige neue Messwerte folgen im
bisherigen nominalen 5-s-Zyklus. Damit bleibt unerwarteter Verbrauch bei AUS
sichtbar, ohne ihn aus dem Software-Schaltzustand künstlich auf null zu setzen.
Die Anzeige allein ist keine automatische Kontaktfehlerauswertung.
