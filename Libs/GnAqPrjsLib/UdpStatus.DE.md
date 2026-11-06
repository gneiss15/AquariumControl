<!-- UdpStatus.DE.md, Version: 1.01 -->
# UDP-Status und Beschreibungen

[English](UdpStatus.md)

Die Geräte veröffentlichen ihren Status auf UDP-Port **44444**, regulär alle **zehn Sekunden**. Zustandsänderungen, Übersteuerungen, Fehler, Gültigkeitswechsel, Start und Wiederverbindung lösen ebenfalls eine Meldung aus. Veröffentlicht werden die Werte abgeschlossener Messzyklen. Reine Zahlenänderungen erzeugen keine zusätzlichen Broadcasts.

HTTP-Statusabfragen liefern weiterhin das vollständige Format. Ältere unkomprimierte Statussender mit Version 2 werden weiterhin gelesen.

```json
{"Version":2,"Type":"Status","ID":"02:11:22:33:44:55@4","Compact":true,"InfoRevision":"0123456789abcdef","Status":{"BootCounter":7,"Channels":[{"Id":0,"State":true}],"Temperature":25.4}}
```

`ID` ist die physische MAC@ServiceId. Funktionsnamen, Kanaleigenschaften und weitere Beschreibungen werden getrennt aufgelöst. `InfoRevision` kennzeichnet den Inhalt und ist kein Ablaufdatum. Eine unveränderte Beschreibung bleibt gültig; eine geänderte Kennung verwirft den bisherigen Beschreibungsstand dieses Senders.

Der Empfänger fragt die fehlenden Angaben gezielt bei der IP des Senders auf UDP-Port **44500 + ServiceId** an (beispielsweise MultiTimer: 44501):

```json
{"Version":2,"Type":"InfoRequest","ID":"02:11:22:33:44:55@4","InfoRevision":"0123456789abcdef","Fields":["Identity","Channel:0"]}
```

Beschreibungsfelder heißen `Root`, `Identity`, `Channel:<Kanal-Id als JSON>` und `Measurement:<Messwert-Id>`. Antworten enthalten `Type: Info`, dieselbe `ID`/`InfoRevision`, `Field` und `Data` als vollständiges serialisiertes JSON-Objekt. Pro Beschreibungsteil wird ein Datagramm gesendet. Jeder Dienst besitzt einen eigenen Kontrollsocket (44501–44504); unabhängige Beobachter wie HeatHistory nutzen einen dynamischen Antwortport. Dadurch erreichen sowohl Anfragen als auch Antworten die richtige Anwendung, wenn Dashboard und MultiTimer dieselbe IP nutzen. Fehlende Antworten werden mit wachsendem Abstand erneut angefragt. Beschreibungsantworten erneuern weder den Online-Zeitpunkt noch erzeugen sie Messwertstichproben.

Kompakte Kanäle behalten `Id` und `State`. Das optionale `Bits` codiert Eingangszustand (1), aktive Übersteuerung (2), deren Zustand (4), Kalibrierübersteuerung (8), abweichenden Automatikzustand (16), Automatik EIN (32), unbekannten Eingang (64) und unbekannten Automatikzustand (128). `Until` enthält das Ende der Übersteuerung. Empfänger ergänzen daraus vor der fachlichen Verarbeitung wieder das vollständige Kanalobjekt.

## Quellenauflösung ohne laufendes Dashboard

Bestätigte Quellenzuordnungen bleiben auf den Verbrauchern gespeichert. Sie werden nicht durch ihr Alter ungültig. Nur Dashboard ändert die verbindliche Zuordnung; andere Teilnehmer können ihren zuletzt zentral bestätigten Stand beantworten.

```json
{"Version":2,"Type":"SourceRequest","Function":"FeederAq1"}
```

Ist die Quelle unbekannt, kann diese Anfrage als Broadcast gesendet werden. Ein Teilnehmer oder Dashboard antwortet gezielt an IP und Antwortport mit `Type: SourceInfo`, seiner eigenen `ID`, `Function`, `Source` (MAC@3), `Revision` (zentrale Zuordnungsrevision) und `Confirmed: true`. Neuere bestätigte Revisionen ersetzen zwischengespeicherte Angaben. Bei widersprüchlichen Angaben derselben Revision wird nicht geraten; eine vorhandene lokal bestätigte Zuordnung bleibt der Rückfall. Antworten anderer Geräte schreiben keine zentrale Konfiguration und machen diese nicht zu weiteren Konfigurationsinstanzen.

Nach einem Neustart ist die lokal gespeicherte bestätigte Zuordnung sofort nutzbar. Fehlt sie, fragt das Gerät die erreichbaren Teilnehmer. Gibt es keine bestätigte Antwort, bleibt die Quelle unbekannt; das bestehende Verhalten bei fehlenden Messwerten gilt weiter. Ein Backup-Dashboard ist als getrennte Erweiterung vorgemerkt.

## Steuerung und temporäre Diagnose

`Status.HeatControl` ist der dauerhafte Boden-Steuervertrag mit `Schema`, `Mode`, `Profile`, `Config.FloorDevice`, `Config.FloorChannel` und `FloorRequest`. Zeitprüfung, 60-s-Gültigkeit, Zuordnungsprüfung und Ausfallverhalten bleiben von der Diagnose unabhängig. Empfänger bevorzugen diesen Block; `HeatPi` dient nur bei fehlendem `HeatControl` als Übergangs-Rückfall. Explizites null verwirft die Anforderung.

`GN_DEBUG_HEAT_PI` schaltet den derzeit für HeatHistory benötigten Beobachtungsblock `Status.HeatPi`. `GN_DEBUG_CHARGE_LEARNING` schaltet das wiederholte letzte RAM-Lernereignis `Status.ChargeLearning` und das Dashboard-Journal. Beide Schalter in `GnTemporaryDiagnostics.h` stehen zunächst auf 1. Abschalten darf weder Steuerung noch Lernen ändern.
