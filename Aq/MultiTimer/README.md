<!-- README.md, Version: 1.15 -->
# MultiTimer

## Hardware und Dokumentationsstand

Raspberry Pi mit achtfacher Relaiskarte und Netzteil im Conrad-IR-Powermanager-Gehäuse. **Coming soon – Hardwarebeschreibung unvollständig.** Die vorhandenen [Fotos und Hardwareunterlagen](Hw/README.DE.md) sind eingebunden; Stückliste, Anschlussplan und Einbaudetails folgen. Dashboard nutzt denselben Pi; der Raumtemperatur-Sensor wird separat angeschlossen.

MultiTimer ist der native Timer für Raspberry Pi und PC/Dummy. Produktversionen
stehen ausschließlich in `Versions.txt`; der Build erzeugt daraus `Version.inc.h`
und die Web-Versionsdatei. Dashboard und MultiTimer bleiben getrennte Prozesse.

## Verzeichnisse und Daten

- `src/`, `include/`: native und gemeinsam genutzte C++-Quellen.
- `WebSrc/`: lesbare Webquellen; PreBuild kopiert die statischen Webdateien nach data/; von dort werden sie ausgeliefert.
- `data/`: erzeugte Ausgangsdaten und veränderliche Laufzeitdateien.
- `generated/build/<target>/`: Compiler-Cache für `dummy` oder `raspi`.
- `generated/Instances/GN-Pi/raspi/`: rohe Rücklesekopien vom Pi.
- `generated/Instances/GN-Pi/raspi-readable/`: lesbare Gegenstücke.
- `generated/Instances/GN-Pi/local-readable/`: lokale Vergleichskopie für BCO.
- `include/ConfigDefaults.inc.h`: kompatible Beschreibung der Konfigurationsfelder
  und Standardwerte für die bestehende JSON-Konfigurationsseite.
- `ESP32-MultiSwitch.zip`: archivierter, nicht realisierter ESP-MultiSwitch-Entwurf.
  Seine separate Dokumentation liegt als `Aq/MyTimerWeb/README-ESP32-MultiSwitch.md`
  im ZIP. Das Archiv gehört nicht zum nativen Build oder Pi-Upload.

`Config.json`, `Channels.json`, `SwitchTimes.json` und `SwitchTimes.sync.json`
gehören zum Laufzeitbestand. Die Config.json im Projektstamm liefert dem nativen Programm seinen Namen.
Die separate data/Config.json ist die bisherige erzeugte Web-Konfiguration.
`Channels.json` enthält Profile und Kanalzuordnungen;
Schaltzeiten und Intervalle bleiben im bestehenden kombinierten Format.
Der Dashboard-Sync liefert die zentralen Werte. Bei Quell-/Webupdates dürfen
laufende Geräte-Konfiguration, Kanalzuordnungen und Schaltpläne nicht überschrieben
werden. `data/` kann private Daten enthalten und gehört nicht in Quellarchive.

Die Anwendung ermittelt `data/` und `Log/MultiTimer.log` relativ zur ausführbaren Datei.
`PreBuild.sh` erzeugt Ausgangsdateien aus WebSrc und den Konfigurationsdefinitionen.
Es ist kein Rücklese- oder Backupwerkzeug für laufende Pi-Daten. Beim erneuten
PreBuild wird der lokale data/-Bestand durch Ausgangsdaten ersetzt. DownloadData
schützt davor nicht automatisch. Der neue Download lädt den gesamten Pi-data/-Bestand
und die native Config.json zusätzlich zurück, formatiert die Kopien und öffnet BCO: lokal links,
Pi-Download rechts. Er übernimmt keine Dateien in lokale Build-/Laufzeitquellen.
NativeConfig.json im Vergleich ist die Config.json aus dem Projektstamm;
Config.json innerhalb des Downloadordners stammt aus data/.
Der Pi-Codeupload schließt die laufenden JSON-Dateien hingegen ausdrücklich aus.

