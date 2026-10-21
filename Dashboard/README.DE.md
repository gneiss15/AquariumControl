<!-- README.DE.md, Version: 2.11 -->
# Aquarium Dashboard

[English](README.md)

## Hardware und Dokumentationsstand

Dashboard und das Programm MultiTimer laufen auf demselben Raspberry Pi. MultiTimer steuert die angeschlossene achtfache Relaiskarte. Fotos und die bisherige Beschreibung von Relaiskarte, Netzteil und Gehäuse stehen unter [MultiTimer – Hardware](../MultiTimer/Hw/README.DE.md). **Coming soon: Die Hardwarebeschreibung wird noch vervollständigt.**

Separate Übersicht und Bedienoberfläche für mehrere Dashboard-kompatible Geräte. Die Übersicht gruppiert Ausgänge nach Aquarium; gemeinsam genutzte Ausgänge können in mehreren Aquariumgruppen erscheinen. Die Geräte senden aktuelle Zustände und Messwerte wie Temperatur, Spannung, Strom und Leistung per UDP-Broadcast an den DashboardServer. Dieser leitet die Daten an die Browser weiter; die Browser fragen die Geräte dafür nicht direkt ab.

## Externe Laptop-Anzeige

Registrierte `ExternalSources` erscheinen als reine Statussender unter System. `LaptopBattery` wird als Laptop angezeigt; sein
`Status.BatteryPercent` erscheint als Akkustand in Prozent. Der Schaltausgang
LaptopSw und seine Rückmeldeüberwachung bleiben separate Anzeigen.

Der Server übernimmt nur Pakete registrierter MAC@Service-IDs in den Cache.
Der Browser benötigt keine Geräte-/Konfigurations-API am Laptop. Nach 20 Sekunden
ohne Statuspaket gilt der Sender als Offline; veraltete Messwerte werden
ausgeblendet. Ungültige Prozentwerte werden als „—“ angezeigt. Der Laptop bleibt
eine externe Quelle und wird nicht zum verwalteten Feeder/Schaltgerät.

## MAC-Identität und Funktionstausch

Jedes physische Gerät wird anhand seiner MAC-Adresse und Dienstkennung erkannt. Die Zuordnung zu einer Funktion, etwa Fütterung oder Beleuchtung, lässt sich im Bereich **Gerätetausch** ändern. Zeitpläne und Kanalrollen bleiben dabei der jeweiligen Funktion zugeordnet.

Verbraucher vorher von beiden Geräten trennen. Tausch nur innerhalb Feeder bzw. TimerSw; Hardwareparameter bleiben am Exemplar. Hostname, ApSsid, ApPw und FtpPw wandern mit der Funktion. Beide betroffenen ESPs starten neu; ein ausstehender Tausch bleibt bis zur frischen Bestätigung im privaten Journal erhalten.

Der Dialog warnt vor veralteten PC-FS-Binaries. Dashboard synchronisiert keine PC-Dateien. Der ausdrückliche PC-Abgleich liest Geräte-FS zurück und übernimmt auf Wunsch persistente Daten in das passende Hardwareprojekt. Vollständige Bedienung und Prüfgrenzen in [DeviceIdentity.md](../Libs/GnBasicLib/DeviceIdentity.md).

## HW-Endpunkte und Kanaltausch

Server 1.23/Data 2.69 erweitert Gerätezuordnung um einzelne HW-Endpunkte. Adresse: `MAC@Diensttyp/Kanalnummer`. `PhysPos: "Main X"` liefert die MT-Steckdosennummer 1..8 und den festen Anzeigenamen `MultiTimer1@X`; Nous/Shelly und die derzeitigen Feeder-Temperaturgeber verwenden Kanal 0. Die JSON-Reihenfolge bleibt davon unabhängig. Fähigkeiten sind bitcodiert: 0 keine, 1 Schalten, 2 Temperatur messen, 3 beides. Aktuell gibt es Endpunkte mit 1 oder 2; Tausch verlangt exakt identische, nichtleere Fähigkeiten. Ein MT-Eintrag ohne Ausgang ist kein Schaltziel.

In der aufklappbaren Gerätetausch-Box werden HW-Endpunkte einschließlich ihrer aktuellen Funktion ausgewählt. Die Tabelle unterscheidet Fütterungsfunktion und Temperaturgeber. MT↔MT, MT↔Nous/Shelly und Nous↔Shelly verwenden denselben Kanaltausch. Der Anlass ist beliebig. Beide Verbraucher vorher trennen, Geräte versorgt lassen. Während ausstehender Übernahme sind weitere Zuordnungsänderungen sowie zentrale Zeit-/Profiländerungen gesperrt.

Name, Rolle, Profil und Schaltzeiten/Intervalle wandern mit der Funktion; Gruppen- und Temperaturquellenzuordnung folgen ihr. GPIO, Taster, Polarität, PhysPos, Kalibrierung, Gerätehostname und Zugangsdaten bleiben an der Hardware. Die MT-Belegung und alle Kanal-Listboxen werden aus der aktualisierten Channels.json erzeugt. Reserven verwenden dieselbe Logik wie andere Timerfunktionen. Bei Funktionswechsel wird ein alter flüchtiger Override verworfen, der neue Zustand wird neu berechnet.

Der zentrale Journalstand enthält Endpoints und den vollständigen gewünschten Times-Stand gemeinsam. Nach Abbruch wird dieser wieder verteilt. MT speichert zusammengehörige Channels/Zeiten über ein dauerhaftes Redo-Journal; Nous/Shelly verwenden ein entsprechendes LittleFS-Journal für Channels, Zeiten und Intervalle. Startup-Recovery erfolgt vor der Hardwareinitialisierung. Das ist eine pro Teilnehmer wiederherstellbare Transaktion, kein gleichzeitiger physischer Wechsel aller Geräte. Abschluss verlangt die gleiche angewandte Zuordnungsrevision aller Teilnehmer. Alte Switch-Firmware wird vor dem Kanaltausch abgewiesen (`EndpointAssignments: 1` erforderlich).

Temperaturgebertausch verteilt die neue Sources-Zuordnung; Fütterungspläne werden dabei nicht getauscht. Der weiterhin angebotene Feeder-Gesamtgerätetausch bleibt davon getrennt und übernimmt auch seine bisherigen Funktions-/Zugangsdaten. Nach Endpunktinitialisierung werden Schalter über den Kanaltausch bedient, nicht mehr über den alten Gesamtgeräte-Swap.

Die bisherigen gerätebezogenen Schlüssel bleiben stabile Transport-/DevState-Schlüssel. Die Schaltfunktion ist nun separat am Endpunkt/Channels-Eintrag hinterlegt; keine Hostname-Abhängigkeit wird wieder eingeführt. PC-Abgleich und FS-Neubau nach einem Tausch bleiben ausdrücklich notwendig. HeatHistory 1.15 folgt Heizrollen an ihrem tatsächlichen Gerätekanal.

Eine bereits offene MT-Seite prüft beim Fokus/Sichtbarwerden bzw. beim Dashboard-Bereichswechsel einmal ihre Konfigurationsrevision. Geänderte Belegung/Zeiten werden neu geladen, ohne Polling-Timer. Ungespeicherte Eingaben werden erhalten und mit einem Änderungshinweis versehen. Erst nach bestätigtem Tausch zur MT-Seite wechseln; bei dauerhaft sichtbarer Seite alternativ manuell neu laden.

## Geräte-Editor

Unter Gerätezuordnung bleibt die Tabelle sichtbar. Der Button **Gerätetausch** öffnet die Box mit mittiger Überschrift; darin öffnet **Geräte-Editor** separat den ausgerichteten JSON-Editor. Beide Bereiche sind nach dem Laden geschlossen.

