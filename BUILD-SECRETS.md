<!-- BUILD-SECRETS.md, Version: 1.07 -->
# Build-private Dateien und gerätespezifischer Zustand

Für die ESP-Projekte werden drei Arten von Quellen getrennt:

- `WebSrc/`: gemeinsame statische LittleFS-Quellen eines Projekts.
- `DevState/<Device>/`: vollständiger persistenter Zustand genau einer Geräteinstanz.
- `../PrjsPrivate/`: lokale projektweite private Dateien außerhalb des `Prjs`-Workspace, z. B. Signierschlüssel und zentrale Build-Secrets.

`DevState/<Device>/Config.json` enthält die vollständige Gerätekonfiguration einschließlich gerätespezifischer Passwörter (`WlanPw`, `WlanPw2`, `ApPw`, `FtpPw`). Diese Werte gehören nicht in eine gemeinsame Projekt-Config, weil mehrere Geräteinstanzen desselben Projekts unterschiedliche Zugangsdaten haben können.

`../PrjsPrivate/Secrets.local.json` bleibt für zentrale beziehungsweise nicht gerätespezifische Build-Secrets und für Projekte ohne `DevState` erhalten. Bei einem Device-Build ist `DevState/<Device>/Config.json` die letzte und damit maßgebliche Quelle; Werte aus `../PrjsPrivate/Secrets.local.json` dürfen Gerätewerte nicht überschreiben.

## Private-Verzeichnis

Die lokalen privaten Dateien liegen unter:

```text
PrjsPrivate/private.key
PrjsPrivate/public.key
PrjsPrivate/Secrets.example.json
PrjsPrivate/Secrets.local.json
```

`PrjsPrivate/` liegt außerhalb des von `CreateProjectZip.sh` archivierten `Prjs/`-Baums und wird daher nicht in Projekt-ZIPs aufgenommen. Die alte `Prjs/Private/`-Ausschlussregel darf als zusätzliche Leckschutz-Sicherung bestehen bleiben.

Für die Firmware-Signierung verwenden alle signierten ESP-Builds das gemeinsame Schlüsselpaar `../PrjsPrivate/private.key` und `../PrjsPrivate/public.key`.

## DevState

Gerätespezifische persistente LittleFS-Daten liegen beispielsweise unter:

```text
Feeder/DevState/FeederAq1/
  Config.json
  FeedTimes.json
  FeedTimes.sync.json
  ServoSteps.json

FeederSimple/DevState/FeederAq2/
  Config.json
  FeedTimes.json
  FeedTimes.sync.json

TmrSw/DevState/LightTimerAq3/
  Config.json
  Channels.json
  SwitchTimes.json
  SwitchTimes.sync.json
```

`*.sync.json` ist dabei eine normale persistente Geräte-Datei. Sie wird weder speziell behandelt noch vom Uploader besonders geschützt: sie gehört in `DevState/<Device>/` und wird von dort in das gerätespezifische LittleFS übernommen.

`CreateProjectZip.sh` schließt jedes `DevState/` vollständig aus, weil dort die vollständige `Config.json` mit gerätespezifischen Passwörtern liegt. Beim Aktualisieren eines Projekt-ZIPs bleibt der lokale `DevState`-Baum deshalb erhalten und wird nicht aus dem Archiv wiederhergestellt.

## Build-Ablauf der Geräteprojekte

Für Feeder, FeederSimple, TmrSw und TmrSwShelly gilt:

1. `EspCompile.sh` ermittelt alle Unterverzeichnisse unter `DevState/`.
2. Für jedes Device erzeugt `PreBuild.sh` aus gemeinsamem `WebSrc/` plus `DevState/<Device>/` den vollständigen LittleFS-Stand unter `generated/<Device>/data/`.
3. `ConfigEntries.json` und die Struktur/Defaults von `Config.json` werden weiterhin aus der jeweiligen `*Config.inc.h` abgeleitet. Die vollständige Geräte-Config aus `DevState/<Device>/Config.json` wird danach als letzte Quelle angewendet.
4. Die Firmware wird nur einmal pro Projekt kompiliert, weil sie für alle Geräteinstanzen desselben Projekts identisch ist.
5. Das LittleFS wird pro Device erzeugt.
6. Das Combined-Update wird aus der gemeinsamen Firmware und dem jeweiligen Geräte-LittleFS erzeugt.

Beispiel TmrSw:

```text
TmrSw.ino.bin.signed                  # gemeinsame Firmware
LightTimerAq3.ino.littlefs.signed     # gerätespezifisches FS
LightTimerAq3.ino.update.signed       # Firmware + dieses FS
```

Weitere TmrSw-Geräte werden allein durch weitere Unterverzeichnisse unter `TmrSw/DevState/` ergänzt.

Die früheren Projektverzeichnisse `TmrSw/Config/` und `TmrSwShelly/Config/` sind obsolete und werden nicht mehr verwendet.

## Download- und Vergleichsablauf

`tools/DownloadLittleFS.sh <Device>` erzeugt zunächst:

```text
generated/ftp-readable/<Device>/
```

Der erste Beyond-Compare-Vergleich ist:

```text
generated/ftp-readable/<Device>/
<->
generated/<Device>/data-readable/
```

Damit wird der reale Gerätestand mit genau dem letzten für dieses Gerät gebauten LittleFS verglichen.

Nur wenn der zweite Vergleich gewünscht wird, erzeugt das Skript zusätzlich:

```text
generated/ftp-devstate-readable/<Device>/
```

Darin liegen ausschließlich Dateien, die auch unter `DevState/<Device>/` geführt werden. Der zweite Vergleich ist damit direkt:

```text
generated/ftp-devstate-readable/<Device>/
<->
DevState/<Device>/
```

Gewünschte Laufzeitänderungen können so gezielt nach `DevState/<Device>/` zurückübernommen werden.

## Generierte Daten und Projekt-ZIP

Unter `generated/` dürfen vollständige Build-Daten einschließlich Passwörtern liegen. Dieses Verzeichnis ist rein generiert und wird vollständig von `CreateProjectZip.sh` ausgeschlossen.

Das Projekt-ZIP schließt damit mindestens aus:

```text
../PrjsPrivate/
*/DevState/
*/generated/
```

Die sensiblen gerätespezifischen Daten bleiben lokal im Projektbaum, werden aber nicht in das portable Projektarchiv aufgenommen.

`Aq/CleanGeneratedConfig.sh` entfernt bei Bedarf Konfigurationskopien aus den generierten Verzeichnissen. Für den normalen Build ist das nicht erforderlich.

## Arduino-Library-Pfade

Projektlokale Arduino-Libraries werden immer einzeln als exakter Library-Root mit `--library` an `arduino-cli compile` übergeben, z. B.:

```bash
--library "$PrjsDir/Libs/GnBasicLib"
```

Sie dürfen nicht durch `--libraries "$PrjsDir/Libs"` ersetzt werden. Der übergeordnete Suchpfad hat bei den Projekten wiederholt `Multiple libraries were found ...` verursacht.