## Bauen

PC/Dummy benötigt g++, make, libjson-c und pthreads. Raspberry Pi benötigt zusätzlich
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
`make clean` verschiebt nur Compiler-Caches in Trash und erhält MultiTimer;
`make full-clean` entfernt zusätzlich MultiTimer. Andere Quellen, Daten, doc und
Pi-Rücklesekopien werden dabei nicht entfernt. Wenn Trash fehlschlägt, bricht der
Aufruf ab. Die Pfade von MultiTimer, Config.json, data/ und Log/ im Projektstamm bleiben
für Pi und PC/Dummy erhalten.

Die ausführbare Datei heißt `MultiTimer`. `make TARGET_SYSTEM=dummy` bzw.
`make TARGET_SYSTEM=raspi` wählt das Ziel ebenfalls explizit. Nach einer frischen
Wiederherstellung ohne Laufzeitbestand erzeugt `./PreBuild.sh` die Ausgangsdaten.
Bestehende laufende Daten vorher sichern bzw. mit dem Pi-Stand abgleichen.

## Start und Dienst

```bash
./MultiTimer
./MultiTimer --port 8090
```

Standard ist HTTP 8080, WebSocket 8081. Bei `--port` nutzt WebSocket den nächsten
Port; HTTP muss kleiner als 65535 sein. `--log-start` protokolliert den ersten Start,
`--ReStart` einen Neustart.

Der Entwicklungsbaum liegt unter `Prjs/Aq/MultiTimer`, die Pi-Installation unter
`/home/pi/Prjs/MultiTimer`. Der bestehende Pi-Datenstand wurde beim Umbenennen vollständig erhalten. Der Dienst heißt `MultiTimer.service`.
Ein über die Weboberfläche angeforderter Neustart beendet MultiTimer; systemd startet
es über `Restart=always` neu. Der Webprozess bekommt dafür keine sudo-Rechte.

Vom PC aus startet `bash Aq/MultiTimer/CopyToRasPi.sh` die vorhandene Übertragung
mit interaktiver Zustimmung. `RASPI_HOST` und `RASPI_PRJS_DIR` überschreiben Ziel
und Pfad. Laufende Pi-Daten bleiben beim Quellupdate erhalten. Das Archiv wird
über den rsync-Filter vom Transfer ausgeschlossen.

Nach einer ausdrücklich gewünschten Übertragung wird auf dem Pi im dortigen
MultiTimer-Verzeichnis gebaut. `Install.sh` installiert/startet den vorhandenen
systemd-Dienst und entfernt frühere MultiTimer-Start-/Watch-Cronzeilen; Wartungsjobs
für RebootPi und FreeCheck bleiben erhalten.

```bash
systemctl status MultiTimer.service
journalctl -u MultiTimer.service -f
```

`StartService.sh` unterscheidet Erststart und Neustart innerhalb desselben Boots.
`tools/DownloadData.sh` dient zum Rücklesen des Pi-Datenbestands; die bestehenden
Dienst-/Übertragungswerkzeuge bleiben erhalten.

## Archivierter MultiSwitch

Die ESP32-/ESP8266-Portierung war ein möglicher zusätzlicher MultiSwitch und wird
laut Benutzer mit den vorhandenen NOUS-/Shelly-Geräten nicht aufgebaut.
ESP-spezifische Hardware-/Config-/Logger-/Webserver-Dateien, Sketch sowie Build-/
Flash-Einstiege sind in `ESP32-MultiSwitch.zip` archiviert. Der damalige gesamte
README- und gemeinsame Quellstand bleibt dort zur Einordnung erhalten.

Gemeinsam genutzte, bedingt kompilierte ESP-Zweige in nativen Quelldateien bleiben
bei dieser Archivierung unverändert. Sie sind kein aktiver ESP-Buildeinstieg;
so bleibt die bereits verwendete Pi-Implementierung ohne zusätzlichen Umbau erhalten.

