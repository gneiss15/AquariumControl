<!-- README.DE.md, Version: 1.31 -->
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
`../Common/EspRescue/_EspRescueCompileEsp32Common.sh` mit `source` ein.
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
Rescue wird direkt mit ESP-IDF 5.5.1 gebaut. Die Partitionstabelle liegt bei `0x8000`.
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
Technische Einzelheiten stehen in [DeviceIdentity.md](../Libs/GnAqPrjsLib/DeviceIdentity.md).

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

## Ladenachlauf für Akku-Profile

Im Dashboard-Profil **Ladenachlauf aktiv** einschalten und **Sekunden je Prozentpunkt** angeben. Das setzt `Measurement: "BatteryPercent"` voraus. Der Editor schlägt zunächst `15` Sekunden je Prozentpunkt vor; derselbe Faktor wird anschließend durch Lernen angepasst. Ohne aktivierten Ladenachlauf bleibt das bisherige Verhalten bei fehlenden Messwerten erhalten.

Wenn der Sender ausbleibt und der Ausgang zuvor automatisch EIN war, berechnet das Schaltgerät die Restladezeit:

`(Abschaltgrenze − letzter Akkustand) × Sekunden je Prozentpunkt`

Die seit der letzten Meldung bereits geladene Zeit wird angerechnet. Ein zuvor ausgeschalteter Ausgang wird dadurch nicht eingeschaltet. Jeder manuelle Schaltbefehl verwirft den Lernzyklus sofort, auch beim Zurückschalten zur Automatik. Aus dem abgebrochenen Zyklus wird kein Faktor gelernt. Zeitplan AUS oder geänderte Profilparameter beenden den laufenden Zyklus ebenfalls. Es gibt keine zusätzliche maximale Nachlaufzeit.

Bei der nächsten gültigen Akkumeldung wird aus tatsächlicher Einschaltzeit und Akkugewinn ein Faktor ermittelt. Ab einem Gewinn von einem Prozentpunkt wird er mit 25 % gewichtet; 75 % stammen vom bisherigen Faktor. Ohne messbaren Gewinn bleibt der Faktor unverändert. Der neue Faktor wird auf ganze Sekunden abgerundet. Änderungen unter 5 % gegenüber dem zuletzt gespeicherten Faktor bleiben im RAM und gehen in den nächsten Lernzyklus ein. Erst ab 5 % Unterschied wird der Faktor im Profil gespeichert und mit dem Dashboard synchronisiert. Der laufende Lernzyklus liegt ausschließlich im RAM; nach einem Neustart beginnt er erst mit frischen Akkumeldungen und automatisch eingeschaltetem Ausgang erneut.

Der Laptop erteilt die Lernfreigabe mit `Status.ChargeLearningAllowed` (boolesch) und meldet `Status.ChargeBootCounter` als überlauffähigen `uint32`-Startzähler einschließlich Null. Die Lernmessung darf bei Rückkehr nur ausgewertet werden, wenn die Zählerdifferenz modulo `2^32` genau 1 beträgt. Zusätzliche Starts oder eine fehlende/ungültige Freigabe verwerfen die Messung; der normale Nachlauf bleibt erhalten. Der Lernschalter erhöht den Zähler nicht. Installation, Bedienung und Besonderheiten bei externem Laden stehen unter [LaptopBattery](../LaptopBattery/README.DE.md).

Die JSON-Felder heißen `ChargeRunOn` und `ChargeSecondsPerPercent`. Zusätzliche Profilfelder sind erlaubt und bleiben beim Bearbeiten und Synchronisieren erhalten. Bei `ChargeRunOn: true` ist ein positiver ganzzahliger Faktor (`uint32_t`) Pflicht; ohne Aktivierung müssen diese Felder nicht vorhanden sein.

## Statusmeldungen

Regelmäßige UDP-Statusmeldungen erfolgen alle zehn Sekunden. Ein-/Ausgangszustände, Übersteuerungen, Fehler und Änderungen der Messwertgültigkeit werden sofort gemeldet, ebenso Start und Wiederverbindung. Reine Messwertänderungen lösen keine zusätzlichen Broadcasts aus.
Spannung, Strom und Wirkleistung werden gemeinsam aus dem letzten vollständigen Messzyklus gesendet. Vorläufige Ergebnisse überschreiben keine einzelnen Felder des freigegebenen Satzes. Diagnosewerte der Leistungsmessung bleiben gezielt über `/api/v1/status` abrufbar; UDP-Broadcasts enthalten sie nicht.

## PI-Beobachtungsbetrieb

