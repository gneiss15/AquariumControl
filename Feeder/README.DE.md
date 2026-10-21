<!-- Feeder/README.DE.md, Version: 1.27 -->
# Feeder

[English](README.md)

Feeder ist ein DIY Futterautomat mit OLED und Drehencoder.

Zeitplan, Konfiguration, WLAN/Webserver, Status-API, Temperatur und WebSocket werden gemeinsam mit `FeederSimple` aus `../Libs/Common/Feeder` eingebunden. Die Servo-, OLED- und Drehencoder-Implementierung bleibt ausschließlich in diesem Projekt.

Die gemeinsame Weboberfläche liegt unter `../Libs/Common/Feeder/WebSrc`. `ServoSteps.json` und die zugehörige Konfigurationsseite werden nur von diesem Servo-Feeder verwendet.

## Hardware und Dokumentation

[Hardwareübersicht](Hw/README.DE.md) · [Interaktive Explosionsansicht](Hw/ExplodedView.DE.md) · [Stückliste](Hw/BOM.DE.md) · [Verdrahtungsplan](Hw/Schematics/Wiring.DE.md)

Das mechanische Modell liegt in `Hw/CAD/Feeder.FCStd`. Die konstruierte Variante [FeederV2](Hw/CAD/FeederV2.FCStd) ergänzt einen O-Ring in `Top` und eine aktualisierte CAD-Explosionsansicht; sie ist noch nicht praktisch getestet. Einzelheiten stehen in der [Hardwareübersicht](Hw/README.DE.md). Die Elektronik verwendet gekaufte Module; eigene PCB-Entwürfe sind nicht vorgesehen.

## Projekt- und Ausgabestruktur

- `Hw/`: Hardwareunterlagen, CAD, Bilder und Verdrahtungspläne.
- `doc/`: lokale Arbeitsdokumente; über die projektbezogene `.gitignore` von Git ausgeschlossen. Vorhandene Dateien bleiben auf der Festplatte.
- `Sw/`: projektlokale Firmwarequellen und Header einschließlich Config-Definition; `Version.inc.h` und `DeviceType.inc.h` werden hier generiert.
- `EspBoardConfig.sh` legt Board, Arduino-FQBN, LittleFS-Geometrie und den gemeinsamen Bin-Unterordner für Main und Rescue fest.
- `DevState/<Gerätename>/` enthält die maßgeblichen persistenten Gerätedaten.
- `generated/build/main/` und `generated/build/rescue/` enthalten getrennte, löschbare Build-Caches.
- `generated/bin/<BoardId>_Arduino2/` enthält Main-, Rescue-, LittleFS- und Combined-Artefakte dieses Arduino-Ziels. Main- und Rescue-Clean löschen jeweils nur ihre eigenen Dateien.
- `generated/Instances/<Gerätename>/LittleFS/` ist das aus der gemeinsamen `Libs/Common/Feeder/WebSrc` und `DevState` erzeugte vollständige LittleFS-Uploadabbild; `LittleFS-readable/` ist dessen lesbare Vergleichskopie.
- Unter `generated/Instances/<Gerätename>/` liegen außerdem die FTP-Readbacks `ftp/`, `ftp-readable/` und `ftp-devstate-readable/`. `ftp/` ist der rohe Geräte-Readback, `ftp-readable/` dessen lesbare Darstellung und `ftp-devstate-readable/` der ausgewählte Vergleich mit `DevState`.
- `generated/data/Versions.txt` ist die generierte Versionsdatei für den projektweiten Fallback-Datenbereich; sie ist keine zusätzliche Versionsquelle. Reguläre Instanz-Builds erhalten ihre `Versions.txt` im jeweiligen `LittleFS/`.

`ServoSteps.json` speichert jeden Schritt als `[PositionGrad, SpeedMs, HoldMs, PositionTicks]`. `PositionGrad` ist der vom Nutzer eingegebene, gültige Gradwert und bleibt für die Lesbarkeit unverändert erhalten. `PositionTicks` ist der kanonische Maschinen-Sollwert und wird bei vorhandenem Feld direkt verwendet; dadurch entsteht beim erneuten Laden/Speichern kein Grad->Ticks->Grad-Drift. Alte 3-Feld-Einträge `[PositionGrad, SpeedMs, HoldMs]` werden beim Laden einmalig in Ticks umgerechnet und anschließend automatisch im 4-Feld-Format gespeichert.

Beim Web-Manual-Feed wird der Servostart nur vorgemerkt und erst im nächsten `TFeederServo::Loop()` ausgeführt. Damit kann die HTTP-Antwort den ESP verlassen, bevor der Timer1-/NMI-gesteuerte Servolauf beginnt. `Running()` berücksichtigt auch einen bereits vorgemerkten Feed.

## OneWire-Diagnose

Der produktive Dallas-Messpfad arbeitet in einem festen 10-s-Raster mit `TGnCoopTimerMs`: `RESET -> SKIP ROM -> CONVERT T`, 1000 ms später `RESET -> SKIP ROM -> READ SCRATCHPAD`. Bei ungültiger Scratchpad-CRC wird unmittelbar bis zu 19-mal erneut gelesen; die Statistik zählt getrennt Erfolg im 1. bis 20. Read sowie endgültiges NOK. Ein kompaktes Binärlog `/OneWireDiag.bin` speichert nur Boot sowie Wechsel `OK`/`NOK`; Poll-/Fehlerzähler bleiben im RAM. Unter `/OneWireDiag/` stehen Statistik, Zustandswechsel, CSV-Export und Reset für einen Langzeittest zur Verfügung. Das rohe FTP-Abbild behält die Binärdatei als Fallback, `ftp-readable` blendet sie für Beyond-Compare aus.