Der Editor enthält `Devices` mit `Function`, `MAC`, `IP`, `HttpPort`, `ServiceId`, `DeviceType`, `HardwareName`, `Group`, `UseHardwareName` und `Active`. Er ermöglicht Hinzufügen, Entfernen und Metadatenänderungen. Bestehende Funktion/MAC/Hardwaretyp sind geschützt; für Funktionswechsel weiterhin den Tausch verwenden. Die beiden Pi-Dienste bleiben unveränderlich. Doppelte Identitäten, ungültige Daten, veraltete Revisionen und Änderungen während einer ausstehenden Zuordnung werden abgewiesen.

Neue aktive Geräte benötigen vorab passende Firmware/FS und eine feste IP. Der Server prüft ihre tatsächliche MAC, Dienstklasse und Hardwareart. Bei nötiger Hostname-/Funktionsübernahme wird der Neustart bestätigt. `Active: false` erlaubt vorbereitete Einträge ohne Gerätekommunikation. Entfernen/Deaktivieren ist erst ohne zentrale Zeitplan- und Temperaturquellenreferenzen möglich. Entfernte Geräte separat außer Betrieb nehmen; die Tabellenänderung löscht deren Dateisystem nicht.

Änderungen werden atomar zusammen mit der Zuordnung in der privaten Laufzeitdatei `data/DeviceAssignments.json` gespeichert (`EditorConfiguration`: Devices/HardwareNames). `Devices.json` liefert die Ausgangswerte; Quellübertragung überschreibt die laufende Editor-Konfiguration nicht. Nach bestätigter Übernahme vom PC `python3 Aq/tools/ReadDeviceStates.py --apply` im Prjs-Stamm starten. Danach betroffene FS neu bauen.

## Dateien

- `index.html`: einzeiliger Kopfbereich, kompakte Bereichsauswahl, Aquariumgruppen, iframe-Werkzeuge und interne Detailansicht
- `Index.css`: kompakte responsive Darstellung für Tablet und Smartphone
- `Dashboard.js`: Geräteverwaltung, Statusübersicht, Tasteraktionen, Detail-Drawer und FitFrame für Geräte-iframes
- Der aktive Dashboard-Bereich wird wie bei den anderen Geräteoberflächen im Cookie `MNr` gespeichert und nach einem Neuladen wieder geöffnet. Ist ein gespeicherter Gerätebereich nicht mehr verfügbar, startet das Dashboard in der Übersicht.
- `StatusClient.js`: liest die statischen Geräteinformationen einmalig über `/api/v1/device` und den laufenden Status ausschließlich aus dem UDP-Statuscache des lokalen DashboardServer
- `Devices.json`: Gerätenamen, Adressen, FitFrame-Einstellungen, Gruppenfilter und die Reihenfolge der Übersichtsausgänge
- `DashboardServer.cpp`: kleiner eigenständiger POSIX-Webserver für die statischen Dateien und zentrale Speicher
- Der DashboardServer stellt zusätzlich das lokale Statusgerät `GN-Pi System` bereit. Es wertet `vcgencmd get_throttled` aus und zeigt aktuelle beziehungsweise seit dem Boot aufgetretene Unterspannungs-, Taktbegrenzungs-, Drossel- und Temperaturflags als Dashboard-Fehler an. CPU-Temperatur sowie die Systemlast-Durchschnitte für 1, 5 und 15 Minuten erscheinen als Messwerte.
- `TimeStore.cpp/.h`: bidirektionale Synchronisation der Schalt- und Fütterungszeiten
- `data/Times.json`: zentrale Schaltzeiten, Sync-Status sowie die je Gerät getrennt gespeicherten Kanalzuordnungen
- `data/HeatProfiles.json`: zentrale, separat gespeicherte Heizprofile
- `data/Version.json`: aus `Versions.txt` erzeugte Data-Version, die der Server zur Laufzeit liest
- Die zentrale Heizprofilseite verwendet Eingabefelder statt JSON-Text. Ein Profil lässt sich erst löschen, wenn keine Geräte-Kanalzuordnung in `data/Times.json` mehr darauf verweist. Nicht synchronisierte Geräte werden rot angezeigt.
- In der zentralen Zeitenliste lassen sich die Datensätze über die Spaltenüberschriften sortieren; die Sortierung ändert nicht die gespeicherte Reihenfolge. Klick auf `#` stellt die manuelle Reihenfolge wieder her. Die Geräteauswahl zeigt nur noch den Gerätenamen.
- Die Zeitliste zeigt Einträge als bearbeitbare Zeit-, Wochentag-, Kanal- und Zustandsfelder. `#` stellt die manuelle Reihenfolge wieder her; Typ und Art bleiben in den Daten erhalten, werden aber nicht als redundante Spalten angezeigt.
- Auf der Heizprofilseite werden nur Kanäle mit Heizprofil-Zuordnung aufgeführt. Bei Geräten mit nur einem Kanal steht in der Kanalspalte `-`; die Funktion wird separat angezeigt. Funktion und Profil lassen sich ändern; vorhandene Gerätekanäle können als Heizprofilnutzer hinzugefügt oder daraus entfernt werden. Dabei bleibt die Gerätekanalstruktur erhalten. Änderungen erhalten einen neuen Kanal-Zeitstempel und werden nur an das betroffene Gerät verteilt.
- `CopyToRasPi.sh`: liest vor einer Übertragung den Pi-Datenbestand zurück, erstellt die Snapshots und führt den Vergleich der zentralen Dateien durch
- `Makefile`: Build des Dashboard-Servers
- `DashboardServer.service`: systemd-Servicevorlage
- `Install.sh`: Installation und Aktivierung des systemd-Dienstes

## Feste Netzwerkzuordnung

| Gerät/Dienst | Adresse |
| --- | --- |
| Aq Dashboard | `192.168.99.51:8090` |
| MultiTimer Aq1 & 2 | `192.168.99.51:8080` |
| Aq Tablet | `192.168.99.100` |
| FeederAq1 | `192.168.99.101` |
| LightTimerAq3 | `192.168.99.102` |
| FeederAq2 | `192.168.99.103` |
| FeederAq3 | `192.168.99.104` |
| HeaterMainAq1 | `192.168.99.105` |
| HeaterMainAq2 | `192.168.99.106` |
| HeaterMainAq3 | `192.168.99.107` |
| Shelly1 | `192.168.99.108` |
| Shelly2 | `192.168.99.109` |

## Geräte konfigurieren

`Devices.json` bearbeiten. Das lokale Statusgerät wird mit `StatusOnly: true` ohne Konfigurationsseite und ohne Kanalbereich eingebunden:

```json
{
  "Name": "Aquarium Dashboard",
  "Devices": [
    {
      "Name": "GN-Pi System",
      "BaseUrl": ".",
      "Group": "System",
      "StatusOnly": true
    },
    {
      "Name": "MultiTimer Aq1 & 2",
      "BaseUrl": "http://gn-pi:8080",
      "GroupViews": [
        { "Group": "Aq1", "Channels": ["LichtAq1Haupt", "LichtAq1Hinten", "LichtAq1Vorne", "HeizungBodenAq1", "Luft"] },
        { "Group": "Aq2", "Channels": ["LichtAq2Haupt", "LichtAq2Hinten", "HeizungBodenAq2", "Luft"] }
      ],
      "ConfigUrl": "/",
      "FrameWidth": 1200,
      "MinimumScale": 0.72
    },
    {
      "Name": "Futterautomat Aq3",
      "BaseUrl": "http://Feeder1",
      "Group": "Aq3",
      "ConfigUrl": "/Index.html",
      "FrameWidth": 1000,
      "MinimumScale": 0.72
    }
  ]
}
```

