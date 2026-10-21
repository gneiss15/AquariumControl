<!-- README.DE.md, Version: 1.35 -->
# FeederSimple

[English](README.md)

## Stand der Hardwaredokumentation

**Coming soon – unvollständig.** Fotos und die Beschreibung des Kaufteil-Umbaus stehen unter [Hardware](Hw/README.DE.md). Eine vollständige Baubeschreibung und Stückliste folgen.

FeederSimple verwendet denselben Zeitplan-, Web-, API-, WebSocket-, WLAN-, OTA- und Temperaturkern wie `Feeder`.

Die gemeinsamen Firmwarequellen und die lesbaren Webquellen liegen unter `../Libs/Common/Feeder`. Motor, Home-Eingang, lokaler Taster, Status-LED und Dallas-Diagnoseseite bleiben projektspezifisch.

## Hardware

Die Originalsteuerung wird nicht verwendet. Das vorhandene PCB bleibt wegen der Mechanik im Gerät und stellt die vorhandenen Bauteile beziehungsweise Anschlusspunkte bereit:

- Taster direkt am GPIO-Eingang, aktiv Low, mit internem Pull-up
- LED über R2 am GPIO-Ausgang, aktiv High
- Motor über R4 und Q1 am GPIO-Ausgang, aktiv High
- Home-Signal über R3 am GPIO-Eingang
- Versorgung der Platine und des ESP gemeinsam mit 3,3 V

U1-Pinbelegung und Anschlusspunkte sind in lokalen Unterlagen unter `doc/` dokumentiert; die öffentliche Hardwarebeschreibung wird noch vervollständigt. HIGH schaltet den Motor ein, LOW schaltet ihn aus. Die Firmware verwendet deshalb einen normalen Push-Pull-Ausgang. HOME ist aktiv LOW; ein interner HOME-Pull-up wird nicht verwendet.

Die konkreten ESP-Pins, Entprellzeiten und Motor-/Homing-Timeouts stammen ausschließlich aus `Sw/FeederConfig.inc.h` und werden daraus nach `Config.json` und `ConfigEntries.json` abgeleitet.

## Home-Eingang testen

Für HOME gelten `FeederSimpleHomeActiveLow=1` und `FeederSimpleHomeUsePullup=0`; die Timeout-Werte sind `HomeLeaveTimeoutMs=1000`, `RotationTimeoutMs=7500` und `HomingTimeoutMs=7500`. `DallasPresent=true` aktiviert die Temperaturmessung und die Anzeige im Dashboard. Der produktive OneWire-Pfad setzt genau einen DS18B20 voraus und benötigt deshalb keine ROM-Erkennung: alle 10 s `RESET -> SKIP ROM -> CONVERT T`, 1000 ms später `RESET -> SKIP ROM -> READ SCRATCHPAD`, bei CRC-Fehler bis zu 19 unmittelbare Read-Retries. Unter `http://<FeederSimple-IP>/Test` bleibt der Dallas-/OneWire-Stabilitätstest mit Presence, SEARCH ROM, READ ROM, SKIP ROM und MATCH ROM als unabhängiger Diagnosepfad erhalten. Für mechanische Belastungs-/Kontaktprüfungen kann die Seite zusätzlich einen abbrechbaren Dauertest ausführen; zwischen zwei Versuchen liegt eine kurze Pause, und die Einzelanzeige wird auf die letzten 100 Versuche begrenzt.

Bei einem Feed wird der Motorstart um einen Loop-Durchlauf verzögert, damit insbesondere bei einem Web-Manual-Feed zuerst die HTTP-Antwort gesendet werden kann. Während der eigentlichen Fütterung leuchtet die Status-LED dauerhaft.

## Status-LED und lokaler Taster

Die Status-LED liegt fest an `StatusLedPin` = GPIO13/D7 (`#define` in `Sw/HwConfig.FeederSimple.h`) und ist aktiv High. Im normalen Bootpfad ist dieser GPIO vor `FeederDrive.Setup()` noch nicht als Ausgang initialisiert; ein kurzes helles Aufleuchten oder schwaches Leuchten unmittelbar nach Reset ist daher noch kein normaler Betriebsstatus. Im Startup-Level 0 initialisiert der Recovery-Pfad den Pin dagegen direkt und lässt die LED etwa alle 150 ms blinken. Nach erfolgreichem Laden von LittleFS, `Config.json`, `Index.html` und `FeedTimes.json` setzt `FeederDrive.Setup()` die LED zunächst auf AUS.

Danach gilt:

- keine STA-WLAN-Verbindung: LED toggelt alle 500 ms
- WLAN verbunden, Zeit/NTP noch ungültig: LED toggelt alle 1 s
- WLAN verbunden und Zeit gültig: LED AUS
- während einer Fütterung: LED dauerhaft EIN

Bricht der Start bereits vor `FeederDrive.Setup()` ab, wird die LED von der Firmware noch nicht gesteuert und auch der lokale Taster ist noch nicht initialisiert.

Der lokale Taster liegt derzeit an GPIO0/D3, ist aktiv Low und wird beim Loslassen ausgewertet:

