<!-- README.DE.md, Version: 1.18 -->
# MultiTimer

[English](README.md)

MultiTimer ist eine achtfach-Schaltsteckdose.

## Hardware und Dokumentationsstand

Raspberry Pi mit achtfach-Relaiskarte und Netzteil im Conrad-IR-Powermanager-Gehäuse und (optionalem) Raumtemperatursensor.
Die Programme Dashboard und MultiTimer laufen auf demselben Raspberry Pi.
MultiTimer steuert die Schaltausgänge der achtfach-Relaiskarte anhand von Zeitplänen und messwertabhängigen Profilen.
Mit seinen acht Schaltausgängen übernimmt MultiTimer die Steuerungsaufgaben von acht einzelnen Nous- oder Shelly-Schaltsteckdosen.

**Coming soon – Hardwarebeschreibung unvollständig.** [Fotos und Hardwareunterlagen](Hw/README.DE.md) zeigen den Aufbau. Stückliste, Anschlussplan und Einbaudetails folgen.

## Verzeichnisse und Daten

- `src/`, `include/`: native und gemeinsam genutzte C++-Quellen.
- `WebSrc/`: lesbare Webquellen; PreBuild kopiert die statischen Webdateien nach data/; von dort werden sie ausgeliefert.
- `data/`: erzeugte Ausgangsdaten und veränderliche Laufzeitdateien.
- `generated/build/<target>/`: Compiler-Cache für `dummy` oder `raspi`.
- `generated/Instances/GN-Pi/raspi/`: rohe Rücklesekopien vom Pi.
- `generated/Instances/GN-Pi/raspi-readable/`: lesbare Gegenstücke.
- `generated/Instances/GN-Pi/local-readable/`: lokale Vergleichskopie für BCO.
- `include/ConfigDefaults.inc.h`: Beschreibung der Konfigurationsfelder
  und Standardwerte für die JSON-Konfigurationsseite.

`Config.json`, `Channels.json`, `SwitchTimes.json` und `SwitchTimes.sync.json`
gehören zum Laufzeitbestand. Die Config.json im Projektstamm liefert dem nativen Programm seinen Namen.
Die separate `data/Config.json` enthält die erzeugte Web-Konfiguration.
`Channels.json` enthält Profile und Kanalzuordnungen;
`SwitchTimes.json` enthält Schaltzeiten und Intervalle.
Der Dashboard-Sync liefert die zentralen Werte. Bei Quell-/Webupdates dürfen
laufende Geräte-Konfiguration, Kanalzuordnungen und Schaltpläne nicht überschrieben
werden. `data/` kann private Daten enthalten und gehört nicht in Quellarchive.

Die Anwendung ermittelt `data/` und `Log/MultiTimer.log` relativ zur ausführbaren Datei.
`PreBuild.sh` erzeugt Ausgangsdateien aus WebSrc und den Konfigurationsdefinitionen.
Es ist kein Rücklese- oder Backupwerkzeug für laufende Pi-Daten. Beim erneuten
PreBuild wird der lokale data/-Bestand durch Ausgangsdaten ersetzt. `tools/DownloadData.sh`
schützt davor nicht automatisch. Es lädt den gesamten Pi-data/-Bestand
und die native Config.json zusätzlich zurück, formatiert die Kopien und öffnet BCO: lokal links,
Pi-Download rechts. Er übernimmt keine Dateien in lokale Build-/Laufzeitquellen.
NativeConfig.json im Vergleich ist die Config.json aus dem Projektstamm;
Config.json innerhalb des Downloadordners stammt aus data/.
Der Pi-Codeupload schließt die laufenden JSON-Dateien hingegen ausdrücklich aus.

## Bauen

Der PC-Testmodus simuliert die Hardware ohne angeschlossene Relais.
Benötigt werden Python 3.10 oder neuer, ein C++20-fähiges g++, make, libjson-c und pthreads. Raspberry Pi benötigt zusätzlich
libgpiod 2.x für die GPIO-Implementierung. Das Makefile erkennt einen Raspberry Pi
über `/proc/device-tree/model` und wählt dort automatisch das Pi-Ziel.

```bash
make
```

Explizite Ziele:

```bash
make dummy
make raspi
```

`make build` leert die Compiler-Caches und baut anschließend vollständig neu.
`make clean` löscht die Compiler-Caches und erhält das Programm `MultiTimer`;
`make full-clean` löscht zusätzlich das Programm. Quellen, Konfiguration,
Laufzeitdaten und Pi-Rücklesekopien bleiben erhalten.

Die ausführbare Datei heißt `MultiTimer`. `make TARGET_SYSTEM=dummy` bzw.
`make TARGET_SYSTEM=raspi` wählt das Ziel ebenfalls explizit. Bei einer Einrichtung
ohne Laufzeitbestand erzeugt `./PreBuild.sh` die Ausgangsdaten.
Bestehende laufende Daten vorher sichern bzw. mit dem Pi-Stand abgleichen.

## Start und Dienst

```bash
./MultiTimer
./MultiTimer --port 8092
```

