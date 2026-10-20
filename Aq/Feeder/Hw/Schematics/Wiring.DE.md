# Feeder – Anschlussliste

[English](Wiring.md) · [Hardware](../README.DE.md)

![Wiring](FeederWiring.DE.png)

| Leitung | WiFi Kit 8 | Gegenstelle |
| ---: | --- | --- |
| 1 | 5V | DC +5 V |
| 2 | GND | DC GND |
| 3 | 5V | SG90 +5 V |
| 4 | GND | SG90 GND |
| 5 | GPIO0 / D3 | SG90 PWM |
| 6 | 3V3 | Encoder VCC / + |
| 7 | GND | Encoder GND |
| 8 | GPIO13 / D7 | Encoder DT |
| 9 | GPIO12 / D6 | Encoder CLK |
| 10 | GPIO14 / D5 | Encoder SW |
| 11 | 3V3 | DS18B20 VDD |
| 12 | GND | DS18B20 GND |
| 13 | GPIO2 / D4 | DS18B20 DQ |

Leitungen 11–13 sind nur für den optionalen Temperaturfühler erforderlich. Die drei Signale führen über die Sensorbuchse und den passenden Kabelstecker zum DS18B20; eine verbindliche Kontaktnummerierung der Stecker ist noch zu ergänzen. Nicht nach Aderfarbe allein anschließen.

Der Widerstand 4,7 kΩ wird direkt an der Buchse zwischen 3,3 V und DQ angelötet und benötigt keine eigene Dupont-Leitung. Die Kabel sind direkt auf die Stiftleisten gesteckt; an den Buchsen und am Servo sind sie teilweise einseitig angelötet. Die erforderlichen Kabellängen am Einbau bestimmen.

Das OLED ist bereits auf dem Modul angeschlossen. GPIO4, GPIO5 und GPIO16 bleiben dafür reserviert. GPIO2 wird für OneWire verwendet; GPIO15/D8 ist dafür ungeeignet. Encoder und DS18B20 verwenden 3,3 V, der Servo 5 V. Alle Baugruppen haben gemeinsame Masse.

Die Hohlbuchse gemäß der Polung des gewählten Netzteils verdrahten. Der Plan zeigt nur die 5-V-Seite. Grundlage: Pinbelegung in `Sw/Feeder.ino`, aktive Servo-/Encoder-/Dallas-Pinwerte und die vorhandene WiFi-Kit-8-Anschlussübersicht (manufacturer reference; not bundled). Die Illustration zeigt einen funktionsgleichen Aufbau; das Kabelrouting ist schematisch.
