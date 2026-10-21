<!-- DeviceIdentity.md, Version: 1.14 -->
# Geräteidentität und Funktionstausch

Die Netzwerkidentität ist ausschließlich STA-MAC plus Dienstnummer:
1 MultiTimer, 2 Dashboard, 3 Feeder, 4 TimerSw. Funktionsnamen bleiben die
Schlüssel für Zeitpläne, Profile und menschenlesbare DevState-Verzeichnisse.
Feste IPs sind Voraussetzung; FritzBox-Namen werden gegebenenfalls separat geändert.
Legacy-IDs und eine erneute MAC-Aktivierung werden nicht mehr unterstützt.

## Gerätetausch

Verbraucher von beiden Geräten trennen, Geräte selbst versorgt lassen.
Nur Feeder untereinander oder TimerSw untereinander tauschen. Dashboard verteilt
die neue Funktion, Zeitpläne und Sources-Zuordnung sowie genau vier Config-Felder:
`Hostname`, `ApSsid`, `ApPw`, `FtpPw`. Alle anderen Config-Felder und die
Hardwarekalibrierung bleiben beim physischen Gerät. Beide betroffenen ESPs starten
neu; Abschluss erfordert die neue AppliedRevision und einen erhöhten BootCounter.
Erst nach bestätigtem Abschluss die Verbraucher passend anschließen.

Ein ausstehender Tausch wird nach Dashboard-Neustart fortgesetzt. Sein privates
PendingOperation-Journal enthält Zugangsdaten, wird mit Dateimodus 0600 gespeichert
und ist über die statische Webauslieferung gesperrt. Öffentliche Registry-Antworten
enthalten diese Zugangsdaten nicht. Die bestehenden Geräte-Config.json-Dateien
bleiben im LAN lesbar, einschließlich Passwörtern. Laut Betreiber ist der
Internetzugang aller ESP-Geräte gesperrt; der Pi ist davon ausgenommen.
Diese Netzvorgabe ersetzt keinen Schutz vor anderen LAN-Teilnehmern.

## Temperaturquellen ohne Dashboard

MyTimer, Nous und Shelly besitzen auf Konfiguration einen Sources-JSON-Editor.
Er bearbeitet ein Objekt Funktion → physische Feeder-ID (`MAC@3`). Bei erreichbarem
Dashboard ist der Editor gesperrt; auch der Server prüft vor jedem Speichern die
Erreichbarkeit. Bei Wiederverbindung ersetzt die zentrale Zuordnung lokale Quellen
auch bei unveränderter Revision. Fehlende/ungültige Temperaturquellen verwenden
NoTempOn; es gibt keinen Rückfall auf frühere Hostnamen.

## PC-Abgleich nach einem Tausch

Dashboard hat keinen Zugriff auf den PC und synchronisiert dessen Dateien nicht.
Der Tauschdialog weist darauf hin, dass zuvor erzeugte FS-Binaries veraltet sind.
Nous- und Shelly-FS-Abbilder sind nicht austauschbar.

Vom Prjs-Stamm ausdrücklich starten:

```sh
python3 Aq/tools/ReadDeviceStates.py
```

Dieser Aufruf liest sämtliche ESP-FS-Inhalte zurück und bewahrt sie unter dem
jeweiligen Projekt in generated/Instances auf. Er verändert keine Geräte und
keine DevState-Daten. Erst der ausdrücklich gewählte Aufruf

```sh
python3 Aq/tools/ReadDeviceStates.py --apply
```

