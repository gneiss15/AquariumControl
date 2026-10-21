<!-- README.DE.md, Version: 1.27 -->
# TmrSwA8T

[English](README.md)

## Übernahme der Geräte

Die Erstübernahme von Nous A8T und Shelly Plug M Gen3 wird im eigenständigen Repository [ShellyNousTakeover](https://github.com/gneiss15/ShellyNousTakeover) beschrieben. Dieses Projekt gehört zu [AquariumControl](../README.DE.md) und stellt die Main-Anwendung nach der Übernahme bereit.

Nous-A8T-Projekt mit Main, Rescue und gerätespezifischem LittleFS.
Produktversionen stehen in `Versions.txt`. Eigene Gerätekonfigurationen und
Signierdaten werden wie unter [Build und private Konfiguration](../BUILD-SECRETS.md)
beschrieben bereitgestellt.

[Hardwareunterlagen](Hw/README.DE.md)

## Hardware-Leistungsmessung

Die Leistungsmessung verwendet Hardware-Impulszähler (PCNT) und zwei zeitgesteuerte
Messtore (LEDC). GPIO25 erfasst CF1 mit nominal 2 s Messung und 0,5 s Pause,
abwechselnd für Spannung und Strom. GPIO32 erfasst CF mit nominal 4 s Messung
und 1 s Pause. Jede Messgröße wird einmal pro nominalem 5-s-Zyklus aktualisiert;
erste Werte benötigen ein vollständiges Messfenster.

Auslesen, Messgrößenwechsel und Löschen des Zählers erfolgen bei geschlossenem
Messtor. Die Umrechnung berücksichtigt die tatsächliche Torzeit. Ein Akkumulator
erweitert den Hardwarezähler; beim Auslesen werden Akkumulator und Restzähler
zusammen berücksichtigt. Einzelheiten stehen im Architekturkommentar von
`Sw/TmrSwA8TPowerMeter.cpp`.

Strom und Wirkleistung werden auch bei logisch ausgeschaltetem Relais aus den
realen Zählerwerten berechnet und angezeigt. Ein Relaiswechsel verwirft die
betroffenen Messfenster; neue Werte stehen nach vollständigen Messfenstern zur
Verfügung. So bleibt unerwarteter Verbrauch bei AUS sichtbar. Die Anzeige allein
ist keine automatische Kontaktfehlerauswertung.

Kalibrierwerte liegen gerätespezifisch im NVS. Diagnosezähler erfassen die Impulse
der akzeptierten Messfenster.

## Kalibrierung der Leistungsmessung

Ein Nous A8T kann mithilfe eines Shelly als Messreferenz abgeglichen werden. [Anleitung](../tools/CalibrateA8T.DE.md) · [Skript](../tools/CalibrateA8T.py). Die Messung verwendet zwei vertauschte Gerätereihenfolgen. Das Skript schaltet beide Relais temporär ein; Kalibrierwerte werden ausschließlich auf den A8T und nur mit `--apply` sowie zusätzlicher Bestätigung geschrieben. Die Shelly-Kalibrierung bleibt unverändert.

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

Main und Rescue verwenden inkrementelle Builds. `--clean` löscht nur den
eigenen Cache; `--full-clean` zusätzlich die eigenen Artefakte im gemeinsamen
Bin-Ordner. Andere Builds bleiben erhalten.

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

## ESP-IDF-Main

Main verwendet ESP-IDF 5.5.1 und Arduino-ESP32 3.3.11 als aus Quellen gebaute
Komponente. Die setup/loop-Aufgabe läuft auf Core 1 mit 8192 Byte Stack.
Rescue verwendet einen Arduino-CLI-Build.
Die Binärdateien liegen unter `generated/bin/esp32_Arduino2/`.

Die lokale IDF-Umgebung wird über `ESP_IDF_ENABLE_SCRIPT` geladen;
`ESP_IDF_COMPILER_BIN`, `ARDUINO_PORTABLE_DIR` und `ARDUINO_CORE_DIR` erlauben
abweichende Werkzeugpfade. Der Buildeinstieg ist `./EspCompile.sh`.
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

Relais, LED, Taster und Messwertauswahl werden über die gemeinsame
GnBasics-GPIO-Schnittstelle angesteuert. Vor dem Aktivieren der Ausgänge werden
die inaktiven Ausgangspegel gesetzt. Der Relaisstatus gilt erst nach erfolgreicher
Initialisierung als bekannt.
