# HeatHistory

[English](README.md)

Dokumentversion: 1.32
Stand: 2026-11-02

HeatHistory läuft auf einem PC oder Raspberry Pi. Es beobachtet die Aquarium-
Heizungsregelung passiv und speichert pro Profil CSV-Verläufe auf dem jeweiligen Rechner.
Konfigurationen werden nicht verändert.

## Profil- und Statusprüfung

HeatHistory empfängt Profile, Kanal- und Gerätezuordnungen zentral vom Dashboard
über SSE `/api/recording-events`. Nur der separate einmalige Diagnoseabruf nutzt
`/api/heatprofiles`, `/api/times` und `/api/device-registry`. `TempId` benennt die logische Temperaturquelle. Die jeweils eindeutigen Kanäle für `HeaterFloor` und `HeaterMain` werden anhand ihrer Profilzuordnung über alle Geräte gesucht. Die physische Temperaturquelle folgt bei Sensor-Tausch der Endpunkt-Zuordnung.

Temperatur und Schaltzustände liest HeatHistory direkt aus den UDP-Statuspaketen
der Geräte auf Port 44444. Es fragt dafür keine Geräte-Webseiten oder
`/api/v1/status`-Endpunkte ab. Jedes Paket enthält Geräte-ID und Status; der
Feeder ergänzt die Temperatur in Celsius. Standardmäßig wartet HeatHistory bis
zu acht Sekunden auf aktuelle Pakete und akzeptiert Statusdaten bis zu 15
Sekunden Alter. Diese Grenzen und die Dashboard-Adresse stehen in
`config.json`. Der aufzeichnende Rechner muss sich im selben Broadcast-Netz wie die Geräte befinden;
eine lokale Firewall muss eingehenden UDP-Verkehr auf Port 44444 zulassen.

Fehlen zentrale Profile/Kanalzuordnungen, ein benötigtes UDP-Paket oder eine
gültige Temperatur bzw. ein Schaltzustand, pausiert HeatHistory nur das
betroffene Aquarium und meldet den Fehler mit UTC-Zeitstempel. Nach
erfolgreicher Prüfung wird das Logging automatisch fortgesetzt. HTTP-Fehler
beim Dashboardabruf zeigen zusätzlich die Dauer des fehlgeschlagenen Requests.

Reine JSON-Formatierungsänderungen gelten nicht als Profiländerung. Ändert sich
der semantische Profilinhalt, legt HeatHistory einen Snapshot und eine neue CSV
an. Die erste Datenzeile des neuen Logs enthält den zum Startzeitpunkt aktuellen
Zustand.

CSV-Header und -Reihenfolge sind fest:

```text
Time;Temp;Floor;Main
```

Es wird ein Datensatz geschrieben, wenn sich mindestens Temperatur, Floor-
oder Main-Status gegenüber dem vorherigen Datensatz geändert hat.

## Ereignisgesteuerte Aufzeichnung

Beim Start öffnet HeatHistory die UDP-Sockets und eine SSE-Verbindung zum
Dashboard (`/api/recording-events`, DashboardServer ab 1.46). Das Dashboard sendet
zuerst einen vollständigen Konfigurationsstand, danach nur geänderte oder entfernte
Profile/Kanalgruppen und bei Bedarf die Gerätezuordnung. Es gibt keine regelmäßige
HTTP-Abfrage der Profile oder Zuordnungen. Die Verbindung enthält gelegentliche
Keepalive-Zeilen, keine periodischen Konfigurationskopien.