Die unbenutzte alternative 16-Kanal-Vorlage Channels.opt.json und die nicht mehr
benötigte CMake-Metadatei sind im ESP32-MultiSwitch.zip mit ihrer Beschreibung
archiviert. Die aktive Kanalbelegung wird dadurch nicht verändert.

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

JSON-Editoren: Objektarrays (Profile/Kanäle) werden mit einer ausgerichteten Zeile pro Datensatz dargestellt; Zusatzfelder bleiben erhalten. Channels.json nutzt die verfügbare Seitenbreite. Aktueller Webstand: MultiTimer Web 2.29.

Channels.json besitzt wie Sources.json eine umrahmte Box mit innerem Textfeld. Sources.json ist initial zugeklappt und wird über den Button Sources.json … angezeigt bzw. wieder verborgen. Die Dashboard-Erreichbarkeitssperre bleibt serverseitig und im Editor bestehen.

## Kanaltausch aus dem Dashboard

MultiTimer 2.56/Web 2.31 unterstützt den HW-Endpunkt-Tausch. PhysPos Main 1..8 bleibt die feste Steckdose; Id, Rolle, Profil und Zeiten wechseln mit der Funktion. Beim Heizungsrollentausch gelten am selben Anschluss anschließend die Grenzen der neuen Rolle. GPIO/Taster/Polarität und Arrayposition bleiben erhalten.

Kanäle und Zeiten werden gemeinsam geprüft und über data/Configuration.transaction.json wiederherstellbar gespeichert. Nach Unterbrechung vervollständigt der Start die Transaktion vor der Hardwareinitialisierung. Bei ausstehender Dateimaterialisierung liefern Konfigurationsdatei-Zugriffe HTTP 409; bestehende Dateien werden nicht als gemischter Stand zurücksynchronisiert. Erst nach erfolgreicher Aktivierung wird die Zuordnungsrevision bestätigt.

Belegung und Listboxen stammen aus Channels.json. Eine offene Seite fragt /api/v1/config-revision beim Aktivieren einmal ab und lädt nur bei geänderter Revision nach. Der Dashboard-Bereichswechsel sendet ConfigurationActivated. Ungespeicherte Eingaben bleiben erhalten; bei Änderung erscheint ein Hinweis zum kontrollierten Neuladen. Kein Konfigurationspolling-Timer.

## Umbenennung und Kompatibilität

Projekt, Programm, Dienst und Weboberfläche heißen MultiTimer. Das bestehende MAC@Service-ID-Schema, GN-Pi als Funktions-/Transportschlüssel und der technische Zeitdaten-Formatbezeichner `MyTimer` bleiben kompatibel. Archivierte Projekte und historische Snapshot-/Arbeitsnachweise behalten ihre ursprünglichen Namen. Zugangsdaten und bestehende Gerätekonfiguration werden nicht durch den neuen Projektnamen verändert.

Der Pi-Übergang ist durchgeführt. Alte ausführbare Datei, Dienstvorlage, installierte Unit und vorheriger Cronstand liegen wiederherstellbar unter `generated/name-migration/<UTC-Zeitstempel>/`. Die bisherige Logdatei liegt jetzt unter `Log/MultiTimer.log`. Der einmalige Übergangscode wurde nach erfolgreicher Installation entfernt. Reguläre Quellupdates verwenden ausschließlich MultiTimer; auf dem Pi nach `make` den Dienst neu starten. `./Install.sh` installiert bzw. erneuert die Unit und die Wartungsjobs.

UDP-Empfang verarbeitet ausschließlich Version 2 mit benannten numerischen
Feldern in `Status`. Profile wählen mit `Measurement` den Messwert, standardmäßig
`Temperature`. Alte Root-Felder werden nicht ausgewertet. MultiTimer 2.61 sendet
bereits Version 2; die Quellbereinigung für den nächsten Build 2.62 entfernt
nur den alten Version-1-Empfang und ist noch nicht erneut ausgerollt.