Standard ist HTTP 8080, WebSocket 8081. Bei `--port` nutzt WebSocket den nächsten
Port; HTTP muss kleiner als 65535 sein. `--log-start` protokolliert den ersten Start,
`--ReStart` einen Neustart.

Der Standardpfad auf dem Pi ist `/home/pi/Prjs/MultiTimer`.
Der Dienst heißt `MultiTimer.service`.
Ein über die Weboberfläche angeforderter Neustart beendet MultiTimer; systemd startet
es über `Restart=always` neu. Der Webprozess bekommt dafür keine sudo-Rechte.

Die Übertragung vom PC aus in einem interaktiven Terminal vom Repository-Stamm starten:

```bash
./MultiTimer/CopyToRasPi.sh
```

`RASPI_HOST` und `RASPI_PRJS_DIR` überschreiben Ziel und Pfad.
Der Transfer verlangt eine Bestätigung. Laufende Pi-Daten bleiben beim Quellupdate erhalten.

Nach der Übertragung auf dem Pi im MultiTimer-Verzeichnis mit `make` bauen.
`Install.sh` installiert und startet den systemd-Dienst sowie die Wartungsjobs.
Bei einer bestehenden Installation den Dienst nach dem Neubau mit
`sudo systemctl restart MultiTimer.service` neu starten.

```bash
systemctl status MultiTimer.service
journalctl -u MultiTimer.service -f
```

`StartService.sh` unterscheidet Erststart und Neustart innerhalb desselben Boots.
`tools/DownloadData.sh` dient zum Rücklesen des Pi-Datenbestands.

## Geräteidentität und Funktionstausch

Jedes physische Gerät wird anhand seiner MAC-Adresse und Dienstkennung erkannt.
Im Dashboard lassen sich Funktionen zwischen funktionskompatiblen Geräten bzw.
einzelnen Schaltausgängen tauschen. Hardwarekonfiguration und Kalibrierung bleiben
beim physischen Gerät. Die Bedienung beschreibt
[Dashboard – Geräteidentität und Funktionstausch](../Dashboard/README.DE.md#geräteidentität-und-funktionstausch).
Technische Einzelheiten stehen in [DeviceIdentity.md](../Libs/GnBasicLib/DeviceIdentity.md).

## Gerätezuordnung und PC-Dateien

Ein Funktionstausch allein erfordert keinen Dateisystem-Upload. Soll später ein
ESP-Dateisystem neu aufgespielt werden, müssen vor dem Erstellen des Abbilds die
PC-Daten mit der aktuellen Gerätezuordnung abgeglichen werden. Den folgenden
Befehl im Stammverzeichnis des AquariumControl-Repositories ausführen:

```bash
python3 tools/ReadDeviceStates.py --apply
```

Dashboard selbst hat keinen Zugriff auf die PC-Dateien. Für MultiTimer werden die
laufenden Pi-Daten über `tools/DownloadData.sh` zurückgelesen; der Quelltransfer
mit `CopyToRasPi.sh` erhält sie auf dem Pi.

JSON-Editoren: Objektarrays (Profile/Kanäle) werden mit einer ausgerichteten Zeile pro Datensatz dargestellt; Zusatzfelder bleiben erhalten. Channels.json nutzt die verfügbare Seitenbreite.

Channels.json besitzt wie Sources.json eine umrahmte Box mit innerem Textfeld. Sources.json ist initial zugeklappt und wird über den Button Sources.json … angezeigt bzw. wieder verborgen. Der lokale Sources-Editor ist nur bei nicht erreichbarem Dashboard freigegeben; der Server prüft dies beim Speichern erneut.

## Kanaltausch aus dem Dashboard

Beim Kanaltausch bezeichnet `PhysPos: "Main X"` die feste Steckdose 1 bis 8; Id, Rolle, Profil und Zeiten wechseln mit der Funktion. Beim Heizungsrollentausch gelten am selben Anschluss anschließend die Grenzen der neuen Rolle. GPIO/Taster/Polarität und Arrayposition bleiben erhalten.

Kanäle und Zeiten werden gemeinsam geprüft und gespeichert. Unterbrochene
Zuordnungsänderungen werden beim Neustart vor der Hardwareinitialisierung
abgeschlossen. Erst nach erfolgreicher Übernahme bestätigt MultiTimer die neue
Zuordnung. Während einer unvollständigen Übernahme ist der Zugriff auf die
betroffenen Konfigurationsdateien gesperrt.

Belegung und Listboxen stammen aus Channels.json. Eine offene Seite fragt /api/v1/config-revision beim Aktivieren einmal ab und lädt nur bei geänderter Revision nach. Der Dashboard-Bereichswechsel sendet ConfigurationActivated. Ungespeicherte Eingaben bleiben erhalten; bei Änderung erscheint ein Hinweis zum kontrollierten Neuladen. Kein Konfigurationspolling-Timer.

## Messwerte

MultiTimer empfängt Messwerte über UDP-Version 2 als benannte numerische Felder
im Objekt `Status`. Profile wählen mit `Measurement` den auszuwertenden Messwert,
beispielsweise `Temperature` oder `BatteryPercent`. Ein leeres Messwertfeld bedeutet
`Temperature`.