liest erneut alle ESPs und übernimmt Config.json, bei Switches Channels.json und
bei Servo-Feedern ServoSteps.json in DevState/<aktuelle Funktion> im passenden
Hardwareprojekt. Zuerst werden alle persistenten JSON-Dateien verglichen; nur
geänderte Inhalte werden geschrieben. Unveränderte Dateien behalten ihre mtime.
Nur bei nötiger Funktionsordner-Verlagerung werden bestehende Verzeichnisse
wiederherstellbar in den Papierkorb verschoben.
Neue Teilnehmer erhalten frische DevState-Verzeichnisse mit ihren eigenen persistenten Dateien; fremde Hardwaredateien werden nicht übernommen. Lokal nicht mehr zugeordnete Funktionsverzeichnisse werden in Trash verschoben. Die lokale öffentliche Zuordnung einschließlich verfügbarer Gruppenzuordnung wird aktualisiert. Unvollständige Downloads,
ein laufender Tausch oder geänderte Zuordnungen verhindern die Übernahme.
Nach erfolgreichem --apply werden die werkzeugeigenen Rücklese- und
Zwischenkopien direkt gelöscht (einschließlich früherer Läufe). Bei Fehlern
bleiben sie erhalten. Die Ausgabe nennt nur geänderte Quellpfade bzw. keine
Änderung; erfolgreiche Einzel-Downloads bleiben still. Geräte-Webdateien und
zentrale Laufzeit-Caches werden nicht als neue Buildquellen übernommen. WebSrc sowie
zentrale Dummy-Zeitpläne bleiben maßgeblich.

Anschließend im jeweiligen betroffenen Hardwareprojekt `./EspCompile.sh` starten.
Erst neu erzeugte, zum DeviceType passende FS-Abbilder verwenden. Das Werkzeug
baut oder lädt selbst keine FS-Binaries hoch.

## Geräteverwaltung und Prüfstand

Gerätezuordnung enthält die per Button sichtbare Gerätetausch-Box und darin einen separat aufklappbaren JSON-Geräte-Editor. Er bearbeitet Identität, feste Adresse, Hardwarebeschriftung, Gruppe und Aktivstatus als gemeinsame Ansicht. Bestehende Funktion/MAC/Hardwaretyp sowie Pi-Dienste bleiben geschützt. Neue aktive ESPs werden live geprüft; Entfernen ist bei Zeitplan-/Temperaturquellenreferenzen gesperrt. Inaktive Vorbereitungen werden nicht angesprochen. Entfernte Hardware separat außer Betrieb nehmen.

EditorConfiguration mit Devices/HardwareNames liegt atomar im privaten DeviceAssignments.json; statische Devices.json bleibt Ausgangskonfiguration. CopyToRasPi überschreibt diese Laufzeitänderungen nicht. Nach bestätigter Änderung erfolgt der PC-Abgleich weiterhin ausdrücklich mit ReadDeviceStates.py --apply.

Dashboard Server 1.22/Data 2.67 ist lokal gebaut und mit simulierten Teilnehmern geprüft. Installation auf dem Pi und reale Tests des Hinzufügens/Entfernens stehen aus. Der Benutzer hat die bisherigen Geräte-/FS-Uploads und Anzeigeprüfungen bestätigt. Diese Dashboard-Änderung benötigt keinen erneuten ESP-Firmware- oder FS-Upload.

Hostname-Regel beim Gerätetausch: Für konkrete Funktionen wird der neue Funktionsname gesetzt. Für mit UseHardwareName gekennzeichnete Reservefunktionen wird der Hardware-Name der Ziel-MAC gesetzt. Hostname wird daher beim Transfer berechnet und nicht unverändert von der Quellhardware kopiert. Die übrigen drei Funktionsfelder ApSsid/ApPw/FtpPw behalten ihre bisherige Übertragung. Interne Funktionsschlüssel, physische IDs und DevState-Verzeichnisnamen bleiben davon unabhängig.

## HW-Endpunkte ab Dashboard 1.23

Einzelne Schalt- und Temperaturfunktionen liegen in Endpoints. Endpunktadresse MAC@ServiceId/Kanalnummer, Fähigkeiten als Bitmaske (1 Schalten, 2 Temperatur). MT Main 1..8 ergibt feste Namen MultiTimer1@1..8; NS/Feeder-Temperaturgeber verwenden Kanal 0. Tausch nur mit identischen Fähigkeiten. Name, Rolle, Profil, Gruppe, Zeitplan und erforderliches Quellenrouting folgen der Funktion. GPIO, PhysPos, Taster, Polarität, Kalibrierung, Hostname und Zugangsdaten bleiben beim Endpunktgerät.