## Dallas / Heltec WiFi Kit 8

Der Default für den optionalen DS18B20 ist `DallasPin = 2` (GPIO2). Der externe 4,7-kΩ-Pull-up liegt nach 3,3 V. GPIO4/D2 und GPIO5/D1 sind durch das OLED belegt; GPIO15/D8 ist wegen seiner Boot-Strap-Bedingung LOW nicht für eine OneWire-Leitung mit Pull-up nach 3,3 V vorgesehen.

## Statusanzeige

Der Servo-`Feeder` besitzt im Unterschied zu `FeederSimple` keine von der Firmware definierte `StatusLedPin`-Status-LED. Eine eventuell auf dem ESP8266-/Heltec-Board vorhandene LED wird vom Feeder-Code nicht als Betriebsstatus angesteuert; ihr Verhalten beim Reset/Boot ist daher nicht als Statusanzeige zu interpretieren.

Der Betriebsstatus wird über das OLED angezeigt. Im Statusmenü gilt insbesondere:

- `StIp: connecting`: STA-WLAN ist noch nicht verbunden
- `StIp: <Adresse>`: STA-WLAN ist verbunden
- `NTP: --`: Zeit noch nicht gültig
- `NTP: OK`: Zeit synchronisiert/gültig

Bei einem Startfehler vor Initialisierung des OLED-/Menüpfads ist auch aus einer Board-LED kein verlässlicher Fehlerzustand ableitbar; hierfür ist der serielle Bootlog maßgeblich.

## Bauen und Aktualisieren

Vor jedem LittleFS-Build für ein vorhandenes Gerät zuerst dessen aktuelle Dateien laden und vergleichen:

```sh
./tools/DownloadLittleFS.sh FeederAq1
```

Vergleiche:

```text
generated/Instances/FeederAq1/ftp-readable/
  gegen generated/Instances/FeederAq1/LittleFS-readable/
generated/Instances/FeederAq1/ftp-devstate-readable/
  gegen DevState/FeederAq1/
```

Abweichungen bei `Config.json` und `ServoSteps.json` vor dem Build klären und gewünschte Gerätedaten nach `DevState/FeederAq1/` übernehmen.

Main und die instanzbezogenen LittleFS-Pakete werden gemeinsam gebaut:

```sh
./EspCompile.sh
```

Das Combined-Paket für FeederAq1 liegt anschließend unter:

```text
generated/bin/wifi_kit_8_Arduino2/FeederAq1.ino.update.signed
```

Auf der Geräte-Webseite dieses Paket im Bereich „Fw-Update“ unter „Update package“ auswählen und das Update starten.

Alternativ können Main und LittleFS getrennt hochgeladen werden:

```text
generated/bin/wifi_kit_8_Arduino2/Feeder.ino.bin.signed
generated/bin/wifi_kit_8_Arduino2/FeederAq1.ino.littlefs.signed
```

Diese Dateien im Bereich „Update Firmware and/or Filesystem“ auswählen. Die Firmware ist für alle Feeder-Instanzen gleich; das LittleFS ist gerätespezifisch.

Ein getrennter Upload setzt voraus, dass die neue Firmware mit dem vorhandenen LittleFS-Stand starten kann. Bei inkompatiblen Dateiformaten das Combined-Paket verwenden.

Rescue wird separat gebaut, verwendet aber dasselbe Boardprofil und denselben Bin-Ordner:

```sh
./EspRescueCompile.sh
```

Das empfohlene Rescue-Artefakt ist `generated/bin/wifi_kit_8_Arduino2/Feeder.Rescue.ino.bin.gz.signed`.

**Auf einem neuen Node muss Rescue einmalig über USB installiert werden.**


## Geräteidentität und Funktionstausch

Jedes physische Gerät wird anhand seiner MAC-Adresse und Dienstkennung erkannt.
Im Dashboard lassen sich Funktionen zwischen funktionskompatiblen Geräten tauschen.
Hardwarekonfiguration und Kalibrierung bleiben beim physischen Gerät. Die Bedienung
beschreibt [Dashboard – Geräteidentität und Funktionstausch](../Dashboard/README.DE.md#geräteidentität-und-funktionstausch).
Technische Einzelheiten stehen in [DeviceIdentity.md](../Libs/GnBasicLib/DeviceIdentity.md).

## Gerätezuordnung und PC-Dateien

Ein Funktionstausch allein erfordert keinen Dateisystem-Upload. Soll später ein
Dateisystem neu aufgespielt werden, müssen vor dem Erstellen des Abbilds die
PC-Daten mit der aktuellen Gerätezuordnung abgeglichen werden. Den folgenden
Befehl im Stammverzeichnis des AquariumControl-Repositories ausführen:

```bash
python3 tools/ReadDeviceStates.py --apply
```

Dashboard selbst hat keinen Zugriff auf die PC-Dateien. Danach die betroffenen
Dateisystem-Abbilder neu bauen; Abbilder verschiedener Hardwaretypen sind nicht
austauschbar.

UDP-Statusmeldungen verwenden Version 2 mit dem optionalen Temperaturmesswert in
`Status.Temperature`: bei ungültiger Messung `null`, bei deaktiviertem Sensor
kein Feld.