- `Name` ist der im Dashboard und in der Bereichsauswahl sichtbare Name. Ein Firmware-Update nur für den Anzeigenamen ist daher nicht nötig.
- `Group` ordnet eine Gerätekarte einer Aquariumgruppe zu. `GroupViews` erlaubt gefilterte Ansichten eines Geräts in mehreren Gruppen. Der MultiTimer zeigt so die Aq1- und Aq2-Ausgänge getrennt; der gemeinsame Ausgang `Luft` kann beiden Gruppen zugeordnet werden. `System` ist für Statusgeräte ohne Aquarium.
- `OverviewOutputOrder` in `Devices.json` bestimmt je Aq die feste Ausgabereihenfolge. Für einen dort vorgesehenen, aktuell fehlenden Messwert oder Ausgang bleibt eine Leerzeile; nicht gelistete zusätzliche Ausgänge werden anschließend angezeigt.
- Kanalzeilen zeigen nur die Funktion (z. B. `Licht Haupt`, `Heizung Boden`, `Luft`), ohne erneuten Aquariumzusatz.
- Gruppenfilter und Ausgabereihenfolge verwenden die technischen Kanalnamen aus dem Statusfeld `Name`; die numerische API-`Id` bleibt für Geräteaktionen erhalten.
- `PowerLabel` bestimmt je Gerät den Übersetzungsschlüssel der Leistungszeile: `PowerLight` für Licht und `PowerHeater` für Heizung. Ohne Angabe bleibt die allgemeine Leistungsbeschriftung erhalten.
- `ConfigUrl` ermöglicht den Konfigurationstab bereits, bevor die Geräte-API geantwortet hat. Nach erfolgreichem API-Abruf hat `ConfigUrl` aus `/api/v1/device` Vorrang.
- `FrameWidth` ist die virtuelle Breite, mit der die vorhandene Geräteseite im iframe dargestellt wird.
- `MinimumScale` begrenzt die automatische Verkleinerung. Auf breiten Ansichten verwendet das iframe die vollständige verfügbare Breite; auf schmalen Ansichten bleibt unterhalb der Grenze horizontales Scrollen möglich.
- Mit `"Enabled": false` kann ein Eintrag vorübergehend deaktiviert werden.

## Heizprofile

`data/HeatProfiles.json` ist die zentrale Quelle für Profildefinitionen und
führt mit `TimeStamp` eine globale UTC-Revision für den gesamten Profilbestand.
`data/Times.json` enthält zusätzlich `Channels`, eine Zuordnung vom Gerätenamen
zu dessen Kanalzeilen und einem eigenen UTC-`TimeStamp` je Gerät. Ein Eintrag
hat die Form `{"TimeStamp":"...","Rows":[...]}`. Die Zeitstempel der
SwitchTimes-Sets bleiben davon getrennt. Geräte behalten ihr bisheriges
gebündeltes `Channels.json` mit `Profiles` und `Channels`; das Geräteformat
ändert sich nicht. Der vorhandene `/api/times/sync` nimmt das gebündelte
`Channels.json` zusammen mit den Zeitdaten entgegen. Der DashboardServer trennt
Profilwerte nach `HeatProfiles.json` und Kanalzuordnungen nach `Times.json`.
Eine vom Gerät gemeldete Änderung an `Channels.json` wird nur unter diesem
Gerätenamen gespeichert. Bei einer Rückmeldung erhält dieses Gerät seine eigene
Kanalzuordnung zusammen mit den zentralen Profilen; Kanalzuordnungen werden
nicht an andere Geräte verteilt. Profilteilsets werden nach Profil-ID
zusammengeführt; ein leeres oder unvollständiges `Profiles`-Array löscht keine
zentralen Profile. Eine angenommene Änderung am globalen Profilbestand wird an
alle SwitchTimes-Teilnehmer verteilt. Bei einer Profilabweichung entscheidet
der Profil-Zeitstempel über die Übernahme; die gültigen Zeiten und
Kanalzuordnungen werden separat über ihre eigenen Zeitstempel ermittelt.

Die zentrale Datenkopie auf dem Raspberry Pi ist der Laufzeitstand. Beim Start
des DashboardServers werden offene Zeit-/Profilstände an die Geräte verteilt;
Geräte mit älteren lokalen Sync-Zeitstempeln übernehmen den zentralen Stand.
Gemeldete Schaltzeiten und Kanalzuordnungen werden jeweils für das meldende
Gerät gespeichert. Nur globale Profiländerungen werden an alle
SwitchTimes-Teilnehmer verteilt; jede Rückverteilung verwendet weiterhin die
eigene Kanalzuordnung des jeweiligen Geräts. `SyncOnlyDevices` in
`Devices.json` nimmt Geräte in die Profilverteilung auf, ohne ihnen eine
Dashboard-Karte zuzuweisen.

Die Seite `Zeiten` vergleicht beim Speichern die gerätespezifischen Zeit- und
Intervallinhalte mit dem bisherigen Stand. Nur geänderte Gerätesätze erhalten
einen neuen Zeitstempel und werden zur Übertragung vorgemerkt. Die Seite
`Heizprofile` vergleicht den semantischen Profilbestand; bei einer Änderung
erhält der globale Profilbestand einen neuen Zeitstempel und alle
SwitchTimes-Teilnehmer erhalten ihr eigenes Zeit-/Intervallpaket mit der
jeweiligen Kanalzuordnung und den neuen Profilen. Ein unverändertes Speichern
stößt keine Geräteverteilung an.

Die Zeitseite erlaubt Schaltzeiten und Intervalle auch für `Format=TmrSw`.
Beide Arrays werden im bestehenden `/api/v1/times`-Payload gemeinsam mit der
jeweiligen Kanalzuordnung übertragen. TmrSw speichert Intervalle in
`/Intervals.json`; ältere TmrSw-Gerätepayloads ohne `Intervals` lassen den
zentralen Intervallstand unverändert.

Die versionierten TmrSw-`DevState/<Gerät>/SwitchTimes.sync.json`-Dateien
enthalten den gemeinsamen Sync-Zeitstempel für `Channels.json` und
`SwitchTimes.json`. Sie stehen derzeit auf `1970-01-01T00:00:00.000Z` und
`Synced:false`; so gelten die übrigen Geräte-Fallbackdateien aus dem Repo als
alt und der Server kann nach einem FS-Upload seine aktuelle Version verteilen.
Die separaten `Channels.json`- und `*Times.json`-Dateien selbst haben kein
Zeitstempelfeld. `MultiTimer/WebSrc`-Dateien sind ebenfalls Build-Fallbacks.

`CopyToRasPi.sh` lädt den vollständigen Pi-Datenstand herunter. Der unveränderte
Snapshot liegt danach unter `generated/raspi/`; `generated/raspi-readable/` enthält
ausschließlich pretty-gedruckte `Times.json` und `HeatProfiles.json`. Verglichen
werden diese beiden Dateien mit dem pretty-gedruckten Build-Stand unter
`generated/build-readable/`. Neuere Zeitstempel aus
`Times.json` werden automatisch übernommen, wenn der zugehörige Datensatz
abgesehen vom Zeitstempel identisch ist. Andere Inhaltsunterschiede bleiben
für den BCO-Abgleich sichtbar. `Layouts.json` ist eine
Pi-seitige Dashboard-Datei. `HeatProfiles.sync.json` enthält den Laufzeit- und
Geräte-Syncstatus. Beide Dateien werden weder verglichen noch in den Build
übernommen oder beim Upload überschrieben.

