<!-- README.DE.md, Version: 1.07 -->
# AquariumControl

[English](README.md)

**Work in progress – Anmerkungen, Testergebnisse und Verbesserungsvorschläge sind ausdrücklich erwünscht.**

Hardware, Firmware und Software für ein lokal betriebenes Aquarium-System: Fütterung, Beleuchtung, temperaturgeregelte Heizung, ein Raspberry-Pi-Dashboard und Verlaufsauswertungen am PC.

Die eigenen Projektdateien stehen unter [PolyForm Noncommercial 1.0.0](LICENSE.md). Required Notice: Copyright 2026 Günter Neiß. Bestehende Komponentenlizenzen bleiben unverändert; siehe [THIRD_PARTY.md](THIRD_PARTY.md).

## Komponenten

- [Dashboard](Dashboard/README.DE.md): Aquariumübersicht, Schaltzeiten, Heizprofile, MAC-basierte Gerätezuordnung und Funktionstausch.
- [MultiTimer](MultiTimer/README.DE.md): nativer Mehrkanal-Timer und Controller.
- [NOUS A8T](TmrSwA8T/README.md) und [Shelly](TmrSwShelly/README.md): Schaltgeräte-Firmware mit Leistungsmessung und Heizprofilen.
- [Feeder](Feeder/README.DE.md) und [FeederSimple](FeederSimple/README.md): Futterautomaten und Temperatursender.
- [HeatHistory](HeatHistory/README.md): passiver CSV-Rekorder und Diagramme auf Abruf, optional als PC-Benutzerdienst.
- `Libs/`: gemeinsamer Code; `tools/`: Build-, Synchronisations- und Kalibrierwerkzeuge.

Die Erstübernahme der Schaltgeräte wird separat in [ShellyNousTakeover](https://github.com/gneiss15/ShellyNousTakeover) gepflegt. Die kompatiblen Main-Anwendungen liegen hier in AquariumControl.

## Leistungsmessung kalibrieren

NOUS A8T können mithilfe eines Shelly als Messreferenz abgeglichen werden. Das [Kalibrierskript](tools/CalibrateA8T.py) und die [Anleitung](tools/CalibrateA8T.DE.md) sind enthalten. Das Skript berechnet Korrekturwerte aus zwei Messreihen mit vertauschter Gerätereihenfolge; geschrieben wird nur auf den A8T, mit `--apply` und zusätzlicher Bestätigung. Die Shelly-Kalibrierung bleibt unverändert.

## Lokale Konfiguration und Builds

Die Python-Werkzeuge benötigen Python 3.10 oder neuer. Native Builds verwenden die Projekt-Makefiles. ESP32- und ESP8266-Projekte verwenden unterschiedliche Toolchains; vor dem Bauen die jeweilige Projekt-README lesen. Vorhandene lokale Werkzeugpfade sind konfigurierbar. Dashboard und MultiTimer wurden auf einem PC erfolgreich kompiliert. Ob sich die ESP-Firmware allein mit dem veröffentlichten Repository und einer eigenen Konfiguration bauen lässt, ist noch nicht geprüft. **Erfahrungen mit eigenen Builds sind willkommen: Bitte berichtet, was funktioniert hat und wo Anleitungen oder Abhängigkeiten fehlen.**

Enthalten sind Quellen und Hardwareunterlagen. Gerätespezifische `DevState`-Konfigurationen, WLAN-Zugangsdaten, private Signierschlüssel, Raspberry-Pi-Laufzeitdaten, Mitschnitte, Logs und Buildprodukte sind nicht enthalten. Eigene Konfiguration und Signierdaten außerhalb des öffentlichen Repositorys bereitstellen; siehe [BUILD-SECRETS.md](BUILD-SECRETS.md). **Coming soon:** eine vollständige Erstaufbau-Anleitung und generische Geräte-/Dashboard-Konfigurationsbeispiele. Der Entwicklungsstand ist noch kein sofort betriebsfähiges Komplettpaket.

## Prüfstand

Der laufende Betrieb ist für die bestehende Aquarium-Installation bestätigt; daraus folgt keine Funktionsgarantie für andere Hardware. Das Hinzufügen und Entfernen von Hardware im Geräteeditor ist lokal automatisiert geprüft, der Gerätetest steht noch aus.

## Weiterführende Hardware-READMEs

Die Hardwarebeschreibung und ihr jeweiliger Dokumentationsstand stehen in den Projektunterlagen:

- [Feeder – Hardware](Feeder/Hw/README.DE.md)
- [FeederSimple – Hardware](FeederSimple/Hw/README.DE.md)
- [MultiTimer / Dashboard – Raspberry-Pi-Hardware](MultiTimer/Hw/README.DE.md)

## Rückmeldungen ausdrücklich erwünscht

**Bitte meldet unklare Anleitungen, fehlende Informationen, Probleme und Testergebnisse. Auch Erfahrungen mit einem erfolgreichen Nachbau und Verbesserungsvorschläge sind willkommen.**

[Rückmeldung als GitHub Issue eintragen](https://github.com/gneiss15/AquariumControl/issues)