`Run()` wartet blockierend auf UDP- oder Dashboard-Ereignisse. `OnDashboardEvent()`
verteilt Änderungen an `OnProfileChange()`, `OnChannelChange()` und
`OnRegistryChange()`. `ApplyConfiguration()` aktualisiert daraus die Zuordnung.
`UdpDescriptions.OnUdpPacket()` dekodiert und prüft UDP-Pakete zentral und verteilt
sie an `OnInfoPacket()` oder `OnStatusPacket()`. Vollständige Statusdaten gelangen
über den `OnStatusPacket()`-Handler zur Aufzeichnung. Nachgelieferte Beschreibungen
behalten die ursprünglichen Empfangszeitpunkte der Messdaten bei.
CSV-Dateien bleiben offen; vollständige Zeilen werden für gleichzeitige Browser-
Leser geflusht. Ein Abschnittswechsel schließt nur die betroffenen Schreiber.
`Start()` und `Close()` bündeln Aufbau und Beenden.

Bei unterbrochener Dashboard-Verbindung läuft die Aufnahme mit dem letzten
vollständigen gültigen Stand weiter. Nach Wiederverbindung folgt ein vollständiger
Snapshot, damit zwischenzeitliche Änderungen nicht verloren gehen. Ohne ersten
gültigen Stand beginnt keine Aufnahme. Eine laufende Geräte-Neuzuordnung wird wie
bisher bis zum Abschluss nicht als gültige Konfiguration übernommen.
Die Bereinigung läuft einmal beim Dienststart, durch den geplanten Pi-Neustart
also täglich. Ein zusätzlicher Bereinigungstimer entfällt.
Alte `PollIntervalSeconds`-Einträge werden nicht mehr verwendet.

## Aufbau des Datenverzeichnisses

Unter `~/.local/share/HeatHistory/data/` besitzt jedes Aquarium sein eigenes
Profilverzeichnis, beispielsweise `HeatProfileAq1/`:

| Ort / Dateityp | Inhalt |
|---|---|
| `HeatProfileAqX/*.csv` | Temperatur und gemeldete Schaltzustände von Floor/Main. Eine Zeile entsteht bei einer Änderung dieser Werte. |
| Gleichnamige `*.json` | Snapshot des zu dieser CSV gehörenden Heizprofils, damit spätere Profiländerungen die historischen Daten nicht umdeuten. |
| `state.json` | Aktuelle Dateizuordnung, Systemstartkennung und letzter aufgezeichneter Zustand zur Fortsetzung der Temperatur-/Schaltaufzeichnung. |
| `Energy/*.energy.csv` | Leistungsmeldungen, Wasser-/Raumtemperatur, Schaltzustände, integrierte Energie und Angaben zu gültigen, gehaltenen, interpolierten oder unbekannten Abschnitten. |
| `Energy/*.energy.json` | Geräte-/Kanalzuordnung, Profil, feste Bodenleistung, Faktoren und Integrationsbeschreibung für den betreffenden Energieabschnitt. |
| `Pi/*.pi.csv` | Diagnosewerte des PI-Reglers: Temperatur, Temperaturabweichung, P-/I-/Wärmeverlustanteil, Leistungsbedarf, Schaltanforderungen und Lernstatus. `Pi` meint hier den PI-Regler. |
| `Pi/*.pi.json` | Zugehörige Reglerkonfiguration einschließlich Sollwert, Betriebsart, Geräteidentität und Bootkennungen. |

Die **Messreihen bleiben CSV**. Die JSON-Dateien speichern ergänzende Metadaten
bzw. den Fortsetzungszustand; sie sind keine zweite Kopie der Messreihe. Sie werden
für die historische Zuordnung und das Wiederaufnehmen benötigt und sollten mit
ihrer CSV erhalten bleiben.

Die UTC-Zeitstempel im Dateinamen kennzeichnen den Abschnittsbeginn. Neue
Abschnitte entstehen je nach Datenart beim Systemstart des Aufzeichnungsrechners,
bei Profil-/Zuordnungsänderungen oder beim Neustart des Reglers. Ein reiner
HeatHistory-Dienstneustart setzt kompatible Dateien desselben Systemstarts fort.

## Diagramme im Browser