Heizprofile können ein zusätzliches `Pi`-Objekt mit `Mode: "Shadow"` enthalten. Die bisherige Hystereseregelung schaltet unverändert weiter. Der PI-Regler berechnet alle zehn Sekunden einen Gesamtleistungsbedarf und teilt ihn zuerst der Bodenheizung zu; erst der Rest entfällt auf die Hauptheizung. Die gemeldeten Schaltanforderungen sind hypothetisch und werden nicht an Relais oder andere Geräte übertragen. `Mode: "Off"` bzw. ein fehlendes `Pi`-Objekt deaktiviert die Beobachtung. Ein aktiver PI-Schaltmodus wird noch nicht angenommen oder akzeptiert.

Der Integralanteil ist begrenzt, wird bei Sättigung nicht weiter in die gleiche Richtung aufintegriert ausschließlich aus der Temperaturabweichung mit Ki = Kp / Ti fortgeschrieben. Nicht überbrückbare Messwertlücken, manuelle Übersteuerung und ausgeschaltete Zeitfreigabe unterbrechen das Lernen. Ein fehlender Bodenstatus nimmt dessen Leistung aus der Aufteilung und sperrt das Lernen; fehlende Raumtemperatur stoppt nur Modelllernen/Vorsteuerung, nicht die PI-Berechnung.

Das langsame Modelllernen verwendet reale gemessene Hauptleistung und die gemessene konstante Bodenleistung bei bestätigtem EIN. Gültige 0 W bleiben 0 W. Einstündige durchgehend gültige Fenster passen den effektiven Wärmeverlust bei nahezu gleicher Start-/Endtemperatur schwach an; ausreichend große Temperaturänderung (mindestens 0,25 K) ermöglicht eine begrenzte Wärmekapazitäts-/Verstärkungsanpassung. Pro akzeptiertem Fenster werden nur 5 % der Schätzung übernommen, plausible Schätzungen müssen innerhalb des halben bis doppelten konfigurierten Startwerts liegen. Die konfigurierte Verzögerung verschiebt die Energiebasis für das Modelllernen. Lernwerte bleiben im Beobachtungsbetrieb im RAM und werden als Telemetrie aufgezeichnet; Neustart oder geänderte Konfiguration beginnt mit den Profil-Startwerten. Ti bleibt der konfigurierte langsame Startwert. Das ist kein Nachweis einer bereits eingeregelten Anlage.

`HeatPi` im HTTP-/UDP-Status enthält Bedarf, P-/I-/Vorsteuerungsanteil, tatsächliche Leistung, hypothetische Boden-/Hauptanforderung, Modellparameter und Lernstatus. Beobachtungswerte verursachen keine zusätzlichen UDP-Sendungen. Der Bodenpeer wird über seine feste MAC@Service-ID und den aktuellen Kanalnamen geprüft; nach Kanal-/Gerätetausch ist diese explizite Beobachtungszuordnung anzupassen.

## PI-Messwertlücken

Bei fehlenden oder ungültigen Messwerten verwendet der PI-Regler die letzten
gültigen Temperatur-, Leistungs- und Raumtemperaturwerte bis zu einem Alter von
60 Sekunden. Die PWM läuft weiter; Integralfortschreibung und Lernen laufen mit den überbrückten Werten weiter.
Die Diagnose nennt diesen Zustand `measurement-held`; die Werte sind keine
frischen Messungen. Ohne vorherigen gültigen Wert oder nach Ablauf greift der
konfigurierte Rückfall. Die letzte gültige Hauptanforderung für die Bodenheizung
gilt ebenfalls 60 Sekunden; eine ausdrücklich empfangene AUS-Anforderung wirkt
sofort. Neustart oder Profilwechsel verwirft den Messwertcache.

Bei `measurement-held` nutzt das Lernen bis zur 60-s-Grenze angenäherte Energie aus den letzten gültigen Leistungswerten. Diese Abschnitte sind über den Diagnosegrund erkennbar und keine zusätzlichen echten Messungen.

Die aktive PI-Schaltung verwendet ein festes Energiebudget je PWM-Periode. Hauptenergie wird aus gemessener Leistung integriert, Bodenenergie aus gemessener konstanter Bodenleistung und gemeldetem Schaltzustand. Fehlende Energie wird mit Bodenpriorität in die nächste Periode übertragen; Überlieferung bis zum nächsten Abtastpunkt verringert das Folgebudget. Manuelle Eingriffe, ungültige Messwerte nach Ablauf der Toleranz, Konfigurationsänderungen und Beobachtungslücken verwerfen das laufende Budget. Budget und Vortrag liegen nur im RAM; Mess- und Rückmeldetakt begrenzen die Abschaltgenauigkeit.