Übernimm gewünschte Pi-Änderungen in BCO auf der linken Build-Seite. Nach dem
Schließen von BCO und Enter werden die bearbeiteten zentralen Dateien geprüft
und nach `data/` zurückgeschrieben. Vorher wird der vorherige lokale Inhalt von
`Dashboard/data/` vollständig unter
`generated/data-before-sync/<Zeitstempel-Zufallssuffix>/` gesichert. Das ist
eine Sicherung des vorherigen lokalen `data/`-Buildstands; der Pi-Snapshot liegt
unter `generated/raspi/`. Es bleiben höchstens die drei neuesten Sicherungen erhalten.
Anschließend fragt das Skript ausdrücklich, ob es
den Dashboard-Quell-/Webdateistand, `Times.json`, `HeatProfiles.json` und die
generierte `Version.json` zum Pi übertragen soll. Das kompilierte
`DashboardServer`-Programm wird nicht übertragen. Nur bei Änderungen an
Server-Quellen oder der Server-Version muss es danach auf dem Pi im
Dashboard-Verzeichnis mit `make` gebaut und der Dienst neu gestartet werden.
Eine reine Data-Versionsänderung wird durch `Version.json` sofort wirksam.
Pi-eigene Dateien wie
`Layouts.json` und `HeatProfiles.sync.json` bleiben erhalten. Bei Ablehnung
bleiben sämtliche Pi-Dateien unverändert; bestätigte lokale Build-Änderungen
bleiben in `data/` erhalten. Ein Fehler beim Download oder bei der Validierung
stoppt den Lauf vor jeglichem Upload. Das Profilformat lässt sich prüfen mit:

```bash
./tools/ValidateHeatProfiles.py data/HeatProfiles.json
```

Formatierungsänderungen gelten nicht als neue Profilrevision. Eine neue
Revision entsteht nur bei geändertem semantischem Profilinhalt.

## Übertragung auf den Raspberry Pi

`CopyToRasPi.sh` ermittelt das lokale Dashboard-Verzeichnis aus seinem eigenen Speicherort. Das Skript kann daher unabhängig vom aktuellen Arbeitsverzeichnis und vom Mountpunkt des Datenträgers direkt gestartet werden:

```bash
bash Aq/Dashboard/CopyToRasPi.sh
```

Es lädt zuerst Pi-Daten herunter, vergleicht die beiden zentralen Laufzeitdateien und synchronisiert danach den Dashboard-Quellstand sowie die beiden zentralen JSON-Dateien und `data/Version.json` nach `pi@gn-pi:${RASPI_PRJS_DIR:-/home/pi/Prjs}/Dashboard/`. Bei abweichenden zentralen Dateien fragt es vor deren Übertragung; bei einem Fehler im Vorabgleich wird nichts kopiert. Das lokal gebaute `DashboardServer`-Binary wird nicht übertragen. Nur bei Änderungen an Server-Quellen oder der Server-Version muss es auf dem Raspberry Pi mit `make` neu gebaut und der Dienst neu gestartet werden. Eine reine Data-Versionsänderung wird ohne Neubau wirksam. Für abweichende Ziele können `RASPI_HOST` und `RASPI_PRJS_DIR` gesetzt werden.

Wenn die zentralen Dateien auf dem Pi erhalten bleiben sollen, auch wenn sie vom Repository abweichen, verwende:

```bash
bash Aq/Dashboard/CopyToRasPi.sh
```

Der Kopierlauf führt den beschriebenen BCO-Abgleich und die Upload-Abfrage selbst durch. Starte ihn in einem interaktiven Terminal, damit du Änderungen im BCO auswählen und den Upload anschließend ausdrücklich bestätigen kannst.

## Pi-Daten und Validierung

Der Pi-Datenbestand wird durch `CopyToRasPi.sh` automatisch vor dem Vergleich eingelesen; ein separates Download-Skript ist nicht erforderlich. Der vollständige Pi-`data/`-Bestand wird unverändert unter `Aq/Dashboard/generated/raspi/` abgelegt. Unter `Aq/Dashboard/generated/raspi-readable/` entstehen pretty-gedruckte Kopien ausschließlich von `Times.json` und `HeatProfiles.json`. Pi-seitige Dateien wie `Layouts.json` und `HeatProfiles.sync.json` bleiben im Roh-Snapshot, werden aber nicht für den zentralen Vergleich aufbereitet. Das Zurücklesen ändert keine Datei unter `data/`. `RASPI_HOST` und `RASPI_PRJS_DIR` werden von `CopyToRasPi.sh` unterstützt.

Die Datenverzeichnisse haben folgende Aufgaben:

- `data/`: zentraler Build-Datenstand (`Times.json`, `HeatProfiles.json`).
- `generated/raspi/`: unveränderter Snapshot des vollständigen vom Pi zurückgelesenen `data/`-Verzeichnisses.
- `generated/build-readable/`: temporäre pretty-gedruckte Build-Kopien für BCO.
- `generated/raspi-readable/`: pretty-gedruckte Pi-Kopien nur der beiden zentralen Vergleichsdateien.
- `generated/data-before-sync/`: Sicherungen des lokalen `data/`-Stands unmittelbar vor dem Übernehmen von BCO-Änderungen; maximal drei neueste Sicherungen.

`tools/ValidateHeatProfiles.py` wird im `CopyToRasPi.sh`-Ablauf auf die vom Pi gelesene `HeatProfiles.json` sowie auf den lokalen Build-Stand angewendet. Es prüft JSON-Struktur, Pflichtfelder, Datentypen, Hysteresegrenzen, eindeutige Profil-IDs und den optionalen globalen Zeitstempel. Nach einem BCO-Abgleich wird die bearbeitete Build-Datei vor dem Zurückschreiben erneut geprüft. Zur Einzelprüfung kann das Tool weiterhin direkt auf eine Datei angewendet werden.

## Server bauen und starten

Auf dem Raspberry Pi im Dashboard-Verzeichnis (Standardpfad `/home/pi/Prjs/Dashboard`, mit `RASPI_PRJS_DIR` überschreibbar):

```bash
export RASPI_PRJS_DIR="${RASPI_PRJS_DIR:-/home/pi/Prjs}"
cd "$RASPI_PRJS_DIR/Dashboard"
make
./DashboardServer 8090 .
```

Vor jedem Build erzeugt das Makefile `Version.inc.h` und `data/Version.json`
erneut aus der Masterdatei `Versions.txt`. Die Server-Version wird in den
Server kompiliert. Die Data-Version steht separat in `data/Version.json` und
wird von `/api/version` zur Laufzeit gelesen. Eine reine Data-Versionsänderung
erfordert daher keinen Server-Neubau; `CopyToRasPi.sh` erzeugt und überträgt
die aktualisierte Datei automatisch. Nur Änderungen an Server-Quellen oder
der Server-Version erfordern anschließend `make` und einen Dienstneustart.

Dashboard öffnen:

```text
http://gn-pi:8090/
```

Optional funktioniert weiterhin der Python-Server:

```bash
RASPI_PRJS_DIR="${RASPI_PRJS_DIR:-/home/pi/Prjs}"
python3 -m http.server 8090 --bind 0.0.0.0 --directory "$RASPI_PRJS_DIR/Dashboard"
```

Der Python-Server eignet sich nur für die statische Anzeige. Für die Dashboard-APIs und die Bearbeitung zentraler Daten wird `DashboardServer` benötigt.

## systemd-Dienst installieren

Der Pi behält `/home/pi/Prjs/Dashboard`; nur der Entwicklungs-PC-Quellbaum liegt jetzt unter `Prjs/Aq/Dashboard`. Das lokale Verschieben allein erfordert daher keinen neuen systemd-Installationslauf. Wenn du das Dashboard auf dem Pi aktualisieren willst, übertrage es vom PC und baue es danach unter dem bisherigen Pi-Pfad. `Install.sh` kann dort anschließend wie gewohnt `DashboardServer.service` installieren und neu starten; es erhöht selbst per `sudo` die Rechte.

