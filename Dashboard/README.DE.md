<!-- README.DE.md, Version: 2.16 -->
# Aquarium Dashboard

[English](README.md)

Dashboard ist die gemeinsame Übersicht und Bedienoberfläche für die Aquarium-Geräte. Die Übersicht gruppiert Schaltausgänge und Messwerte nach Aquarium. Gemeinsam genutzte Ausgänge können in mehreren Aquariumgruppen erscheinen.

Die Geräte senden aktuelle Zustände und Messwerte wie Temperatur, Spannung, Strom und Leistung per UDP-Broadcast. Der DashboardServer empfängt die Daten und übermittelt Aktualisierungen direkt an die Browser. Die Browser müssen die Messwerte daher nicht regelmäßig bei den Geräten abrufen.

## Hardware und Dokumentationsstand

Die Programme Dashboard und MultiTimer laufen auf demselben Raspberry Pi. MultiTimer steuert die angeschlossene achtfach-Relaiskarte. Fotos und die Beschreibung von Relaiskarte, Netzteil und Gehäuse stehen unter [MultiTimer – Hardware](../MultiTimer/Hw/README.DE.md).

**Coming soon: Die Hardwarebeschreibung wird noch vervollständigt.**

Gerätespezifische Konfigurationen, Zugangsdaten und Laufzeitdaten sind nicht im öffentlichen Repository enthalten. Eigene Konfigurationen und Signierdaten werden wie unter [Build und private Konfiguration](../BUILD-SECRETS.md) beschrieben bereitgestellt. **Coming soon:** eine vollständige Erstaufbau-Anleitung und allgemeine Konfigurationsbeispiele.

## Übersicht und Bedienung

Die Bereichsauswahl öffnet die Übersicht, die Zeitpläne und die Konfigurationsseiten der Geräte. Der zuletzt gewählte Bereich wird im Browser gespeichert und nach dem Neuladen geöffnet. Ist er nicht verfügbar, erscheint die Übersicht. Die Sprache des Dashboards wird unabhängig von den eingebetteten Geräteseiten gewählt.

In der Übersicht stehen Geräte mit ihrem Verbindungsstatus sowie die zugeordneten Messwerte und Ausgänge. Ein nicht erreichbares Gerät wird als **Offline** angezeigt; veraltete Messwerte werden ausgeblendet. Die Gruppe **System** enthält gemeinsame Geräte und Statusanzeigen, beispielsweise den Raspberry Pi und einen registrierten Laptop.

Die Darstellung passt sich an PC, Tablet und Smartphone an. Auf schmalen Bildschirmen erscheinen Zeitpläne als Karten. Gerätekonfigurationen werden in die verfügbare Fläche eingepasst; bei Erreichen der Mindestgröße ist horizontales Scrollen möglich. Skalierung und Neuladen lassen sich neben der Bereichsauswahl bedienen.

### Schaltausgänge bedienen

- Auf **EIN** oder **AUS** tippen: die kurze Tasteraktion des Geräts auslösen.
- Auf den Kanalnamen tippen: die Detailansicht öffnen.
- In der Detailansicht sind kurze und, sofern vom Gerät unterstützt, lange Tasteraktionen verfügbar.
- Eine lange Tasteraktion muss innerhalb von vier Sekunden ein zweites Mal bestätigt werden.

Die Aktion entspricht der Bedienung am Gerät. Was ein kurzer oder langer Tastendruck bewirkt, beschreibt die jeweilige Geräteanleitung.

### Tabellen bedienen

Ein Klick auf eine Spaltenüberschrift sortiert aufsteigend oder absteigend. Die Sortierung verändert die gespeicherte Reihenfolge nicht. In der Zeitentabelle stellt **#** die manuelle Reihenfolge wieder her.

Spalten passen ihre Breite an den Inhalt an. Eine Spaltengrenze lässt sich ziehen; ein Doppelklick stellt die automatische Breite wieder her. Per Tastatur ändern Links/Rechts die Breite, Enter stellt die Automatik wieder her. Manuelle Breiten werden im Browser gespeichert.

