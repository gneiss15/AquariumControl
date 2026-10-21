# HeatHistory

[English](README.md)

Dokumentversion: 1.19
Stand: 2026-10-21

HeatHistory ist ein eigenständiges PC-Projekt. Es beobachtet die Aquarium-
Heizungsregelung passiv und speichert pro Profil CSV-Verläufe auf dem PC.
Konfigurationen werden nicht verändert.

## Profil- und Statusprüfung

HeatHistory liest Profile und Kanalzuordnungen zentral vom Dashboard über
`/api/heatprofiles`, `/api/times` und `/api/device-registry`. `TempId` benennt die logische Temperaturquelle. Die jeweils eindeutigen Kanäle für `HeaterFloor` und `HeaterMain` werden anhand ihrer Profilzuordnung über alle Geräte gesucht. Die physische Temperaturquelle folgt bei Sensor-Tausch der Endpunkt-Zuordnung.

Temperatur und Schaltzustände liest HeatHistory direkt aus den UDP-Statuspaketen
der Geräte auf Port 44444. Es fragt dafür keine Geräte-Webseiten oder
`/api/v1/status`-Endpunkte ab. Jedes Paket enthält Geräte-ID und Status; der
Feeder ergänzt die Temperatur in Celsius. Standardmäßig wartet HeatHistory bis
zu acht Sekunden auf aktuelle Pakete und akzeptiert Statusdaten bis zu 15
Sekunden Alter. Diese Grenzen und die Dashboard-Adresse stehen in
`config.json`. Der PC muss sich im selben Broadcast-Netz wie die Geräte befinden;
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

## Diagramme erzeugen

Der Diagrammgenerator liest alle CSV-Logs unter `data/`, parst die ISO-
Zeitstempel direkt als UTC und erstellt pro Log ein gemeinsames Diagramm mit
Temperatur, `Floor` und `Main`. Temperatur erhält den großen oberen Bereich;
die binären Ausgänge stehen darunter jeweils in einem eigenen kompakten
Bereich. Alle drei Bereiche verwenden dieselbe Zeitachse und besitzen je eine
linke Achse. Die Zustandsachsen zeigen `Aus` und `Ein`. Zoomen ist per Mausrad
oder Auswahlrahmen möglich; Schwenken und Schnellwahl (1/6 Stunden, 1/7 Tage,
Gesamtansicht) steuern alle drei Bereiche. Ein Doppelklick setzt den
gemeinsamen Zeitbereich zurück.

Die CSV-Dateien bleiben unverändert; der Bericht bettet die Messdaten ein.
Zum Rendern der interaktiven Diagramme lädt der Browser Plotly.js 4.1.1 vom
Plotly-CDN. Beim Öffnen des Berichts ist deshalb eine Internetverbindung nötig.
Der Python-Generator selbst benötigt keine zusätzlichen Python-Pakete.

```bash
python3 CreateCharts.py
```

Der HTML-Bericht mit interaktiven Diagrammen wird standardmäßig als
`data/HeatHistoryCharts.html` gespeichert und kann im Browser geöffnet oder
gedruckt werden. Die Diagramm-Werkzeugleiste bietet außerdem Exportfunktionen.
Abweichende Verzeichnisse lassen sich angeben:

```bash
python3 CreateCharts.py --data-dir data --output data/HeatHistoryCharts.html
```

Die Zeitachse ist UTC. Da die CSV nur dann eine Zeile enthält, wenn sich
Temperatur oder Schaltzustand ändern, verbindet das Temperaturdiagramm die
gespeicherten Messpunkte; die Zustandskurven halten den letzten Wert bis zur
nächsten Änderung.

## Diagramme direkt im Browser

`python3 HeatHistory.py` startet Logger und Diagrammserver gemeinsam in einem Prozess. Unter `http://localhost:8091/` werden bei jedem Aufruf/Neuladen die Diagramme aus dem aktuellen CSV-Bestand erzeugt, ohne vorherigen HTML-Build. Ein eigener HTTP-Thread arbeitet unabhängig von Dashboard-/UDP-Wartezeiten. `Ctrl+C` beendet beides.

Für Zugriff vom Tablet `python3 HeatHistory.py --host 0.0.0.0` verwenden und `http://<PC-IP>:8091/` öffnen; gegebenenfalls TCP-Port 8091 in der PC-Firewall freigeben. `--port` wählt den Port. Das Datenverzeichnis wird aus der Logger-Konfiguration übernommen. `--no-server` startet nur den Logger; `--once` startet keinen Server. Der separate `python3 CreateCharts.py --serve` bleibt optional verfügbar, darf jedoch nicht denselben Port wie der gemeinsame Prozess verwenden. Bei fehlenden/ungültigen Logs zeigt der Server HTTP 503; nach Behebung genügt Neuladen. Keine Rohdateien/Konfigurationen werden ausgeliefert. Plotly benötigt weiterhin das CDN.

## Installation als PC-Systemdienst

Am PC aus dem Projekt aufrufen:

```bash
./Install.sh
```

Der Installer benötigt sudo und installiert nach `~/.local/share/HeatHistory`
auf die interne PC-Platte. Vorhandene Konfiguration und Daten dort bleiben
unverändert. Nur die beiden Python-Module und Versions.txt werden aktualisiert.
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

- Einmaliger Profil-/Statusdurchlauf: `python3 HeatHistory.py --once`
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