Die Webansicht liefert eine feste HTML-/JavaScript-Seite. Der Browser lädt die
Messreihen über `/api/history.csv?range=1day` als komprimierbares CSV-Paket mit kleinem Metadatenmanifest und erstellt die
Diagramme mit Plotly. Der Server erzeugt beim Reload keine Diagrammseite mit
allen eingebetteten Messreihen.

Beim Neuladen ist **1 day** ausgewählt. **1 h**, **6 h**, **7 days** und **All**
fordern den jeweiligen Zeitraum für beide Aquarien gemeinsam an. **All** zeigt
nur den noch verfügbaren Verlauf: ab Beginn des aktiven PI-Betriebs und innerhalb
der eingestellten maximalen Anzeigespanne, standardmäßig 30 Tage. `ViewFromUTC`
kann den Beginn ausdrücklich festlegen; ohne diesen Wert wird die erste aktive
PI-Aufzeichnung bzw. ein aktiver Profil-Snapshot der Energieaufzeichnung benutzt.
Frühere PC-Daten bleiben unverändert erhalten und gehören nicht zur Webansicht.

Wasser, Raum und historischer Sollwert teilen eine Temperaturachse. Darunter
folgen Floor, Main, Power, Energy und bei vorhandener PI-Diagnose PI demand und
PI components. Alle Zeilen eines Aquariums teilen die Zeitachse. Energy summiert
die aufgezeichneten Energieintervalle und Korrekturen **im ausgewählten Zeitraum**;
kurze überbrückte Abschnitte bleiben als Schätzung gekennzeichnet. Floor nutzt die gemessene feste Leistung und
den Schaltzustand. Main verwendet den korrigierten Endwert ohne erneuten Faktor.
Die Sollwertlinie bleibt auch ohne temporäre PI-Diagnose über die gespeicherten
Energie-Profil-Snapshots verfügbar.

Die Aufzeichnung bleibt CSV. Der Pi wählt anhand der Zeitstempel die benötigten
CSV-Datensätze und überträgt die Zellen unverändert. Der Browser verarbeitet
Zahlen, Gültigkeit, Messlücken, Energieintervalle und Sollwerte. Das kleine
JSON-Manifest enthält nur Zeitraum, Profil-/Segmentzuordnung und Sollwerte;
es gibt keine serverseitige JSON-Konvertierung jeder Messwertzelle. Plotly.js wird
weiterhin vom CDN geladen. Die Desktop-Legende steht rechts, bei schmaler
Anzeige unterhalb des Diagramms. `/api/history.csv` erlaubt ausschließlich bekannte
Zeitraumwerte; Konfigurationen und beliebige Dateien werden nicht ausgeliefert.

Der optionale Offline-Export `python3 CreateCharts.py` bleibt verfügbar und ist
unabhängig von der laufenden Webansicht. Er kann auch ältere PC-Sicherungsdaten
für eine ausdrücklich angeforderte Auswertung lesen.

Die CSV-Dateien werden je Profil und Aufzeichnungsart nach dem UTC-Zeitstempel
im Dateinamen sortiert. Der nächste Dateiname begrenzt das vorherige Segment;
unpassende Dateien werden vor dem Öffnen ausgeschlossen. Start überspringt
Datensätze vor dem Zeitfenster, Mid überträgt vollständige Datensätze ohne
Zeitprüfung jeder Zeile, End stoppt hinter dem Fensterende. Voraussetzung sind
zeitlich aufeinanderfolgende, nicht überlappende Segmente. Falls Dateinamen
angepasst werden müssen, geschieht dies in einem separaten Vorbereitungsschritt.
Eine feste Dateigröße je Abfrage schließt unvollständige Schreibzeilen und später
angehängte Daten aus.

Die HTTP-Antwort wird in begrenzten Blöcken direkt übertragen: bei akzeptiertem
`gzip` durch GZIP Stufe 3, sonst unkomprimiert. Die vollständige Antwort wird nicht
im RAM zusammengesetzt. CSV-Zitate und mehrzeilige Datensätze bleiben erhalten;
Messwertkonvertierung und Diagramme übernimmt weiterhin der Browser.