## Zeitpläne und Profile

Auf der Seite **Zeiten** werden Schaltzeiten, Wochentage, Kanäle, Zustände und Intervalle bearbeitet. Fütterungszeiten werden ebenfalls zentral verwaltet. Änderungen werden an die betroffenen Geräte verteilt. Nicht erreichbare Geräte erhalten ausstehende Änderungen, sobald die Verbindung wiederhergestellt ist.

Die Seite **Heizprofile** verwaltet die zentralen Profile und deren Zuordnung zu Gerätekanälen. Ein Profil kann erst gelöscht werden, wenn kein Kanal darauf verweist. Rollen und Profile lassen sich ändern, ohne die Hardwarebelegung des Geräts zu verändern.

Ein Profil enthält unter anderem:

- **Quelle:** der Geber des Messwerts, beispielsweise ein Temperaturgeber oder `LaptopBattery`.
- **Messwertfeld:** das auszuwertende Feld, beispielsweise `Temperature` oder `BatteryPercent`. Ein leeres Feld bedeutet `Temperature`.
- Die zum Profil gehörenden Schaltgrenzen und Einstellungen für fehlende Messwerte.

Quelle und Messwertfeld sind unterschiedliche Angaben: `LaptopBattery` benennt den Sender, `BatteryPercent` dessen Akkustand. Das Messwertfeld gehört zum Profil, nicht zur Kanalzuordnung. Nach dem Bearbeiten **Speichern / verteilen** betätigen.

Der Revisionszeitstempel bezeichnet die letzte Profiländerung. Er ist keine laufende Uhr und wird neutral dargestellt. Synchronisierte Teilnehmer sind grün; ausstehende Synchronisierungen und Fehler werden rot angezeigt. Hinweise zum Speichern und Verteilen verschwinden nach erfolgreichem Abschluss.

Die zentrale Datenhaltung liegt auf dem Raspberry Pi. Jedes Gerät erhält seine eigenen Zeitpläne und Kanalzuordnungen zusammen mit den zentralen Profilen. Änderungen an einer Kanalzuordnung betreffen nur das zugehörige Gerät. Änderungen an zentralen Profilen werden an alle beteiligten Schaltgeräte verteilt. Unverändertes Speichern löst keine erneute Verteilung aus.

## Geräte konfigurieren

`Devices.json` enthält die Ausgangskonfiguration für Geräte, Adressen und Darstellung. Für eine eigene Installation müssen die Adressen und Zuordnungen angepasst werden.

| Einstellung | Bedeutung |
| --- | --- |
| `Name` | Anzeigename des Geräts. |
| `BaseUrl` | Geräteadresse einschließlich eines gegebenenfalls benötigten Ports. |
| `Group` | Aquariumgruppe oder `System` für gemeinsame Anzeigen. |
| `GroupViews` | Gefilterte Ansichten eines Geräts in mehreren Aquariumgruppen. |
| `OverviewOutputOrder` | Reihenfolge der Ausgänge und Messwerte je Aquarium. Fehlende vorgesehene Einträge behalten eine Leerzeile. |
| `StatusOnly` | Reine Statusanzeige ohne Konfigurationsseite und Kanalbedienung. |
| `ConfigUrl` | Pfad zur Konfigurationsseite des Geräts. |
| `FrameWidth`, `MinimumScale` | Darstellungsbreite und kleinste automatische Skalierung eingebetteter Geräteseiten. |
| `PowerLabel` | Beschriftung der Leistungsanzeige, beispielsweise für Licht oder Heizung. |
| `Enabled` | Mit `false` einen Eintrag deaktivieren. |
| `SyncOnlyDevices` | Geräte an der Profilverteilung beteiligen, ohne eine Übersichtskarte anzuzeigen. |

Gruppenfilter und Ausgabereihenfolge verwenden die technischen Kanalnamen aus dem Statusfeld `Name`. Aktionen verwenden die numerische Kanal-ID. Ein Ausgang wie die gemeinsame Luftversorgung kann in mehreren Aquariumgruppen angezeigt werden.