Vorhandene Dashboard-`@reboot`- und `pgrep`-Watch-Einträge werden aus `/etc/crontab` entfernt. Ein früher unter dem Namen `aquarium-dashboard.service` installierter Dienst wird deaktiviert und entfernt.

```bash
export RASPI_PRJS_DIR="${RASPI_PRJS_DIR:-/home/pi/Prjs}"
cd "$RASPI_PRJS_DIR/Dashboard"
make
sudo ./Install.sh
```

Die Anmeldung auf dem Pi erfolgt beispielsweise mit `ssh pi@gn-pi`. Bei einem abweichenden Prjs-Pfad muss `RASPI_PRJS_DIR` sowohl beim Build als auch beim Installer gesetzt sein.

Status und Logausgabe:

```bash
systemctl status DashboardServer.service
journalctl -u DashboardServer.service -f
```

## Verhalten

- Der Kopfbereich enthält nur noch den Dashboard-Namen in einer Zeile.
- Die obere Bereichsleiste enthält eine Auswahlliste mit `Übersicht`, `Zeiten` und allen Gerätekonfigurationen. Dadurch bleibt sie auch bei vielen Geräten einzeilig.
- `Zeiten` bearbeitet die eine zentrale `data/Times.json`; Einträge können unabhängig von Gerät und Typ global angeordnet werden. UUIDs identifizieren die Einträge dauerhaft.
- Auf schmalen Ansichten werden Zeitdatensätze als kompakte Karten statt in einer breiten Tabelle dargestellt. Der Sync-Status ist eine zugängliche Checkbox-Anzeige; die Zeitfelder bleiben ohne horizontales Seitenscrollen erreichbar.
- Speichern-/Verteilen-Hinweise verschwinden automatisch, sobald die betroffene Synchronisierung erfolgreich abgeschlossen ist. Bei ausstehenden Geräten bleibt der Hinweis sichtbar und prüft den Status weiter.
- `data/HeatProfiles.json` ist die zentrale Sollquelle für Heizprofile. Pro Gerät bleiben die Kanalzuordnungen separat in `data/Times.json`; aus beidem wird das vorhandene Geräteformat zusammengesetzt.
- Pro `Device`/`Type` liegt genau ein UTC-Timestamp und ein `Synced`-Status vor. Änderungen werden sofort übertragen; nicht erreichbare Partner werden periodisch erneut versucht.
- Die Übersicht ordnet Daten nach den Gruppen `Aq1`, `Aq2`, `Aq3` und `System`. Ab 30 rem (bei Standardschriftgröße 480 CSS-Pixel) stehen zwei gerahmte Gruppen nebeneinander, darunter eine. Ab 75 rem (1200 CSS-Pixel) stehen auf Geräten mit feinem Zeiger vier Gruppen nebeneinander; Touch-Geräte mit `pointer: coarse` behalten im Hochformat zwei Spalten und zeigen im Querformat ab 60 rem vier Spalten. Bei zwei Spalten stehen Aq1/Aq2 oben und Aq3/System darunter. Leistungsbeschriftung und Messwerte stehen nebeneinander; die kompakte Schreibweise lautet beispielsweise `233.1V, 0.000A, 0.0W`.
- Auf Touch-Geräten ab 30 rem wird die gesamte Übersicht bei Bedarf proportional in die verfügbare Browserbreite und -höhe eingepasst. Browserleisten, Gruppenstatus und sichtbare Meldungen werden berücksichtigt; Inhalte werden nicht abgeschnitten. Bei wenig Platz werden Schrift und Bedienelemente entsprechend kleiner.
- Ausgangszeilen der Gruppenübersicht haben wie die Statuszeilen keine einzelnen Trennlinien.
- Die Gruppenübersicht verwendet auch auf Touch-Geräten kompakte Kanalzeilen (1,65 rem Mindesthöhe), entsprechend hohe Leerzeilen und kleinere Gruppenabstände.
- Je Aq stehen Gerätenamen mit Live-/Offline-Status kompakt über einer Trennlinie. Darunter folgen Temperatur, Heiz- und Lichtausgänge sowie Luft in der Reihenfolge aus `Devices.json`; fehlende Einträge belegen eine Leerzeile. Leistung steht am Ende: zuerst Heizung, danach gegebenenfalls Licht. Der nicht schaltbare Feeder-Status `Fütterung` und leere „Keine Kanäle“-Meldungen erscheinen dort nicht.
- `GN-Pi System`, `Shelly1` und `Shelly2` stehen in der Gruppe `System`. Beide Shellys haben Status-/Ausgangsanzeige und einen Konfigurationstab; ihre Leistungszeilen sind mit dem jeweiligen Shelly-Namen beschriftet. Die Ausgangszeilen heißen `Shelly1` bzw. `Shelly2`; die gerätespezifischen Übersetzungen gelten auch in Kanal-Details und Aktionsmeldungen. Die zentrale Synchronisierung verwendet ihre Einträge unter `Devices`. Der MultiTimer erhält je eine Ansicht in Aq1 und Aq2. Beide Ansichten verwenden dieselbe Geräteverbindung und filtern die Ausgänge gemäß `GroupViews`; `Luft` ist beiden Gruppen zugeordnet.
- Globale gespeicherte Layouts passen nicht zur Aquariumgruppierung. Der bisherige Layout-Editor wurde entfernt. Die automatische Darstellung wurde vom Benutzer auf Galaxy Tab S9 und Galaxy Tab S7 (Fully Kiosk Browser) bestätigt; gespeicherte Aquariumlayouts sind dafür nicht erforderlich. Die Pi-seitige Datei `data/Layouts.json` wird von der Oberfläche ignoriert und von `CopyToRasPi.sh` weder überschrieben noch gelöscht.
- Der jeweilige iframe wird erst beim ersten Auswählen seines Eintrags erzeugt.
- FitFrame skaliert das iframe automatisch bis zur konfigurierten Untergrenze; danach wird horizontales Scrollen zugelassen.
- Die Dashboard-Oberfläche ist vertikal kompakter: Übersicht, Bereichsauswahl, Detailansicht und Zeitentabelle sind gegenüber dem früheren Stand um ungefähr 10 px pro Zeile reduziert. Die Titelzeile mit `Aquarium Dashboard` bleibt unverändert groß.
- `GN-Pi System` zeigt CPU-Temperatur sowie Systemlast-Durchschnitte (1, 5 und 15 Minuten) an. Darunter erscheint der Fehlerblock nur, wenn ein aktueller oder historischer Systemfehler vorliegt; ein Kanalbereich wird für dieses Statusgerät nicht dargestellt.
- Ein Tipp auf einen Kanal öffnet eine interne Detailansicht innerhalb derselben Browserseite. Es wird kein Browser-Popup und kein neues Fenster verwendet.
- Beim Start wird für jedes Gerät `/api/v1/device` einmalig direkt geladen. Laufende Statusdaten kommen aus dem UDP-Cache des DashboardServer über einen gemeinsamen SSE-Datenstrom `/api/events`; `/api/device-status/<ID>` liefert den Anfangsstand und dient als Polling-Rückfall.
- Liefert `/api/v1/device` zusätzlich `DataVersion`, zeigt das Dashboard die tatsächlich auf dem Gerät liegende Data-/LittleFS-Version an und verwendet sie als Cache-Buster für den Konfigurations-iframe.
- Die Kopfzeile zeigt `Server <Version> · Data <Version>`. `/api/version` liefert die kompilierte Server-Version und liest die Data-Version bei jedem Aufruf aus `data/Version.json`; diese Datei wird aus `Versions.txt` erzeugt. `Dashboard.js` führt keine separate Data-Version.
- Geräte publizieren ihren Status spätestens alle fünf Sekunden und zusätzlich bei relevanten Änderungen per UDP-Broadcast auf Port 44444.
- Der DashboardServer fordert beim Start mit `ID:[]` einen Status aller Publisher an und bei fehlendem beziehungsweise veraltetem Cacheeintrag gezielt die betreffende Geräte-ID.
- Browser öffnen für laufende Statusdaten weder WebSockets noch periodische HTTP-Verbindungen zu den Geräten.
- Gerätezugriffe besitzen ein Timeout von sechs Sekunden; nicht erreichbare Geräte werden alle zehn Sekunden erneut initialisiert.
- Das Dashboard prüft seinen eigenen Webserver regelmäßig über `Devices.json`. Dabei wird gleichzeitig der Inhalt mit dem beim Start geladenen Stand verglichen. Änderungen an `Devices.json` werden ohne Neustart des `DashboardServer` erkannt; die Browserseite lädt sich anschließend automatisch neu.
- Die Pi-seitige `data/Layouts.json` wird nicht von der Weboberfläche verwendet. `data/Times.json` bleibt Laufzeit-/Synchronisationszustand und wird über die vorhandene Zeiten-API aktualisiert.
- Bei WLAN- oder Serverausfall erscheint eine Statusleiste mit automatischem Retry und manuellem Sofortversuch.
- Sobald WLAN und Dashboard-Server wieder erreichbar sind, wird die Seite kontrolliert neu geladen.
- Beim Aufwecken beziehungsweise erneuten Sichtbarwerden wird sofort eine Verfügbarkeitsprüfung ausgeführt.
- Der DashboardServer liefert statische Dateien aus und stellt `/api/version`, `/api/layouts`, `/api/times`, `/api/times/sync`, `/api/heatprofiles` sowie `/api/device-status/<ID>` bereit. `/api/times/sync` nimmt das bestehende gebündelte `Channels.json` zusammen mit Zeitdaten entgegen; es gibt keinen besonderen Profil-`Type` und keinen neuen Geräte-Endpunkt. Die Geräteverteilung nutzt `/api/v1/times`. Der laufende Geräte-/Temperaturstatus bleibt davon getrennt auf UDP.
- `CopyToRasPi.sh` lädt den gesamten Pi-Data-Bestand als Snapshot, vergleicht über BCO ausschließlich `Times.json` und `HeatProfiles.json`, übernimmt bestätigte linke Build-Änderungen zurück nach `data/` und überträgt diese beiden Dateien sowie die generierte `Version.json` nur nach ausdrücklicher Zustimmung. `Layouts.json` und `HeatProfiles.sync.json` bleiben Pi-seitig erhalten.