## Umzug vom PC auf den Raspberry Pi

Voraussetzung: SSH-Zugang als `pi` zu `192.168.99.51`, Python 3.10 oder neuer,
freier Port 8091 und noch keine HeatHistory-Installation im Pi-Zielverzeichnis.
Im PC-Terminal aus diesem Verzeichnis aufrufen:

```bash
./MoveToRasPi.sh --allow-log-expiry
```

Der Aufruf benötigt zuerst das lokale sudo-Passwort und anschließend über SSH
gegebenenfalls das sudo-Passwort des Pi. Er stoppt die PC-Aufzeichnung, exportiert
vollständige Datensätze ab Beginn der aktiven PI-Regelung (höchstens 30 Tage),
überträgt Quellen und Export und installiert den Pi-Dienst. Erst nachdem beide
Aquarien frische Energie-Datensätze liefern, wird der PC-Dienst deaktiviert.
Bei einem Fehler wird die PC-Aufzeichnung wieder gestartet. Die bisherigen
PC-Daten bleiben unverändert unter `~/.local/share/HeatHistory/data/` erhalten.
Der exportierte Stand und Zeitpunkte stehen zusätzlich unter
`generated/PiMigration-<UTC>/`.

Auf dem Pi liegen Programm, Konfiguration und Daten unter
`/home/pi/.local/share/HeatHistory/`; der Dienst heißt `HeatHistory.service`.
Browser: <http://192.168.99.51:8091/>. Die Diagramme berechnet weiterhin der Browser.
Spätere Softwareupdates werden im Pi-Quellverzeichnis mit `./Install.sh`
installiert; vorhandene Konfiguration und Daten bleiben erhalten.

**`--allow-log-expiry` erlaubt ausdrücklich das endgültige Entfernen eigener,
abgeschlossener Pi-Logdateien** nach 30 Tagen oder zur Einhaltung von 512 MiB
verwalteter Messdaten. Zuerst werden abgelaufene geschlossene Abschnitte entfernt.
Danach ermittelt `du -sb` die verbleibende Größe; bei Überschreitung werden weitere
Abschnitte vom ältesten an entfernt. CSV und zugehörige JSON bleiben ein Paar.
Aktive Dateien und `state.json` bleiben geschützt. Voraussetzung: Im Datenpfad
liegen ausschließlich eigene Aufzeichnungen, keine Fremddateien oder Symlinks.
Die tägliche Rotation hält alte Abschnitte geschlossen. Die Freigabe
steht als `StorageDeleteExpired: true` in der Pi-Konfiguration. Ohne Freigabe
entfernt HeatHistory keine bestehenden Logs. Die PC-Sicherung ist davon unabhängig.
Temperatur-, Energie- und PI-Dateien wechseln beim Systemstart (normalerweise
nach dem täglichen Neustart um 00:01 Uhr Ortszeit) und bei relevanten Profil-/
Geräteänderungen. Es gibt keinen zusätzlichen Wechsel um UTC-Mitternacht. Ein
HeatHistory-Dienstneustart setzt die letzten passenden Dateien und Energiesummen
desselben Systemstarts fort, ohne über die Aufzeichnungslücke zu integrieren.
Dateinamen und Messzeitstempel bleiben UTC.
Ist bereits der aktive Bestand größer als die Grenze, stoppt die Aufzeichnung
mit einer Fehlermeldung, statt aktive Daten zu entfernen.

## Installation als PC-Systemdienst

Am PC aus dem Projekt aufrufen:

```bash
./Install.sh
```

Der Installer benötigt sudo und installiert nach `~/.local/share/HeatHistory`
auf die interne PC-Platte. Vorhandene Konfiguration und Daten dort bleiben
unverändert. Die Python-Module und Versions.txt werden aktualisiert.
Unter Prjs bleiben Quellen, Install.sh, Dokumentation und Tests; dort werden
keine Laufzeitdaten gehalten. CreateCharts.py wird vom gemeinsamen Programm
importiert und ist daher weiterhin erforderlich.