`HardwareNames` ordnet MAC-Adressen den Beschriftungen der physischen Geräte zu. Bei Reservefunktionen mit `UseHardwareName` erscheint diese Gerätebeschriftung; fest zugeordnete Aquariumfunktionen zeigen ihren Funktionsnamen.

## Geräteidentität und Funktionstausch

Jedes physische Gerät wird anhand seiner MAC-Adresse und Dienstkennung erkannt. Die Zuordnung zu einer Funktion, etwa Fütterung oder Beleuchtung, lässt sich im Bereich **Gerätetausch** ändern. Zeitpläne und Kanalrollen bleiben dabei der jeweiligen Funktion zugeordnet.

**Vor einem Tausch die angeschlossenen Verbraucher von beiden betroffenen Geräten trennen; die Steuergeräte eingeschaltet lassen.** Ein Gerät kann nur durch ein funktionskompatibles Gerät ersetzt werden.

Beim Tausch ganzer Geräte werden Funktionshostname und AP-/FTP-Zugangsdaten der Funktion zugeordnet. Hardwareeinstellungen bleiben am jeweiligen Gerät. Beide betroffenen ESPs starten neu. Der Vorgang ist abgeschlossen, wenn beide Geräte die Übernahme bestätigt haben. Unterbrochene Vorgänge werden nach einem Neustart fortgesetzt.

Weitere Einzelheiten stehen in [Geräteidentität und Funktionstausch](../Libs/GnBasicLib/DeviceIdentity.md).

### Hardware-Endpunkte und Kanaltausch

Ein Hardware-Endpunkt ist ein einzelner Schaltausgang oder Temperaturgeber. Im Bereich **Gerätetausch** lassen sich die Funktionen zweier funktionskompatibler Endpunkte tauschen, beispielsweise zwischen zwei MultiTimer-Ausgängen oder zwischen einem MultiTimer-Ausgang und einer Nous-/Shelly-Steckdose. Schaltausgänge können nur gegen Schaltausgänge und Temperaturgeber nur gegen Temperaturgeber getauscht werden.

Die Tabelle zeigt die Endpunkte mit ihrer aktuellen Funktion. Bei MultiTimer entsprechen die Ausgangsnummern den Steckdosen 1 bis 8. Fütterung und Temperaturmessung eines Feeders sind getrennte Funktionen; beim Tausch eines Temperaturgebers bleiben die Fütterungspläne unverändert.

Nach dem Trennen der Verbraucher die beiden Endpunkte auswählen und den Tausch bestätigen. Während der Übernahme sind weitere Zuordnungsänderungen sowie Änderungen an zentralen Zeiten und Profilen gesperrt. Der Vorgang ist abgeschlossen, wenn alle beteiligten Geräte die neue Zuordnung bestätigt haben.

Funktionsname, Rolle, Profil, Schaltzeiten und Intervalle werden der neuen Hardware zugeordnet. Aquariumgruppen und Verweise auf Temperaturquellen folgen der Funktion. Hardwareeinstellungen, Kalibrierung, Gerätehostname und Zugangsdaten bleiben beim jeweiligen Gerät. Eine vorübergehende manuelle Übersteuerung wird beim Funktionswechsel aufgehoben; der Ausgangszustand wird anhand der neuen Funktion berechnet. HeatHistory erfasst Heizfunktionen am jeweils zugeordneten Gerätekanal.

Nach abgeschlossenem Tausch die MultiTimer-Seite öffnen oder neu laden. Beim Wechsel zur Seite oder beim erneuten Aktivieren prüft sie die Konfiguration und lädt geänderte Belegungen und Zeiten nach. Ungespeicherte Eingaben bleiben erhalten und werden mit einem Änderungshinweis versehen.

### PC-Daten nach einem Tausch abgleichen

Ein Funktionstausch allein erfordert keinen Dateisystem-Upload. Soll später ein Dateisystem neu aufgespielt werden, müssen vor dem Erstellen des Abbilds die PC-Daten mit der aktuellen Gerätezuordnung abgeglichen werden.