Dashboard und Geräte sollten gemeinsam über HTTP betrieben werden. Eine über HTTPS geladene Seite darf Browser-seitig keine unsicheren HTTP- oder WebSocket-Verbindungen öffnen.


## Aktionen

Geräte können in `/api/v1/device` eine Tasteraktion veröffentlichen:

```json
"Actions": {
  "ButtonPress": {
    "Path": "/api/v1/button/{ChannelId}/{Press}",
    "Presses": ["Short", "Long"]
  }
}
```

Bei Kanälen mit `HasOutput: true` wird der angezeigte Zustand in der Übersicht zur Aktion. Ein physischer Eingang ist dafür nicht erforderlich:

- Tipp auf `EIN` beziehungsweise `AUS`: kurzer Tastendruck
- Tipp auf den Kanalnamen: Detailansicht
- Detailansicht: kurzer und, sofern unterstützt, langer Tastendruck
- langer Tastendruck: zweite Bestätigung innerhalb von vier Sekunden

Das Gerät führt dabei denselben internen Codepfad wie beim physischen Taster aus. Die Übersicht schickt keinen direkten Ausgangszustand.

## Gerätekonfigurationen

`Auto xx %` und der runde Neuladen-Button stehen direkt rechts neben der Bereichsauswahl. Die Geräteseite beginnt ohne zusätzlichen Dashboard-Rahmen unmittelbar darunter. Bei Erreichen von `MinimumScale` wird nicht weiter verkleinert; stattdessen kann horizontal gescrollt werden.

## Feste Hardware-Namen

Devices.json → HardwareNames ordnet die Gehäusebeschriftung ausschließlich der MAC zu (Feeder1–3, Nous1–4, Shelly1–2). Diese Tabelle ist statische Metadatenquelle und wird beim normalen Quelltransfer übertragen; die laufende DeviceAssignments.json bleibt geschützt. UseHardwareName kennzeichnet Reservefunktionen unter System: Statuszeile, Ausgang und Leistungszeile zeigen dort den Namen der aktuell zugeordneten Hardware. Konkrete Aquariumfunktionen behalten ihre Funktionsbezeichnung. In Gerätezuordnung steht der Hardware-Name zusätzlich zur MAC. Keine Änderung von Wire-ID, Zeitplänen oder Geräte-Config. Feeder3 ist trotz bekannter MAC weiterhin inaktiv.

Hostname-Regel beim Gerätetausch: Für konkrete Funktionen wird der neue Funktionsname gesetzt. Für mit UseHardwareName gekennzeichnete Reservefunktionen wird der Hardware-Name der Ziel-MAC gesetzt. Hostname wird daher beim Transfer berechnet und nicht unverändert von der Quellhardware kopiert. Die übrigen drei Funktionsfelder ApSsid/ApPw/FtpPw behalten ihre bisherige Übertragung. Interne Funktionsschlüssel, physische IDs und DevState-Verzeichnisnamen bleiben davon unabhängig.

### Zuordnungsschema 2

`data/DeviceAssignments.json` enthält die Geräte-/Dienstliste unter `Devices` und die vollständige HW-Endpunkt-Zuordnung unter `Endpoints`. `Version: 2` bezeichnet dieses Dateiformat; `State` entfällt. Der DashboardServer migriert ältere MAC-Tabellen beim Start atomar, ohne die Zuordnungsrevision oder ein laufendes Transferjournal zu verändern. Die Pi-Datei bleibt autoritativ und wird beim Quelltransfer nicht überschrieben.

`EndpointId` hat die Form `MAC@ServiceId/Kanalnummer`; `Capabilities` ist ein separates Bitfeld (1 Schalten, 2 Temperatur messen). Beim Feeder sind Fütterungsdienst und Temperatur-Endpunkt getrennte Funktionen derselben Hardware. Die Gerätezuordnung zeigt die Fähigkeiten deshalb in einer eigenen Spalte.

### Tabellenbreiten

Dashboard-Tabellen passen ihre Spalten im Tabellenlayout automatisch an die gerenderten Inhalte an. Die rechte Grenze einer Überschrift lässt sich ziehen; ein Doppelklick setzt diese Spalte auf automatische Inhaltsbreite zurück. Per Tastatur ändern Links/Rechts die Breite, Enter stellt die Automatik wieder her. Manuelle Breiten werden pro Tabelle in diesem Browser gespeichert und bleiben bei Datenaktualisierungen erhalten. Auf schmalen Displays bleibt die Zeitenliste eine Kartenansicht. Gerätezuordnung bleibt eine Tabelle; bei Bedarf kann sie horizontal gescrollt werden. JSON-Schreibvorgänge speichern `/` ohne das optionale Escape.

### Datenverzeichnisse und Pi-Abgleich