Der bestehende Geräte-Funktionsschlüssel dient weiterhin als stabiler Transport-/DevState-Schlüssel; die Schaltfunktion wird separat aus Endpoints/Channels ermittelt. Feeder-Gesamtgerätetausch bleibt eine andere Operation. Quellenrouting aus Sensor-Endpunkten überschreibt die entsprechende Standard-Feederzuordnung.

Vor Schaltkanaltausch Verbraucher von beiden Ausgängen trennen. Bei Temperaturgebertausch Anschluss und gewünschte Funktionszuordnung prüfen. Zentraler Journalstand und vollständiger Times-Wunschstand werden zusammen gesichert und nach Neustart weiterverteilt. Die Empfänger übernehmen vollständige Kanäle/Zeiten über ein Redo-Journal, bestätigen erst nach Anwendung. Keine gleichzeitige Umschaltung aller Geräte behaupten; Verbraucher erst nach bestätigtem Abschluss wieder anschließen.

MyTimer 2.56/Web 2.31, Nous 1.16 und Shelly 1.12 benötigen dafür neuen Code, vorhandene NS-FS bleiben für das Firmwareupdate gültig. Nach einem tatsächlichen Tausch müssen die neuen Channels-Zustände vor späterem FS-Upload am PC zurückgelesen und neu gebaut werden. HeatHistory 1.15 folgt den Heizrollen am jeweils tatsächlichen Kanal. Lokale Builds/Mock- und Recoveryprüfungen bestanden; realer Kanaltausch steht noch aus.

## Allgemeiner UDP-Messwert

Der bereinigte Quellstand sendet und empfängt ausschließlich UDP-Version 2,
einschließlich Request-Paketen. Pakete mit Version 1 oder unbekannter Version
werden verworfen. Die Datenformatversionen von Profilen, Registry und Identity
sind davon unabhängig und bleiben unverändert.

- Benannte numerische Messwerte stehen direkt in `Status`. Root-`Value` und
  Root-`Temperature` werden nicht ausgewertet; kein unbenannter Cache-Rückfall.
- `Status.Temperature`: Grad Celsius; `Status.BatteryPercent`: Prozent (0–100).
  Keine automatische Einheitenumrechnung. `null`, fehlende Felder, nicht-endliche
  Zahlen, Strings und boolesche Werte liefern keinen nutzbaren Regelwert.
- `Status.Kind` beschreibt optional den Statusinhalt (beim Laptop `LaptopPower`),
  ersetzt aber nicht die Messwertauswahl. Hardwaretyp/Identität bleiben getrennt.
- Ein Status ist ein vollständiger aktueller Snapshot. Fehlende Messwerte dürfen
  nicht aus einem älteren Paket übernommen werden; Gültigkeitsdauer bleibt 20 s.

Laptop-Sender:

```json
{"Version":2,"Type":"Status","ID":"F4:06:69:2A:E9:CD@3","Status":{"Kind":"LaptopPower","BatteryPercent":63,"AcOnline":true,"BatteryState":"Charging"}}
```

Profile behalten `TempId` als logische Quelle und wählen mit `Measurement` den
konkreten Statuswert, z. B. `BatteryPercent`. Ohne Feld gilt `Temperature`.
Zulässige Feldnamen: 1–64 ASCII-Buchstaben, Ziffern oder Unterstrich; `Kind`
ist reserviert. Keine Punktpfade oder verschachtelte Messwertsuche.

Feeder veröffentlichen `Status.Temperature`, bei ungültiger Messung `null`.
Ein deaktivierter Sensor erzeugt dieses Feld nicht. Bestehende Statusfelder
(einschließlich Measurements für die Anzeige) bleiben erhalten.
Die Rückmeldeüberwachung nutzt unverändert das unabhängige `Status.AcOnline`.

Die Umstellung aller aktiven Sender auf Version 2 und die Profilverteilung sind
auf Hardware bestätigt (35 s, 129 Statuspakete von zehn Sendern). Die danach
entfernte Version-1-Empfangslogik ist zunächst nur eine Quellstand-Bereinigung;
Produktbuilds und erneuter Rollout erfolgen gesondert. Bereits installierte
Empfänger mit dualem Empfang funktionieren weiterhin mit den Version-2-Sendern.
