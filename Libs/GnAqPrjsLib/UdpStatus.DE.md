<!-- UdpStatus.DE.md, Version: 1.02 -->
# UDP-Status und Beschreibungen

[English](UdpStatus.md)

Die Geräte veröffentlichen ihren Status auf UDP-Port **44444**, regulär alle **zehn Sekunden**. Zustandsänderungen, Übersteuerungen, Fehler, Gültigkeitswechsel, Start und Wiederverbindung lösen ebenfalls eine Meldung aus. Veröffentlicht werden die Werte abgeschlossener Messzyklen. Reine Zahlenänderungen erzeugen keine zusätzlichen Broadcasts.

UDP-Nachrichten verwenden Protokollversion **3**. `Type` ist ein Wert aus `TMsgType`. Nachrichtenfelder und ihre kompakte Statusdarstellung sind in `Definitions/Contract/TUdp*.inc.h` definiert.

```json
{"Ver":3,"Type":2,"ID":"02:11:22:33:44:55@4","Compact":true,"InfoRev":"0123456789abcdef","Status":{"BootCnt":7,"Chs":[{"Id":"02:11:22:33:44:55@4/0","State":true}]}}
```

`ID` bezeichnet eine Dienstinstanz. `InfoRev` ist der Fingerabdruck der zugehörigen Beschreibungsfelder. Er ist kein Ablaufdatum: Unveränderte Beschreibungen bleiben gültig, bis eine andere Revision eintrifft.

Der Empfänger fragt die fehlenden Angaben gezielt bei der IP des Senders auf UDP-Port **44500 + ServiceId** an (beispielsweise MultiTimer: 44501):

```json
{"Ver":3,"Type":3,"ID":"02:11:22:33:44:55@4","InfoRev":"0123456789abcdef","Fields":["Root","Channel:02:11:22:33:44:55@4/0"]}
```

Felder mit `Live` werden in das Status-Datagramm geschrieben. `Info`-Felder werden als angeforderte Beschreibungsteile geliefert: `Root`, `Identity`, `Channel:<Kanaladresse>` und `Measurement:<Messwertart>`. `Omit`-Felder werden im kompakten Status nicht gesendet. Einige davon gibt es über HTTP; als `Omit` markierte Kanalzustände werden stattdessen in `Bits` übertragen.

Eine `Info`-Antwort enthält in `Data` genau ein vollständiges beschreibendes JSON-Objekt mit derselben `ID`, `InfoRev` und angeforderten `Field`-Kennung. Ein Datagramm enthält einen Teil. Jeder Dienst hat einen eigenen Kontrollsocket (44501–44504); unabhängige Beobachter wie HeatHistory verwenden einen temporären Antwortport. Fehlende Antworten werden erneut angefragt. Info-Verkehr aktualisiert weder das Online-Alter noch erzeugt er Messwertstichproben.

Die fünf `JD_BIT`-Einträge in `SharedTypes.inc.h` bestimmen die Kanalbits in dieser Reihenfolge: `InState`, `AutomaticState`, `OvrActive`, `OvrState` und `CalOvrActive`. Zu jedem Feld kommt zuerst das Wertbit und direkt danach das Gültigkeitsbit `<Feldname>Msk`. Ist die Maske gelöscht, behält der Empfänger seinen bisherigen Wert. Ist sie gesetzt, übernimmt er das Wertbit, auch wenn es `false` ist. Die fünf Paare belegen 10 der 16 Bits in `Bits`. Diese Merge-Regel gilt einheitlich; feldspezifische Nachbearbeitung gibt es nicht.

`State` und `OvrUntil` sind normale `Live`-Felder. Kanalname, Hardwareeigenschaften, nächste Änderung und Zustandsbeschriftungen sind beschreibende `Info`-Felder. `PowerMeter.Diag` und `HeatDiag.ActualMainOn` sind `Omit`; HTTP-Status bleibt die Quelle für HTTP-spezifische Leistungsdiagnosen. `HeatDiag.Cfg` ist in das PI-Diagnoseobjekt eingebettet und kein eigenes Datagramm. Der kompakte Writer berücksichtigt auch Feldteile in der eingebetteten Struktur; dort kommen nur `Live`-Felder aus `TJsonHeatConfig` an.

## Quellenauflösung ohne laufendes Dashboard

Bestätigte Quellenzuordnungen bleiben auf den Verbrauchern gespeichert. Sie werden nicht durch ihr Alter ungültig. Nur Dashboard ändert die verbindliche Zuordnung; andere Teilnehmer können ihren zuletzt zentral bestätigten Stand beantworten.

```json
{"Ver":3,"Type":5,"Func":"FeederAq1"}
```

Ist die Quelle unbekannt, kann die Anfrage als Broadcast gesendet werden. Die Antwort geht an IP und Antwortport des Anfragenden:

```json
{"Ver":3,"Type":6,"ID":"02:11:22:33:44:55@4","Func":"FeederAq1","Src":"02:11:22:33:44:66@3","Rev":"2026-11-01T12:00:00.000Z"}
```

Neuere bestätigte Revisionen ersetzen zwischengespeicherte Werte. Widersprüchliche Angaben derselben Revision bleiben ungelöst; eine vorhandene lokal bestätigte Zuordnung bleibt der Rückfall. Antworten anderer Geräte ändern keine zentrale Konfiguration. Gibt es keine bestätigte Zuordnung, bleibt die Quelle unbekannt und das bestehende Verhalten bei fehlenden Messwerten gilt.

Nach einem Neustart ist die lokal gespeicherte bestätigte Zuordnung sofort nutzbar. Fehlt sie, fragt das Gerät die erreichbaren Teilnehmer. Gibt es keine bestätigte Antwort, bleibt die Quelle unbekannt; das bestehende Verhalten bei fehlenden Messwerten gilt weiter. Ein Backup-Dashboard ist als getrennte Erweiterung vorgemerkt.

## Steuerung und temporäre Diagnose

`Status.HeatControl` ist der dauerhafte Boden-Steuervertrag. `Status.HeatDiag` enthält die PI-Beobachtungsdaten für HeatHistory; `Status.ChargeLearn` überträgt das letzte Lade-Lernereignis. Compile-Time-Diagnoseschalter steuern die Veröffentlichung dieser Diagnosefelder, nicht die Heizungsregelung oder das Lernen.
