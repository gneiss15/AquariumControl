# GnAqPrjsLib

[English](README.md)

Gemeinsame Bibliothek für die Aquarium-Projekte: Heiz-/Ladeprofile, PI-Regelung,
Geräte-/Quellenzuordnung, Dashboard-Synchronisierung und das UDP-Protokoll.
Universelle Grundlagen bleiben in `../GnBasicLib`; die Abhängigkeit geht nur von
GnAqPrjsLib zu GnBasicLib.

- [UDP-Protokoll](UdpStatus.DE.md)
- [Geräteidentität](DeviceIdentity.md)
- [Bibliotheksabgrenzung](Abgrenzung.DE.md)
- [UDP-Makroversuch](tests/UdpMacroTrial/README.DE.md)

Main-Builds von Feeder, FeederSimple, TmrSwA8T und TmrSwShelly sowie die nativen
Builds von MultiTimer und Dashboard berücksichtigen beide Bibliotheken.
Headernamen und fachliche Schnittstellen bleiben erhalten. Die Transferhelfer
übertragen die Quellen beider Bibliotheken, ohne Geräte-Laufzeitdaten zu ändern.
Bei einem vorhandenen Pi verbleibende alte Aquarium-Dateien unter GnBasicLib sind
bei der tatsächlichen Umstellung separat wiederherstellbar wegzuräumen, damit
keine veralteten Header die neue Bibliothek überdecken. Noch keine Übertragung.

Kanalrolle `MainSw` ist der allgemeine Hauptschalter für messwertabhängige Profile;
`HeaterMain` bleibt ein gleichbedeutender kompatibler Eingabename.
`HeaterFloor` und `Timer` behalten ihre Bedeutung.