Daten liegen einheitlich unter `data/`, Rücklesedaten unter `generated/raspi/`; temporäre Downloads ebenfalls unter `generated/`. `CopyToRasPi.sh` liest zuerst den aktuellen Pi-Stand aus `data/` zurück und führt den bestehenden Vergleich durch. Nach bestätigter Übertragung werden die abgestimmten Daten und Quellen übertragen. Die einmalige Umbenennung auf dem Pi wurde durchgeführt; ihr Sondercode ist entfernt. Transfer, Server und Versionsgenerator verwenden ausschließlich `data`.

Die Gerätezuordnung lässt sich durch Klick auf die Spaltenüberschrift aufsteigend/absteigend sortieren. Die Sortierrichtung bleibt bei Datenaktualisierungen erhalten; gespeichert wird dadurch keine fachliche Umordnung.

In Gerätezuordnung und Tauschauswahl wird die Aquariumzugehörigkeit einheitlich als `AqX` angezeigt. Enthält der übersetzte Funktionsname bereits dieselbe Angabe, wird sie nicht nochmals angehängt. Fütterung und Temperaturgeber bleiben als getrennte Funktionen gekennzeichnet.
Reserven mit `UseHardwareName` verwenden auch in Gerätezuordnung und Tauschauswahl den tatsächlichen Hardware-Namen, nicht den internen Transport-/Funktionsschlüssel.

Feeder-Gesamtgerätetausch verlangt identische konfigurierte Temperaturfähigkeiten: mit Sensor nur gegen mit Sensor, ohne nur gegen ohne. Dashboard liest dafür `Measurements` der Status-API; der Temperatureintrag ist bei `DallasPresent=true` vorhanden, auch bei ungültigem Messwert. Fehlende/unklare Statusdaten sperren den Tausch. Deaktivierte Temperatursensoren erscheinen nicht als wählbare Temperatur-Endpunkte. Gespeicherte Funktionszuordnungen bleiben für erneutes Aktivieren erhalten; ungültige Messwerte verändern die Hardwarefähigkeit nicht.

Die Dashboard-Dienstfunktion heißt `Dashboard`; die physische MAC@Service-ID bleibt unverändert. Eine vorhandene frühere Dienstbezeichnung wird beim Start in der Zuordnung korrigiert, ohne die Zuordnungsrevision zu verändern. Programm und Dienst heißen weiterhin `DashboardServer`. Hinweise zu separaten Anzeigenamen beziehen sich allgemein auf das Heimnetz.

Für Quelländerungen ohne Änderungen an zentralen Zeiten/Profilen kann `./CopyToRasPi.sh --sources-only` verwendet werden. Der vollständige Pi-Datenstand wird zuvor wie üblich zurückgelesen; nach Bestätigung werden nur Quellen/statische Dateien und Version.json übertragen. Times.json, HeatProfiles.json und die bestehende Gerätezuordnung bleiben unverändert.

Der native Diensttyp heißt `MultiTimer`. Bestehende frühere Typbezeichnungen in der Zuordnung werden ohne Änderung der physischen ID oder Zuordnungsrevision normalisiert. Native Projekt-/Programm-/Dienstpfade verwenden MultiTimer; der bestehende Zeitdaten-Formatbezeichner bleibt kompatibel.

## Externe UDP-Messquellen

Zusätzliche Sender ohne Geräte-/Konfigurations-API werden in der autoritativen
`data/DeviceAssignments.json` unter `ExternalSources` als Funktionsname →
`MAC@3` eingetragen. Sie werden nicht als Feeder verwaltet, getauscht oder auf
Zuordnungsbestätigungen geprüft. Die Tabelle wird mit der zentralen Zuordnung
an MultiTimer und TimerSw verteilt; ein neuer Zuordnungsstand erfordert eine
neue Registry-Revision und Verteilung. Änderungen dieser Tabelle derzeit bei
gestopptem Dashboard durchführen; der Geräteeditor bearbeitet sie nicht.

Beispiel: `LaptopBattery` → `F4:06:69:2A:E9:CD@3`. Der Laptop sendet alle fünf
Sekunden UDP-Statuspakete an Broadcast-Port 44444 mit dem Ladestand im Feld
`Value`. Das Profil `LaptopCharge` verwendet `TempId: LaptopBattery`,
40/80 als Ein-/Ausschaltgrenzen und `NoTempOn: false`: nach 20 Sekunden ohne
gültige Meldung bleibt der Ausgang AUS. Nous1 übernimmt die Funktion
`LaptopSw` unter System. Die Wiederverwendung des Temperaturfelds ändert
nicht dessen Einheit in der bestehenden Oberfläche.

## Rückmeldeüberwachung und Warnton

Unter **Gerätezuordnung → Rückmeldeüberwachung** öffnet ein eigener Button den
JSON-Editor. Die Regeln gehören nicht zu den Heiz-/Ladeprofilen. Jede Regel
verknüpft eine `SwitchFunction` mit einer unabhängigen `Source`, der
Telegrammkennung `StatusKind`, einem booleschen `FeedbackField` und der
Toleranz `GraceSeconds` (1 bis 3600 Sekunden). Der Schaltzustand kommt aus dem
tatsächlich gemeldeten Kanal-`State`, einschließlich manueller Übersteuerung.
Die Schaltfunktion folgt bei einem Endpunkttausch ihrer aktuellen Hardware.

Die vorbereitete Regel prüft `LaptopSw` gegen `LaptopBattery` mit
`Status.Kind: LaptopPower` und `Status.AcOnline`, Toleranz 60 Sekunden.
`BatteryState` entscheidet nicht über den Fehler: Ein Laptop kann trotz
Netzversorgung aufgrund seines Ladelimits nicht laden. Der Monitor selbst
schaltet nichts. Er zeigt unter System den Prüfzustand bzw. eine Abweichung an.

Nur durchgehend unterschiedliche, gültige und höchstens 20 Sekunden alte
Werte führen nach der Toleranz zum Fehler. Übereinstimmung, Richtungswechsel,
fehlende/ungültige Daten oder eine neue Zuordnung setzen die Wartezeit zurück.
Fehlende Rückmeldungen werden als unbekannt angezeigt; sie sind kein gemeldetes
AUS. Die Ladeautomatik behält unabhängig davon ihre Einstellung `NoTempOn`.

Gespeichert wird atomar in `data/FeedbackMonitors.json`; die Datei im
Projektstamm dient nur als Ausgangskonfiguration, solange keine gespeicherte
Konfiguration vorliegt. Der Transfer erhält die Laufzeitdatei. Der Editor lädt
nur beim Öffnen bzw. auf Wunsch und erkennt zwischenzeitliche Änderungen.

Am S7 im Fully Kiosk Browser nach Laden der Dashboardseite **Warnton aktivieren**
antippen. Dies gibt Browser-Audio frei und spielt einen kurzen Testton. Die
Freigabe gilt für diese Seite und muss nach einem vollständigen Neuladen erneut
erfolgen; Lautstärke und Audiofreigabe des S7 müssen passen. Bei einer bestätigten
Abweichung erklingen drei kurze Töne, solange unquittiert alle 30 Sekunden.
**Warnton quittieren** lässt die optische Meldung bestehen und verstummt bis
zur Erholung oder einer neuen Störungsart. **Warnton ausschalten** deaktiviert
die Tonfunktion auf diesem Browser. Ohne frische Dashboard-Statusupdates wird
der Ton nicht wiederholt. Ein echter Hörtest im Fully Kiosk Browser steht aus.

### Rückmeldetexte und Messwert-Kompatibilität

Die Anzeige verwendet „Zustand bestätigt“, „Zustand wird geprüft“ und nach der
Toleranz „Zustand nicht bestätigt“. Fehlende und veraltete Daten heißen
„Rückmeldung fehlt“; frische, aber nicht auswertbare Rückmeldefelder heißen
„Rückmeldung inkorrekt“. Diese unbekannten Werte werden weiterhin nicht als
false interpretiert und erzeugen keinen Abweichungs-Warnton.