Der systemweite
`HeatHistory.service` startet beim Booten ohne Anmeldung, läuft jedoch unter
dem aufrufenden Benutzer. Programm und Daten benötigen die Projekt-SSD nicht.
Bei späteren Quelländerungen Install.sh erneut ausführen.

```bash
systemctl status HeatHistory.service
journalctl -u HeatHistory.service -f
sudo systemctl restart HeatHistory.service
```

Browser: http://localhost:8091/ (Tablet: http://<PC-IP>:8091/).
Konfiguration: `~/.local/share/HeatHistory/config.json`.
CSV, Profil-Snapshots und Zustandsdateien:
`~/.local/share/HeatHistory/data/` bei der Standardkonfiguration.
HeatHistory zeichnet nur HeatProfileAq<number> mit Measurement Temperature auf;
Batterieprofile wie LaptopCharge gehören nicht zu den Aquarium-Heizverläufen.

## Start

Voraussetzung ist Python 3.10 oder neuer. HeatHistory verwendet ausschließlich
die Python-Standardbibliothek.

Vom Repository-Stamm:

```bash
cd HeatHistory
cp config.example.json config.json
python3 HeatHistory.py
```

In `config.json` können Dashboard-Adresse, Pollintervall, UDP-Warte- und
Gültigkeitsdauer sowie Ausgabeverzeichnis angepasst werden. Relative Pfade
beziehen sich auf das HeatHistory-Projektverzeichnis. `Ctrl+C` beendet den Logger.

Standardmäßig landen Logs und Profil-Snapshots unter `data/<Profil-ID>/`.
Dieser Laufzeitbestand gehört zum PC und wird nicht in Firmware oder
Geräte-Dateisysteme geschrieben.

## Aktueller Umfang

- Interaktiver CSV-Diagrammbericht mit synchronisiertem Zoom: `python3 CreateCharts.py`
- Dauerbetrieb mit konfigurierbarem Pollintervall
- Per-Profil Fehlerzustand und automatische Wiederaufnahme
- Semantische Profilrevisionen und Startzeile im jeweils neuen Log
- Keine Profil-Synchronisation

## MAC-Zuordnung

HeatHistory löst über MAC-Identitäten die benötigten Funktionen über die zentrale
Geräteregistry auf physische UDP-IDs auf. Profile/CSV-Schlüssel bleiben funktional.
Ausstehende Tausche oder fehlende Zuordnungen pausieren das Logging. Geräte-Webseiten werden nicht abgefragt.

UDP-Empfang: Ausschließlich Version 2 liest das im Profil mit
`Measurement` gewählte numerische Statusfeld (Standard: `Temperature`). Fehlende,
ungültige oder veraltete Werte pausieren die Aufzeichnung. Ein explizites Standardfeld
ändert die Profil-Signatur nicht; eine andere Messwertauswahl schon.

Externe Quellen aus der Dashboard-Registry werden ebenfalls aufgelöst. Eine
fehlende Einzelquelle blockiert nur das betroffene Profil. Das CSV-Format bleibt
auf Aquarium-Heizprofile `HeatProfileAq<number>` mit Boden-/Hauptheizung ausgelegt;
`LaptopCharge` wird damit nicht als Aquarium-Verlauf aufgezeichnet.

## Leistung, Energie und Raumtemperatur

Zusätzlich zu den unveränderten Temperatur-/Schalt-CSV schreibt HeatHistory pro Profil Dateien unter `Energy/*.energy.csv`. Diese enthalten die gemeldete Hauptleistung, einen gesonderten Korrekturfaktor, die korrigierte Leistung, Geräte-ID, Gerätezeit, Bootzähler, Wasser- und Raumtemperatur sowie integrierte Wh und Angaben zur Messabdeckung. Jede tatsächlich empfangene Haupt-/Bodenstatusmeldung wird verarbeitet; zwischengespeicherte Daten werden nicht erneut als neue Leistungsmessung gezählt.