Den folgenden Befehl im Stammverzeichnis des AquariumControl-Repositories ausführen:

```bash
python3 tools/ReadDeviceStates.py --apply
```

Dashboard selbst hat keinen Zugriff auf die PC-Dateien. Bereits erzeugte Dateisystem-Abbilder können noch die Zuordnung vor dem Tausch enthalten.

### Geräte-Editor

Unter **Gerätezuordnung → Gerätetausch → Geräte-Editor** öffnet sich der JSON-Editor zum Hinzufügen und Entfernen von Geräten sowie zum Bearbeiten ihrer Metadaten. Er enthält unter anderem Funktion, MAC-Adresse, IP-Adresse, Port, Geräteart, Hardwarebezeichnung, Aquariumgruppe und Aktivierungsstatus.

Bestehende Geräteidentitäten, Funktionen und Hardwaretypen sind geschützt; Funktionswechsel erfolgen über den Tauschdialog. Die Pi-Dienste können im Geräte-Editor nicht verändert werden. Ungültige oder doppelte Identitäten und Änderungen während einer ausstehenden Zuordnung werden abgewiesen.

Neue aktive Geräte benötigen passende Firmware, ein eingerichtetes Dateisystem und eine feste IP-Adresse. Der Server prüft ihre tatsächliche Identität und Geräteart. Mit `Active: false` lassen sich Einträge vorbereiten, ohne das Gerät anzusprechen.

Entfernen oder Deaktivieren ist erst möglich, wenn keine Zeitpläne oder Temperaturquellen-/Profilzuordnungen das Gerät verwenden. Ein entferntes Gerät muss separat außer Betrieb genommen werden; das Entfernen aus der Liste löscht keine Daten auf dem Gerät.

Die laufende Konfiguration wird in `data/DeviceAssignments.json` gespeichert und beim Quelltransfer erhalten. `Devices.json` liefert die Ausgangswerte.

## Externe Messquellen und Laptop-Anzeige

Zusätzliche Sender ohne Geräte-/Konfigurations-API werden unter `ExternalSources` in `data/DeviceAssignments.json` eingetragen. Der Eintrag ordnet einen Funktionsnamen einer Identität der Form `MAC@3` zu. Nur registrierte Sender werden ausgewertet.

Änderungen an dieser Tabelle bei gestopptem Dashboard durchführen. Sie benötigen eine neue Zuordnungsrevision und Verteilung an die beteiligten Geräte; der Geräte-Editor bearbeitet diese Tabelle nicht. Einzelheiten zum Datenformat stehen in [DeviceIdentity.md](../Libs/GnBasicLib/DeviceIdentity.md).

Der Laptop-Akku-Sender läuft auf dem Laptop und sendet den Akkustand als `Status.BatteryPercent` in Prozent. Zusätzlich meldet er mit `Status.AcOnline` die Netzversorgung und mit `Status.BatteryState` den Akkuzustand. Ein registrierter Sender `LaptopBattery` erscheint unter **System** als Laptop. Ungültige Prozentwerte werden als „—“ dargestellt. Nach 20 Sekunden ohne gültiges Statuspaket gilt der Sender als Offline; veraltete Messwerte werden ausgeblendet.

Ein Ladeprofil kann beispielsweise die Quelle `LaptopBattery`, das Messwertfeld `BatteryPercent` und die Grenzen 40/80 verwenden. Bei `NoTempOn: false` bleibt der zugeordnete Ausgang ohne gültigen Messwert AUS. Der zugehörige Schaltausgang `LaptopSw` wird als eigene Funktion angezeigt.

## Rückmeldeüberwachung und Warnton

Die Rückmeldeüberwachung vergleicht den gemeldeten Schaltzustand mit einer unabhängigen Rückmeldung. Sie schaltet selbst keine Ausgänge. Die Regeln werden unter **Gerätezuordnung → Rückmeldeüberwachung** im JSON-Editor bearbeitet.