- Druck < `ButtonLongPressMs` (derzeit 5 s): eine Portion
- Ab `ButtonLongPressMs` (derzeit 5 s): LED blinkt schnell. Jetzt loslassen baut die WLAN-Verbindung neu auf.
- Weiter gedrückt halten bis `ButtonResetPressMs` (derzeit 10 s): LED geht aus, der temporäre AP+STA-Zugang wird sofort aktiviert. Loslassen löst keine Fütterung und keinen weiteren Reconnect aus.
- Der temporäre AP verwendet die konfigurierte AP-SSID und das AP-Passwort. Die normale Weboberfläche ist darüber erreichbar (üblicherweise `http://192.168.4.1/`). Der gespeicherte WLAN-Modus bleibt unverändert; nach erfolgreicher STA-Verbindung wird der konfigurierte Modus wiederhergestellt. Ein Neustart beendet den temporären Modus ebenfalls.
- `ButtonResetPressMs` ist der Konfigurationsname für die zweite Druckschwelle zum Aktivieren des AP-Zugangs.

GPIO0 ist gleichzeitig ein ESP8266-Boot-Strap-Pin. Der Taster darf deshalb beim Reset/Einschalten nicht gedrückt gehalten werden, sonst startet der ESP8266 in den seriellen Bootloader.

## Projekt- und Ausgabestruktur

- `Sw/`: projektlokale Firmwarequellen und Header einschließlich Config-Definition; `Version.inc.h` und `DeviceType.inc.h` werden hier generiert.
- `EspBoardConfig.sh` legt Board, Arduino-FQBN und LittleFS-Geometrie für Main, Rescue und Flash-Skript fest.
- `DevState/<Gerätename>/` enthält die maßgeblichen persistenten Gerätedaten.
- `doc/` enthält Hardwarebelegung und technische Unterlagen.
- `generated/build/main/` und `generated/build/rescue/` enthalten getrennte, löschbare Build-Caches.
- `generated/bin/<BoardId>_Arduino2/` enthält Main-, Rescue-, LittleFS- und Combined-Artefakte dieses Arduino-Ziels. Main- und Rescue-Clean löschen jeweils nur ihre eigenen Dateien.
- `generated/Instances/<Gerätename>/LittleFS/` ist das aus `../Libs/Common/Feeder/WebSrc` und `DevState` erzeugte vollständige LittleFS-Uploadabbild; `LittleFS-readable/` ist dessen lesbare Vergleichskopie.
- Unter `generated/Instances/<Gerätename>/` liegen außerdem die FTP-Readbacks `ftp/`, `ftp-readable/` und `ftp-devstate-readable/`.
- FeederSimple verwendet keine `ServoSteps.json`; seine Geräteinstanzen enthalten Konfiguration, Fütterungszeiten und die übrigen gemeinsamen Webdaten.

## Bauen und Aktualisieren

`EspBoardConfig.sh` ist aktuell für ein NodeMCU-ESP8266-Board mit 4 MB Flash und der Aufteilung `4M2M` konfiguriert. Für ein anderes Board oder Flashlayout müssen `FQBN`, `BoardId` sowie die LittleFS-Werte vor dem Build angepasst werden.

Vor jedem LittleFS-Build für ein vorhandenes Gerät zuerst dessen aktuelle Dateien laden und vergleichen:

```sh
./tools/DownloadLittleFS.sh FeederAq2
```

Vergleiche:

```text
generated/Instances/FeederAq2/ftp-readable/
  gegen generated/Instances/FeederAq2/LittleFS-readable/
generated/Instances/FeederAq2/ftp-devstate-readable/
  gegen DevState/FeederAq2/
```

Abweichungen bei `Config.json` vor dem Build klären und gewünschte Gerätedaten nach `DevState/FeederAq2/` übernehmen. Fütterungszeiten werden zentral synchronisiert.

Für die Firmware-Signierung wird das zentrale Schlüsselpaar `../../../PrjsPrivate/private.key` und `../../../PrjsPrivate/public.key` verwendet. Der private Schlüssel bleibt lokal und wird nicht archiviert.

Main und die instanzbezogenen LittleFS-Pakete werden gemeinsam gebaut:

```sh
./EspCompile.sh
```

Das Combined-Paket für FeederAq2 liegt anschließend unter:

```text
generated/bin/nodemcuv2_Arduino2/FeederAq2.ino.update.signed
```

Auf der Geräte-Webseite dieses Paket im Bereich „Fw-Update“ unter „Update package“ auswählen und das Update starten.

Alternativ können Main und LittleFS getrennt hochgeladen werden:

```text
generated/bin/nodemcuv2_Arduino2/FeederSimple.ino.bin.signed
generated/bin/nodemcuv2_Arduino2/FeederAq2.ino.littlefs.signed
```

Ein getrennter Upload setzt voraus, dass die neue Firmware mit dem vorhandenen LittleFS-Stand starten kann. Bei inkompatiblen Dateiformaten das Combined-Paket verwenden.

Rescue wird separat gebaut, verwendet aber dasselbe Boardprofil und denselben Bin-Ordner:

```sh
./EspRescueCompile.sh
```

Das empfohlene Rescue-Artefakt ist `generated/bin/nodemcuv2_Arduino2/FeederSimple.Rescue.ino.bin.gz.signed`.


## OneWire-Langzeitdiagnose

Der normale produktive Messzyklus protokolliert Boot und ausschließlich Zustandswechsel `OK`/`NOK` binär in `/OneWireDiag.bin`. Zähler unterscheiden Erfolg beim 1., 2. oder 3. Scratchpad-Read, endgültiges Read-NOK, Conversion-/Read-Reset, CRC und ungültige Temperatur. `/OneWireDiag/` zeigt die aktuelle Statistik und das Log, bietet CSV-Export sowie einen Reset vor einem Nachtlauf. Der manuelle Dallas-Test unter `/Test` pausiert den Produktivzyklus weiterhin und fließt nicht in diese Statistik ein.

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
kein Feld. Die Empfänger müssen Version 2 unterstützen.
