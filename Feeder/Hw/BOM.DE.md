# Feeder – Stückliste

[English](BOM.md) · [ODS](BOM.ods) · [Hardwareübersicht](README.DE.md)

| Pos. | Menge | CAD-Objekt | Bezeichnung | Hersteller / Anbieter | Anmerkungen | Bezugsquelle (Beispiele) |
| --- | ---: | --- | --- | --- | --- | --- |
| 1 | 1 | CaseBot | Untergehäuse (3D-Druck, PLA) | Eigenbau |  |  |
| 2 | 1 | Bot | Unterteil der Dosiereinheit (3D-Druck, PLA) | Eigenbau |  |  |
| 3 | 1 | Center | Dosierscheibe (3D-Druck, PLA) | Eigenbau |  |  |
| 4 | 1 | Top | Oberteil mit Futterbehälter (3D-Druck, PLA) | Eigenbau |  |  |
| 5 | 1 | Lid | Deckel des Futterbehälters (3D-Druck, PLA) | Eigenbau |  |  |
| 6 | 1 | Funnel | Auslauftrichter (3D-Druck, PLA) | Eigenbau | *8 |  |
| 7 | 1 | Front | Frontplatte (3D-Druck, PLA) | Eigenbau |  |  |
| 8 | 1 | Food | Positionierfuß für den Feeder (3D-Druck, PLA) | Eigenbau | *8 |  |
| 9 | 1 | FrontBridge | Frontbrücke (3D-Druck, PLA) | Eigenbau |  |  |
| 10 | 1 | WifiKit8 | WiFi Kit 8 (ESP8266 mit OLED) | verschiedene | *1,4 | [Link](https://www.amazon.de/DollaTek-ESP8266-Internet-Development-OLED-Display/dp/B0D1TZL5GY) |
| 11 | 1 | RotEnc | Drehencoder-Modul mit Taster | verschiedene | *1 | [Link](https://www.amazon.de/QWORK-Drehgeber-Encoder-Drehwinkelgeber-Druckknopf/dp/B09R41C1HR) |
| 12 | 1 | DC-Jack | DC-Einbaubuchse für Hohlstecker | verschiedene | *1,5 | [Link](https://www.amazon.de/dp/B0H1BTBRYS) |
| 13 | 1 | ServoPart | Positionsservo SG90 (9 g) | verschiedene | *1 | [Link](https://www.amazon.de/servo-sg90/s?k=servo+sg90) |
| 14 | 1 | Lemo-Socket | Dreipolige Einbaubuchse für DS18B20 (optional) | Lemo | *1,2 | [Link](https://www.amazon.de/DRRI-2-polig-3-polig-4-polig-Rundstecker/dp/B07JWCQJB1) |
| 15 | 4 | Screw M2.5 x 12 | Metrische Schraube M2,5 × 12 mm | verschiedene |  |  |
| 16 | 2 | Screw M2.5 x 5 | Metrische Schraube M2,5 × 5 mm | verschiedene |  |  |
| 17 | 6 | N/A | Gewindeeinsatz M2,5 zum Einschmelzen | verschiedene | *6 | [Link](https://www.amazon.de/s?k=Heat-Inserts) |
| 18 | 2 | Screw M1.6 x 5 | Selbstschneidende Kunststoffschraube Ø1,6×5mm | verschiedene | *7 |  |
| 19 | 1 | N/A | Steckernetzteil 5 V DC / 2 A mit Hohlstecker | verschiedene | *5 | [Link](https://www.amazon.de/dp/B097C4JL7P) |
| 20 | 1 | N/A | DS18B20-Temperaturfühler mit Kabel (optional) | Dallas | *2,3 | [Link](https://www.amazon.de/s?k=DS18B20) |
| 21 | 1 | N/A | Dreipoliger Kabelstecker für DS18B20 (optional) | Lemo | *2,3 | [Link](https://www.amazon.de/DRRI-2-polig-3-polig-4-polig-Rundstecker/dp/B07JWCQJB1) |
| 22 | 1 | N/A | Pull-up-Widerstand 4,7 kΩ für DS18B20 (optional) | verschiedene | *9 |  |
| 23 | 1 | O-Ring | O-Ring 22 × 2,5 mm (Innendurchmesser × Schnurstärke) | verschiedene | *10 |  |
| 24 | 13 | N/A | Einadrige Dupont-Leitung | verschiedene | *11 |  |

## Anmerkungen

1. Bei Verwendung anderer Komponenten müssen die entsprechenden 3D-Druckteile gegebenenfalls angepasst werden.
2. Es kann jede beliebige Kombination aus Stecker und Buchse mit mindestens drei Kontakten verwendet werden.
3. Verwendet wurden Temperaturfühler mit Kabel und eine bereits vorhandene Stecker-Buchsen-Kombination. Bei der Verkabelung ist die korrekte Anschlussbelegung an Stecker, Buchse und Controller sicherzustellen.
4. Je nach Anbieter unterscheiden sich Bauform und Displayposition geringfügig. Mit der bestehenden Frontplatte (Pos. 7) lässt sich ein vertikaler Versatz von bis zu 1 mm ausgleichen; andernfalls siehe Anmerkung 1.
5. Netzteil und Buchse müssen denselben Steckertyp verwenden. Die Verdrahtung der Buchse muss zur Polung des Netzteils passen (üblicherweise Innenkontakt Plus).
6. Vier Gewindeeinsätze im CaseBot und zwei in der FrontBridge nehmen die Schrauben der Positionen 15 bzw. 16 auf.
7. Zur Befestigung des Drehencoder-Moduls.
8. Der Auslauftrichter (Pos. 6) steckt im Aquariumdeckel. Der Positionierfuß (Pos. 8) richtet den Feeder dazu aus. Beide Teile hängen vom jeweiligen Aquarium ab und müssen entsprechend angepasst werden.
9. Für den optionalen DS18B20: Pull-up-Widerstand zwischen Datenleitung und 3,3 V.
10. Nur für FeederV2: O-Ring in Top; konstruiert, noch nicht praktisch getestet. Werkstoff noch festzulegen.
11. Gemäß Verdrahtungsplan: Versorgung 2, Servo 3, Encoder 5, Sensorbuchse 3 Leitungen; ohne Sensoranschluss 10 statt 13. Passende Buchsenkontakte bzw. einseitig angelötet; Länge nach Einbau. Sensorkabel separat.