Die Hauptenergie wird zwischen benachbarten gültigen Leistungsmeldungen trapezförmig integriert. Das ist eine Energieabschätzung aus Wattwerten, kein ausgelesener Hardware-Energiezähler: Schalten innerhalb der Sendeperiode und die Lage der Messfenster können nicht exakt rekonstruiert werden. Gültige 0 W zählen als null Energie. Fehlende oder ungültige Leistungswerte werden für höchstens 60 Sekunden mit dem letzten gültigen Wert provisorisch weitergezählt (`held`). Trifft innerhalb dieser Frist ein gültiger Endpunkt ein, wird der Abschnitt linear interpoliert und die Energiedifferenz als signierte Korrektur nachgetragen (`interpolated`). Daher kann eine einzelne Intervallenergie negativ sein; die Gesamtsumme enthält die Berichtigung der zuvor geschätzten Energie. Neustarts und Zeitdiskontinuitäten werden nicht überbrückt, der Teil einer Lücke jenseits von 60 Sekunden bleibt unbekannt. Rohmesswerte bleiben unverändert. Fehlende Temperatur allein unterbricht gültige Wattmessungen nicht. Die Bodenenergie wird aus gemeldetem Schaltzustand und eingetragener Nennleistung geschätzt.

Optionale Konfiguration:

```json
{
  "FloorPowerWatts": {"HeatProfileAq1": 60, "HeatProfileAq2": 40},
  "RoomTemperatureSource": "GN-Pi",
  "PowerFactors": {},
  "EnergyMaxIntervalSeconds": 15
}
```

`PowerFactors` ordnet einer physischen `MAC@ServiceId` einen positiven Faktor zu. Die gemeldeten Wattwerte bleiben unverändert gespeichert; Faktor und korrigierte Werte stehen separat daneben. Die Kalibrierung ist zunächst als vorläufig gekennzeichnet. Ein beim Start gesicherter Kalibrierdatensatz wird in den Sitzungsmetadaten erhalten. Nach einer späteren Gerätekalibrierung müssen Faktoren zeitlich dem richtigen Abschnitt zugeordnet werden: frühere unkalibrierte Werte korrigieren, bereits neu kalibrierte Werte nicht nochmals mit demselben Faktor multiplizieren.

Die Raumtemperatur kommt aus dem `RoomTemperature`-Messwert des zugeordneten Pi-Status. Fehlende Raumtemperatur wird als unbekannt protokolliert und stoppt die übrige Aufzeichnung nicht. Die Browseransicht ergänzt Diagramme für Raumtemperatur, gemeldete/korrigierte Hauptleistung, geschätzte Bodenleistung und Energie.

### Neue Messreihe mit getrenntem Altarchiv

```bash
./Install.sh --new-series
```

Der Installer stoppt den Dienst, verschiebt den bisherigen `data/`-Bestand unverändert nach `~/.local/share/HeatHistory/archive/<UTC-Zeitstempel>/data/`, sichert die bisherige Konfiguration und startet mit leerem `data/` neu. Die Webansicht zeigt dadurch nur die neue Reihe. Ohne `--new-series` werden bestehende Aufzeichnungen fortgesetzt. Archivierte Daten lassen sich weiterhin ausdrücklich mit `CreateCharts.py --data-dir <Archivpfad>` ansehen.

24 Stunden sind ein erster Auswertezeitraum, keine automatische Abschaltgrenze: der Dienst zeichnet weiter auf. Für PI-Startwerte sind ausreichend echte Heizzyklen, dokumentierte Kalibrierfaktoren und gute Messabdeckung erforderlich.