| Feld | Bedeutung |
| --- | --- |
| `SwitchFunction` | Zu überwachende Schaltfunktion. |
| `Source` | Unabhängiger Rückmeldegeber. |
| `StatusKind` | Kennung der erwarteten Statusmeldung. |
| `FeedbackField` | Feld mit einer booleschen Rückmeldung. |
| `GraceSeconds` | Zulässige Abweichungsdauer von 1 bis 3600 Sekunden. |

Beispielsweise kann `LaptopSw` mit der Netzversorgungsrückmeldung `Status.AcOnline` von `LaptopBattery` verglichen werden. `StatusKind` ist dabei `LaptopPower`; eine mögliche Toleranz beträgt 60 Sekunden. Ob der Akku gerade lädt, genügt nicht als Rückmeldung zur Netzversorgung, da ein Ladelimit das Laden verhindern kann.

Nur durchgehend unterschiedliche, gültige und höchstens 20 Sekunden alte Werte führen nach der Toleranz zum Fehler. Übereinstimmung, Richtungswechsel, fehlende oder ungültige Werte und eine neue Zuordnung setzen die Wartezeit zurück. Der Monitor berücksichtigt auch manuelle Übersteuerungen des Ausgangs und folgt einem Kanaltausch.

Die Anzeige unterscheidet **Zustand bestätigt**, **Zustand wird geprüft**, **Zustand nicht bestätigt**, **Rückmeldung fehlt** und **Rückmeldung inkorrekt**. Fehlende oder unbrauchbare Rückmeldungen werden nicht als AUS gewertet und lösen keinen Abweichungs-Warnton aus.

Die Regeln werden in `data/FeedbackMonitors.json` gespeichert. Der Editor erkennt zwischenzeitliche Änderungen. Die Datei `FeedbackMonitors.json` im Projektstamm dient als Ausgangskonfiguration, solange keine Laufzeitdatei vorhanden ist.

### Warnton bedienen

**Warnton aktivieren** gibt die Audioausgabe im Browser frei und spielt einen Testton. Bei einer bestätigten, unquittierten Abweichung erklingen drei kurze Töne alle 30 Sekunden. **Warnton quittieren** lässt die optische Meldung bestehen und unterdrückt den Ton bis zur Erholung oder einer neuen Störungsart. **Warnton ausschalten** deaktiviert die Tonfunktion in diesem Browser. Ohne aktuelle Dashboard-Daten wird der Ton nicht wiederholt.

Die Ein-/Auswahl wird für ein Jahr im Browser gespeichert. Bei gespeichertem Ein versucht die Seite nach dem Laden, die Audioausgabe still wieder aufzunehmen. Blockiert der Browser dies, erscheint **Warnton freigeben**; einmal antippen. Aus bleibt nach dem Neuladen aus. Lautstärke und Audiofreigabe werden am Tablet bzw. im Browser eingestellt. Gelöschte Cookies oder private Sitzungen können die gespeicherte Auswahl verwerfen.

Die Cookie-Laufzeit wird beim Laden und täglich geprüft. Bei höchstens 30 Tagen Restlaufzeit wird die vorhandene Auswahl um ein Jahr verlängert. Eine automatische Wiederaufnahme spielt keinen Testton; ausdrückliches Aktivieren oder Freigeben spielt den Testton.

**Prüfung am eigenen Gerät:** Ton aktivieren, Testton prüfen, Seite neu laden und die Audiofreigabe kontrollieren. Danach ausschalten und erneut laden; die Seite muss stumm bleiben.

## Live-Daten und Verbindungsstatus

Der Datenweg ist **Gerät → UDP → DashboardServer → SSE → Browser**. SSE ist eine dauerhafte HTTP-Verbindung, über die der Server Änderungen direkt an die Browserseite sendet.

Die Geräte veröffentlichen ihren Status spätestens alle fünf Sekunden und zusätzlich bei relevanten Änderungen auf UDP-Port 44444. Eine gemeinsame Verbindung über `/api/events` versorgt die Browserseite mit den Statusdaten aller Geräte und externen Messsender. Der Server fordert fehlende oder veraltete Gerätedaten gezielt an.

