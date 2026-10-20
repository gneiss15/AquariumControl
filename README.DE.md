<!-- README.public.DE.md, Version: 1.02 -->
# AquariumControl

[English](README.md)

**Work in progress – Anmerkungen, Testergebnisse und Verbesserungsvorschläge sind ausdrücklich erwünscht.**

Quellen für ein lokal betriebenes Aquarium-System: Fütterung, Beleuchtung, temperaturabhängige Heizung, Raspberry-Pi-Dashboard und Verlaufsdiagramme am PC.

Die eigenen Projektdateien stehen unter [PolyForm Noncommercial 1.0.0](LICENSE.md). Required Notice: Copyright 2026 Günter Neiß. Bestehende Komponentenlizenzen bleiben unverändert; siehe [THIRD_PARTY.md](THIRD_PARTY.md).

## Komponenten

- [Dashboard](Aq/Dashboard/README.md): Aquariumübersicht, Schaltzeiten, Heizprofile, MAC-basierte Gerätezuordnung und Funktionstausch.
- [MultiTimer](Aq/MultiTimer/README.md): nativer Mehrkanal-Timer und Controller.
- [NOUS A8T](Aq/TmrSwA8T/README.md) und [Shelly](Aq/TmrSwShelly/README.md): Schaltgeräte-Firmware mit Leistungsmessung und Heizprofilen.
- [Feeder](Aq/Feeder/README.DE.md) und [FeederSimple](Aq/FeederSimple/README.md): Futterautomaten und Temperatursender.
- [HeatHistory](Aq/HeatHistory/README.md): passiver CSV-Rekorder und Diagramme auf Abruf, optional als PC-Benutzerdienst.
- `Aq/Libs/`: gemeinsamer Code; `Aq/tools/`: Build-, Synchronisations- und Kalibrierwerkzeuge.

Die Erstübernahme der Schaltgeräte wird separat in [ShellyNousTakeover](https://github.com/gneiss15/ShellyNousTakeover) gepflegt. Die kompatiblen Main-Anwendungen liegen hier in AquariumControl.

## Leistungsmessung kalibrieren

NOUS A8T können mithilfe eines Shelly als Messreferenz abgeglichen werden. Das [Kalibrierskript](Aq/tools/CalibrateA8T.py) und die [Anleitung](Aq/tools/CalibrateA8T.DE.md) sind enthalten. Das Skript berechnet Korrekturwerte aus zwei Messreihen mit vertauschter Gerätereihenfolge; geschrieben wird nur auf den A8T, mit `--apply` und zusätzlicher Bestätigung. Die Shelly-Kalibrierung bleibt unverändert.

## Lokale Konfiguration und Builds

Die Python-Werkzeuge benötigen Python 3.10 oder neuer. Native Builds verwenden die Projekt-Makefiles. ESP32- und ESP8266-Projekte verwenden unterschiedliche Toolchains; vor dem Bauen die jeweilige Projekt-README lesen. Vorhandene lokale Werkzeugpfade sind konfigurierbar. Dashboard und MultiTimer wurden mit der installierten PC-Toolchain aus einem isolierten Export gebaut und gelinkt. ESP-Builds aus einer frischen Kopie stehen noch aus.

Enthalten sind Quellen und Hardwareunterlagen. Gerätespezifische `DevState`-Konfigurationen, WLAN-Zugangsdaten, private Signierschlüssel, Raspberry-Pi-Laufzeitdaten, Mitschnitte, Logs und Buildprodukte sind nicht enthalten. Eigene Konfiguration und Signierdaten außerhalb des öffentlichen Repositorys bereitstellen; siehe [BUILD-SECRETS.md](BUILD-SECRETS.md). **Coming soon:** eine vollständige Erstaufbau-Anleitung und generische Geräte-/Dashboard-Konfigurationsbeispiele. Der Entwicklungsstand ist noch kein sofort betriebsfähiges Komplettpaket.

## Prüfstand

Der laufende Betrieb ist für die bestehende Aquarium-Installation bestätigt; daraus folgt keine Funktionsgarantie für andere Hardware. Das Hinzufügen und Entfernen von Hardware im Geräteeditor ist lokal automatisiert geprüft, der Gerätetest steht noch aus.

## Weiterführende Hardware-READMEs

Die Hardwarebeschreibung und ihr jeweiliger Dokumentationsstand stehen in den Projektunterlagen:

- [Feeder – Hardware](Aq/Feeder/Hw/README.DE.md)
- [FeederSimple – Hardware](Aq/FeederSimple/Hw/README.DE.md)
- [MultiTimer / Dashboard – Raspberry-Pi-Hardware](Aq/MultiTimer/Hw/README.DE.md)

## Rückmeldungen ausdrücklich erwünscht

**Bitte meldet unklare Anleitungen, fehlende Informationen, Probleme und Testergebnisse. Auch Erfahrungen mit einem erfolgreichen Nachbau und Verbesserungsvorschläge sind willkommen.**

[Rückmeldung als GitHub Issue eintragen](https://github.com/gneiss15/AquariumControl/issues)