Die dargestellten Energiesummen werden über alle Sitzungen des Aquariums im aktiven Datenverzeichnis fortgeführt. Bei einem Dienstneustart beginnen sie in der Grafik daher nicht erneut bei null; unbekannte Energie während Aufzeichnungslücken wird nicht ergänzt.

Bei bis zu 900 Pixel Fensterbreite steht die Legende unter dem Diagramm, darüber rechts. Beim Drehen des Tablets passt sie sich automatisch an.

## PI-Beobachtungsbetrieb

Heizprofile können ein zusätzliches `Pi`-Objekt mit `Mode: "Shadow"` enthalten. Die bisherige Hystereseregelung schaltet unverändert weiter. Der PI-Regler berechnet alle zehn Sekunden einen Gesamtleistungsbedarf und teilt ihn zuerst der Bodenheizung zu; erst der Rest entfällt auf die Hauptheizung. Die gemeldeten Schaltanforderungen sind hypothetisch und werden nicht an Relais oder andere Geräte übertragen. `Mode: "Off"` bzw. ein fehlendes `Pi`-Objekt deaktiviert die Beobachtung. Aktiver Betrieb wird ausdrücklich über Mode Active gewählt.

Der Integralanteil ist begrenzt, wird bei Sättigung nicht weiter in die gleiche Richtung aufintegriert ausschließlich aus der Temperaturabweichung mit Ki = Kp / Ti fortgeschrieben. Nicht überbrückbare Messwertlücken, manuelle Übersteuerung und ausgeschaltete Zeitfreigabe unterbrechen das Lernen. Ein fehlender Bodenstatus nimmt dessen Leistung aus der Aufteilung und sperrt das Lernen; fehlende Raumtemperatur stoppt nur Modelllernen/Vorsteuerung, nicht die PI-Berechnung.

Das langsame Modelllernen verwendet reale gemessene Hauptleistung und die gemessene konstante Bodenleistung bei bestätigtem EIN. Gültige 0 W bleiben 0 W. Einstündige durchgehend gültige Fenster passen den effektiven Wärmeverlust bei nahezu gleicher Start-/Endtemperatur schwach an; ausreichend große Temperaturänderung (mindestens 0,25 K) ermöglicht eine begrenzte Wärmekapazitäts-/Verstärkungsanpassung. Pro akzeptiertem Fenster werden nur 5 % der Schätzung übernommen, plausible Schätzungen müssen innerhalb des halben bis doppelten konfigurierten Startwerts liegen. Die konfigurierte Verzögerung verschiebt die Energiebasis für das Modelllernen. Lernwerte bleiben im Beobachtungsbetrieb im RAM und werden als Telemetrie aufgezeichnet; Neustart oder geänderte Konfiguration beginnt mit den Profil-Startwerten. Ti bleibt der konfigurierte langsame Startwert. Das ist kein Nachweis einer bereits eingeregelten Anlage.

`HeatPi` im HTTP-/UDP-Status enthält Bedarf, P-/I-/Vorsteuerungsanteil, tatsächliche Leistung, hypothetische Boden-/Hauptanforderung, Modellparameter und Lernstatus. Beobachtungswerte verursachen keine zusätzlichen UDP-Sendungen. Der Bodenpeer wird über seine feste MAC@Service-ID und den aktuellen Kanalnamen geprüft; nach Kanal-/Gerätetausch ist diese explizite Beobachtungszuordnung anzupassen.

HeatHistory schreibt pro Aquarium unter `Pi/*.pi.csv` eine separate Beobachtungsreihe und JSON-Konfigurationsmetadaten. Nur Telemetrie des tatsächlich zugeordneten Hauptgeräts wird angenommen; unveränderte PI-Samplezähler werden nicht doppelt geloggt. Energie-/Temperatur-CSV bleiben im bestehenden Format. Die Diagramme ergänzen zwei zeitlich gekoppelte Zeilen für Bedarf gegen reale Leistung sowie P/I/Vorsteuerung. Fehlende PI-Daten oder Neustarts werden nicht überbrückt. `./Install.sh` installiert den neuen Logger ohne Zurücksetzen der Messreihe; kein `--new-series` verwenden.