UDP-Messwerte stehen in Version 2 als benannte Felder in `Status`.
Root-`Value` und Root-`Temperature` werden nicht mehr ausgewertet.
Die Rückmeldeüberwachung verwendet das unabhängige `Status.AcOnline`.
Der Laptop sendet `Status.BatteryPercent`; AcOnline und BatteryState bleiben
im selben Statusobjekt.

## UDP-Version 2

Der bereinigte Server-Quellstand akzeptiert ausschließlich Version 2 und verwirft
Version-1-Pakete. Die Profilverwaltung erhält `Measurement` (Messwertfeld),
z. B. `Temperature` oder `BatteryPercent`; ohne Feld gilt `Temperature`.
Alle aktiven Profile und Sender sind bereits umgestellt und real geprüft.
Hardwarefähigkeiten, Quellen-MAC-Zuordnung und unabhängige JSON-Datenversionen
bleiben unverändert. Vollständiger Vertrag:
[DeviceIdentity.md](../Libs/GnBasicLib/DeviceIdentity.md).

Der laufende Server 1.35 sendet bereits UDP-Requests mit Version 2; seinen
Systemstatus erzeugt er lokal über HTTP. Der für den nächsten Produktbuild
vorgemerkte Server 1.36 entfernt nur die alte Empfangslogik. Diese Quellbereinigung
ist noch nicht auf den Pi übertragen oder als Produkt gebaut.

Der Aktivierungs-Hörtest und spätere Rückmelde-Warnton verwenden den vollen
Audiopegel (Gain 1.0). Die Lautstärke wird am Tablet/Browser über die
Medienlautstärke geregelt. Ein/Aus wird pro Browser als Cookie
`AquariumDashboard.FeedbackSound` für ein Jahr gespeichert. Beim Laden und
bei dauerhaft geöffneter Seite einmal täglich wird die Restlaufzeit geprüft.
Bei höchstens 30 Tagen Rest wird sie erneut auf ein Jahr gesetzt, für Ein und
Aus gleichermaßen. Alte 0/1-Cookies werden unter Beibehaltung der Einstellung
übernommen. Gelöschte Cookies werden nicht automatisch neu angelegt. Bei gespeichertem
Ein versucht die Seite nach dem Laden eine stille Audio-Wiederaufnahme. Erlaubt
der Browser sie, lautet der Button „Warnton ausschalten“. Bei blockierter oder
noch ausstehender Freigabe steht dort „Warnton freigeben“; einmal antippen.
Automatische Wiederaufnahme spielt keinen Bestätigungston; ausdrückliches
Aktivieren/Freigeben spielt denselben Hörtest wie der spätere Alarm. Bei
Aus bleibt der Ton nach Reload aus. Keine Browser-Autoplay-Freigabe vortäuschen.

Prüfung im Fully Kiosk Browser: aktivieren und Hörtest prüfen, neu laden und
Buttontext prüfen (ausschalten = bereit; freigeben = Browser benötigt Klick).
Anschließend ausschalten, erneut laden: der Button muss „Warnton aktivieren“
anzeigen und die Seite stumm bleiben. Die Speicherung gilt nur für denselben
Browser/Host; gelöschte Cookies oder private Sitzungen können sie verwerfen.

## Profil-Messwert und Synchronisationsanzeige

Das Messwertfeld gehört zum zentralen Profil im oberen Profileditor, nicht zur
Kanalzuordnung. Für `LaptopCharge` mit Quelle `LaptopBattery` muss `Measurement`
auf `BatteryPercent` stehen. Im Feld „Messwertfeld“ werden `Temperature` und
`BatteryPercent` vorgeschlagen; andere benannte numerische Statusfelder bleiben
als Freitext möglich. Ein leeres Feld bedeutet weiterhin `Temperature`.
Nach Änderung des Profils „Speichern / verteilen“ betätigen. Bei fehlendem
passenden Messwert und `NoTempOn: false` bleibt der zugeordnete Ausgang AUS.
Die Live-Prüfung am 2026-10-19 fand das Feld im laufenden LaptopCharge-Profil
noch nicht gesetzt; es wurde bei dieser Oberflächenkorrektur nicht live geändert.

Data 2.86 zeigt den Revisions-/Synchronisationsblock mit neutraler Grundfarbe;
synchronisierte Teilnehmer bleiben grün. Nur die eigene Zeile für nicht
synchronisierte Teilnehmer und deren Fehler ist rot. Der Revisionszeitstempel
bezeichnet die letzte Profiländerung, nicht die aktuelle Uhrzeit. Ladefehler
bleiben rot. Lokale JS-/Renderer-/Statusclient-/Alarmtests bestanden; diese
reine Oberflächenänderung benötigt keinen Serverbuild oder Dienstneustart.
Übertragung vom Prjs-Stamm: `./Aq/Dashboard/CopyToRasPi.sh --sources-only`,
danach Dashboard neu laden. Der Transfer erhält die laufenden Zeiten/Profile.

Die Profilbeschriftung `TempId` heißt ab Data 2.86 „Quelle“ (EN: „Source“).
`LaptopBattery` bezeichnet den richtigen Geber; `BatteryPercent` bezeichnet das
von ihm gelieferte Messwertfeld. Im frisch geöffneten laufenden Dashboard
(Data 2.85, 2026-10-19) ist „Messwertfeld“ bereits als drittes Feld sichtbar,
unter „Id“ und links neben „Boden Ein“. Bei abweichender älterer Browseransicht
zuerst mit Strg+F5 vollständig neu laden; die beiden Felder nicht vertauschen.

## Status-Push ab Server 1.38 / Data 2.87

Datenweg: Gerät → UDP V2 → DashboardServer → SSE → Browser.
Eine gemeinsame `EventSource`-Verbindung pro Browserseite verteilt Statusupdates
an alle Karten einschließlich externer Messsender. Gültige UDP-Pakete werden
nach Eingang weitergeleitet; auf ein sekündliches Browser-Polling wird im
Push-Betrieb verzichtet. Die A8T-Messfenster bleiben davon unabhängig.
Der lokale Pi-Systemstatus wird alle fünf Sekunden aktualisiert und hält
zugleich den Stream aktiv.

Bei Verbindungsfehlern greift das bisherige sekündliche HTTP-Polling des
Servercaches. `EventSource` verbindet sich automatisch wieder; nach 15 Sekunden
ohne Daten wird auch eine stumme Verbindung neu aufgebaut. Nach Reconnect
kommt ein Snapshot; verspätete Poll-Antworten überschreiben keinen neueren
Push-Stand. Ein Serverneustart darf Revisionsnummern wiederverwenden.
Nach 20 Sekunden ohne gültiges Geräte-UDP wird Offline gepusht; ein Reconnect
macht alte Messwerte nicht wieder frisch. Entfernte Zuordnungen werden als
Offline gemeldet. Der Server begrenzt Streams auf 32 und ausstehende Daten je
Stream auf 256 KiB; nicht lesende Browser blockieren die übrige Arbeit nicht.

Diese Änderung enthält Servercode. Vom Prjs-Stamm zuerst
`./Aq/Dashboard/CopyToRasPi.sh --sources-only` ausführen. Danach auf dem Pi:

```bash
cd /home/pi/Prjs/Dashboard
make
sudo systemctl restart DashboardServer.service
```

Anschließend Dashboard mit Strg+F5 neu laden. Zeiten, Profile, Gerätezuordnung
und Firmware bleiben bei diesem Quelltransfer erhalten. Lokale Build- und
Loopback-Prüfung ersetzen noch keinen übertragenen Pi-/Tablet-Betrieb.