Bei einer unterbrochenen Push-Verbindung fragt der Browser den Statuscache des DashboardServers einmal pro Sekunde per HTTP ab. Die Push-Verbindung wird automatisch wiederhergestellt; eine 15 Sekunden lang stumme Verbindung wird neu aufgebaut. Nach Wiederverbindung erhält der Browser den aktuellen Datenbestand. Mehr als 20 Sekunden alte Gerätedaten werden als Offline behandelt.

Bei WLAN- oder Serverausfall zeigt das Dashboard eine Statusleiste mit automatischem Wiederholungsversuch und einem Reload-Button.
Beim Aufwachen oder erneuten Sichtbarwerden wird die Verbindung sofort geprüft. Geänderte Gerätekonfigurationen werden beim Neuladen berücksichtigt.

Die Gerätekommunikation verwendet UDP-Version 2. Messwerte stehen als benannte Felder im Objekt `Status`, beispielsweise `Temperature` oder `BatteryPercent`. Die technischen Schnittstellen sind in [DeviceIdentity.md](../Libs/GnBasicLib/DeviceIdentity.md) beschrieben.

### Raspberry-Pi-Systemstatus

Der Pi meldet CPU-Temperatur und die Systemlast für 1, 5 und 15 Minuten. Außerdem zeigt Dashboard Unterspannung, Taktbegrenzung, Drosselung und Temperaturgrenzen aus `vcgencmd get_throttled` an. Dabei werden sowohl aktuelle als auch seit dem Start aufgetretene Zustände berücksichtigt. Ein Fehlerblock erscheint nur bei entsprechenden Meldungen.

## Übertragung und Datensicherung

`CopyToRasPi.sh` liest vor einer Übertragung den vollständigen Pi-Datenbestand zurück. Das Skript in einem interaktiven Terminal vom Repository-Stamm starten:

```bash
./Dashboard/CopyToRasPi.sh
```

Das Standardziel ist `pi@gn-pi:/home/pi/Prjs/Dashboard/`. Für andere Ziele `RASPI_HOST` und `RASPI_PRJS_DIR` setzen. Die lokalen Projektpfade ermittelt das Skript aus seinem Speicherort.

Verglichen werden ausschließlich `Times.json` und `HeatProfiles.json` mit Beyond Compare (BCO): links der lokale Build-Stand, rechts der Pi-Download. Gewünschte Änderungen vom Pi auf die linke Seite übernehmen. Nach dem Schließen von BCO und Enter werden die bearbeiteten Dateien geprüft und nach lokal `data/` zurückgeschrieben. Neuere Pi-Zeitstempel können automatisch übernommen werden, wenn der zugehörige Inhalt ansonsten identisch ist.

Vor dem Übernehmen wird der lokale Datenstand gesichert; die drei neuesten Sicherungen bleiben erhalten. Anschließend fragt das Skript ausdrücklich nach der Upload-Freigabe. Fehler beim Download oder bei der Validierung stoppen den Upload. Bei Ablehnung bleiben die Pi-Dateien unverändert; bestätigte lokale Änderungen bleiben lokal erhalten.

Der vollständige Transfer überträgt Quellen, Webdateien, `Times.json`, `HeatProfiles.json` und die erzeugte `Version.json`. Die laufenden Gerätezuordnungen und Pi-eigenen Laufzeitdateien bleiben erhalten. Das lokal kompilierte Serverprogramm wird nicht übertragen.

Für reine Quell-/Webänderungen bei unveränderten zentralen Zeiten und Profilen:

```bash
./Dashboard/CopyToRasPi.sh --sources-only
```

Auch dabei wird zuerst ein Pi-Snapshot eingelesen. Der Vergleich und Upload der zentralen Zeiten und Profile entfallen. Nach Webänderungen den Browser neu laden; bei einer veralteten Ansicht mit Strg+F5.

