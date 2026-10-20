# Feeder – bill of materials

[Deutsch](BOM.DE.md) · [ODS](BOM.ods) · [Hardware overview](README.md)

| Item | Qty | CAD object | Description | Manufacturer / supplier | Notes | Source (examples) |
| --- | ---: | --- | --- | --- | --- | --- |
| 1 | 1 | CaseBot | Lower housing (3D printed, PLA) | DIY |  |  |
| 2 | 1 | Bot | Metering unit base (3D printed, PLA) | DIY |  |  |
| 3 | 1 | Center | Metering disc (3D printed, PLA) | DIY |  |  |
| 4 | 1 | Top | Upper section with food container (3D printed, PLA) | DIY |  |  |
| 5 | 1 | Lid | Food container lid (3D printed, PLA) | DIY |  |  |
| 6 | 1 | Funnel | Discharge funnel (3D printed, PLA) | DIY | *8 |  |
| 7 | 1 | Front | Front panel (3D printed, PLA) | DIY |  |  |
| 8 | 1 | Food | Feeder positioning foot (3D printed, PLA) | DIY | *8 |  |
| 9 | 1 | FrontBridge | Front bridge (3D printed, PLA) | DIY |  |  |
| 10 | 1 | WifiKit8 | WiFi Kit 8 (ESP8266 with OLED) | various | *1,4 | [Link](https://www.amazon.de/DollaTek-ESP8266-Internet-Development-OLED-Display/dp/B0D1TZL5GY) |
| 11 | 1 | RotEnc | Rotary encoder module with push button | various | *1 | [Link](https://www.amazon.de/QWORK-Drehgeber-Encoder-Drehwinkelgeber-Druckknopf/dp/B09R41C1HR) |
| 12 | 1 | DC-Jack | Panel-mount DC barrel socket | various | *1,5 | [Link](https://www.amazon.de/dp/B0H1BTBRYS) |
| 13 | 1 | ServoPart | SG90 positional micro servo (9 g) | various | *1 | [Link](https://www.amazon.de/servo-sg90/s?k=servo+sg90) |
| 14 | 1 | Lemo-Socket | 3-pole panel-mount connector for DS18B20 (optional) | Lemo | *1,2 | [Link](https://www.amazon.de/DRRI-2-polig-3-polig-4-polig-Rundstecker/dp/B07JWCQJB1) |
| 15 | 4 | Screw M2.5 x 12 | Metric screw M2.5 × 12 mm | various |  |  |
| 16 | 2 | Screw M2.5 x 5 | Metric screw M2.5 × 5 mm | various |  |  |
| 17 | 6 | N/A | M2.5 heat-set threaded insert | various | *6 | [Link](https://www.amazon.de/s?k=Heat-Inserts) |
| 18 | 2 | Screw M1.6 x 5 | Self-tapping screw for plastic, Ø 1.6 × 5 mm | various | *7 |  |
| 19 | 1 | N/A | Plug-in power supply, 5 V DC / 2 A, barrel plug | various | *5 | [Link](https://www.amazon.de/dp/B097C4JL7P) |
| 20 | 1 | N/A | DS18B20 temperature probe with cable (optional) | Dallas | *2,3 | [Link](https://www.amazon.de/s?k=DS18B20) |
| 21 | 1 | N/A | 3-pole cable connector for DS18B20 (optional) | Lemo | *2,3 | [Link](https://www.amazon.de/DRRI-2-polig-3-polig-4-polig-Rundstecker/dp/B07JWCQJB1) |
| 22 | 1 | N/A | 4.7 kΩ pull-up resistor for DS18B20 (optional) | various | *9 |  |
| 23 | 1 | O-Ring | O-ring 22 × 2.5 mm (inside diameter × cross-section) | various | *10 |  |
| 24 | 13 | N/A | Single-wire Dupont lead | various | *11 |  |

## Notes

1. When using different components, the corresponding 3D-printed parts may need to be adapted.
2. Any matching plug and socket combination with at least three contacts can be used.
3. Temperature probes with cables and an existing plug/socket combination were used. Ensure the correct pin assignment at the plug, socket and controller when wiring.
4. Board shape and display position vary slightly between suppliers. The existing front panel (item 7) accommodates up to 1 mm of vertical offset; otherwise see note 1.
5. The power supply and socket must use the same connector type. Wire the socket to match the power supply polarity (usually centre positive).
6. Four threaded inserts in CaseBot and two in FrontBridge receive the screws in items 15 and 16, respectively.
7. For securing the rotary encoder module.
8. The discharge funnel (item 6) is inserted into the aquarium lid. The positioning foot (item 8) aligns the feeder with it. Both parts depend on the aquarium and must be adapted accordingly.
9. For the optional DS18B20: connect the pull-up resistor between the data line and 3.3 V.
10. For FeederV2 only: O-ring in Top; designed, not yet physically tested. Material to be determined.
11. Per wiring diagram: power 2, servo 3, encoder 5, sensor socket 3 leads; 10 instead of 13 without the sensor connection. Matching female contacts or one end soldered; length to suit assembly. Probe cable separate.