Kompakte UDP-Meldungen werden anhand gezielt angeforderter Beschreibungen ergänzt. Beschreibungsantworten zählen nicht als neue Messwerte; [Protokoll](../Libs/GnAqPrjsLib/UdpStatus.DE.md).

### Aktiver PI-Betrieb

`Pi.Mode: "Active"` schaltet die Hauptheizung nach dem PI-Bedarf. Zusätzlich ist `MainDevice` (physische MAC mit `@4`) erforderlich. Der Hauptschalter meldet die Bodenanforderung über seinen regulären UDP-Status. Der zugeordnete Bodenkanal akzeptiert nur die passende Geräte-, Profil- und Kanalbindung. Nach 60 Sekunden ohne gültige Anforderung schaltet der Bodenkanal EIN; Zeitpläne und manuelle Übersteuerungen behalten Vorrang. Fehlt umgekehrt der Bodenstatus, wird der Bedarf allein der Hauptheizung zugeordnet. Fehlende oder ungültige Temperatur-/Leistungsdaten werden bis zu einem Alter der letzten gültigen Stichprobe von 60 Sekunden überbrückt (`measurement-held`); Integralfortschreibung und Lernen laufen mit den überbrückten Werten weiter. Ohne vorherigen gültigen Wert oder nach Ablauf verwendet die Hauptheizung `NoTempOn`. Die Hardwarethermostate bleiben unverändert.

HeatHistory zeichnet `Active` und `Shadow` in getrennten PI-Dateien auf. Die Modellanpassungen bleiben im RAM des Hauptschalters; nach einem Neustart gelten wieder die Profilwerte.

Die Diagrammbeschriftung ist Englisch. Water und Room teilen eine Temperaturzeile. Power zeigt Main (gemessene Leistung) und Floor (gemessene feste Bodenleistung × Schaltzustand). Energy zeigt die integrierten Wh von Main und Floor. PI demand enthält Total und Act (aktuell eingespeiste Gesamtleistung). PI components zeigt Proportional, Integral und Heat-loss estimate; eine Erklärung lässt sich oberhalb aufklappen.

Eine rot gestrichelte **Setpoint**-Linie zeigt im Temperaturdiagramm den zeitlich gültigen PI-Sollwert. Änderungen erscheinen als Stufen. Verwendet werden die gespeicherten Konfigurationen der jeweiligen PI-Messreihe; Zeiträume ohne belegten Sollwert bleiben leer.

Für die Hauptrolle akzeptiert HeatHistory sowohl `MainSw` als auch `HeaterMain`. Die Rollenbezeichnung verändert weder die Zuordnung noch die Energieberechnung.

Bei `measurement-held` nutzt das Lernen bis zur 60-s-Grenze angenäherte Energie aus den letzten gültigen Leistungswerten. Diese Abschnitte sind über den Diagnosegrund erkennbar und keine zusätzlichen echten Messungen.

Der bisherige Endpunkt `/api/history` bleibt für vorhandene Diagnose-/Umzugswerkzeuge kompatibel, wird von der Diagrammseite aber nicht verwendet. Ein SQLite-Vergleich erfolgt isoliert mit Kopien; er ändert die laufende CSV-Aufzeichnung nicht.

Fehlende und vom Feeder verworfene Temperaturwerte werden im Diagramm ausgelassen;
die Linie verbindet die gültigen Punkte. Diese Verbindung erzeugt keine neuen
Messwerte. Der Feeder verwirft Sprünge über 2 °C gegenüber dem letzten gültigen
Wert. Ein neues Niveau wird nach zwei aufeinanderfolgenden, untereinander höchstens
2 °C abweichenden Messungen übernommen; ein dazwischenliegender Lesefehler verwirft
die Bestätigung. Der DS18B20-Resetwert 85 °C wird weiterhin immer verworfen.