| Verzeichnis | Inhalt |
| --- | --- |
| `data/` | Lokaler zentraler Datenstand. |
| `generated/raspi/` | Vollständige unveränderte Rücklesekopie der Pi-Daten. |
| `generated/build-readable/` | Lesbare lokale Vergleichskopien für BCO. |
| `generated/raspi-readable/` | Lesbare Pi-Kopien von `Times.json` und `HeatProfiles.json`. |
| `generated/data-before-sync/` | Sicherungen des lokalen Datenstands vor der Übernahme. |

Profil-Dateien lassen sich im Dashboard-Verzeichnis separat prüfen:

```bash
./tools/ValidateHeatProfiles.py data/HeatProfiles.json
```

## Server bauen und starten

Benötigt werden Python 3.10 oder neuer, ein C++20-Compiler, Make und die Entwicklungsbibliothek von json-c. Auf dem Raspberry Pi im Dashboard-Verzeichnis:

```bash
make
./DashboardServer 8090 .
```

Im Browser `http://PI-HOST:8090/` öffnen und `PI-HOST` durch Hostname oder Adresse des Pi ersetzen. Dashboard und eingebettete Geräteseiten über HTTP betreiben, da Browser unsichere HTTP-Inhalte innerhalb einer HTTPS-Seite blockieren können.

`Versions.txt` ist die Versionsquelle. Der Build erzeugt `Version.inc.h` und `data/Version.json`. Änderungen an Serverquellen oder der Serverversion erfordern einen Neubau und Dienstneustart. Eine Änderung der Data-Version wird über `data/Version.json` ohne Serverneubau wirksam. Die Kopfzeile zeigt Server- und Data-Version.

### systemd-Dienst

Der Standardpfad auf dem Pi ist `/home/pi/Prjs/Dashboard`. Bei abweichendem übergeordneten Verzeichnis `RASPI_PRJS_DIR` setzen. Vor der Dienstinstallation das Programm bauen:

```bash
export RASPI_PRJS_DIR="${RASPI_PRJS_DIR:-/home/pi/Prjs}"
cd "$RASPI_PRJS_DIR/Dashboard"
make
sudo ./Install.sh
```

Der Dienst heißt `DashboardServer.service`. Status und Logausgabe:

```bash
systemctl status DashboardServer.service
journalctl -u DashboardServer.service -f
```

Nach Änderungen am Server auf dem Pi neu bauen und den Dienst neu starten:

```bash
make
sudo systemctl restart DashboardServer.service
```

## Wichtige Dateien

| Datei | Aufgabe |
| --- | --- |
| `Devices.json` | Ausgangskonfiguration für Geräte und Darstellung. |
| `data/DeviceAssignments.json` | Laufende Geräte-/Funktionszuordnung, Endpunkte und Editor-Einstellungen. |
| `data/Times.json` | Zentrale Zeitpläne, Intervalle und Kanalzuordnungen. |
| `data/HeatProfiles.json` | Zentrale Profile mit Revisionszeitstempel. |
| `data/HeatProfiles.sync.json` | Laufender Synchronisationsstatus. |
| `data/FeedbackMonitors.json` | Regeln der Rückmeldeüberwachung. |
| `data/Version.json` | Erzeugte Data-Version. |
| `data/Layouts.json` | Auf dem Pi erhaltene Laufzeitdatei; die Oberfläche verwendet sie nicht. |
| `index.html`, `Index.css`, `Dashboard.js` | Browseroberfläche. |
| `StatusClient.js` | Geräteinitialisierung und Empfang der laufenden Statusdaten. |
| `Translations.json` | Englische und deutsche Oberflächentexte. |
| `DashboardServer.cpp` und weitere C++-Dateien | Webserver, Datenhaltung, Zuordnungen und Überwachung. |
| `CopyToRasPi.sh` | Abgleich, Rücklesekopie und Übertragung zum Pi. |
| `Install.sh`, `DashboardServer.service` | Installation und Betrieb als systemd-Dienst. |
