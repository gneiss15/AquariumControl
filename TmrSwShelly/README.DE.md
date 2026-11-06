<!-- README.DE.md, Version: 1.56 -->
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

Relais, Taster und die GPIO-Vorbereitung der RGB-LED verwenden die gemeinsame
GnBasics-Schnittstelle. Vor dem Aktivieren des Relais wird der Ausgangspegel für
AUS gesetzt; der Relaisstatus gilt erst nach erfolgreicher Initialisierung als
bekannt. Die LED-Helligkeit wird über LEDC gesteuert.

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
