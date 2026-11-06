# Abgrenzung GnBasicLib / GnAqPrjsLib

Prüfstand 02.11.2026. Die unten aufgeführten Aquarium-spezifischen Dateien und
ihre Tests sind lokal nach GnAqPrjsLib verschoben. Keine Geräteinstallation.

## Nach GnAqPrjsLib verschobene Bestandteile

| Verschobene Dateien | Konkreter Projektbezug |
|---|---|
| GnHeaterControl.h/.cpp | Heiz-/Ladeprofile, Main/Floor-Rollen, Quellen und Ausfallverhalten |
| GnHeatPi.h, GnHeatPiJson.h, GnHeatPiProfileJson.h, GnHeatPiPeer.h | Aquarium-Modelllernen, Main/Floor-Aufteilung, PI-Profile und Boden-Steuervertrag; der vorhandene Regler ist kein allgemeiner unabhängiger PI-Baustein |
| GnChargeRunOn.h, GnChargeProfileJson.h | Laptop-Ladeprofil, Boot-/Lernzyklus und Dashboard-Abgleich |
| GnTemporaryDiagnostics.h | Genau die temporären Aquarium-PI-/Ladediagnoseschalter |
| GnDeviceIdentity.h/.cpp, DeviceIdentity.md | Aquarium-Funktionszuordnung, Quellen, Verteilungsrevision und Dashboard-Prüfung; keine bloße MAC-Hilfsfunktion |
| GnFunctionConfig.h, GnFunctionConfig.inc.h | Genau die beim Aquarium-Gerätetausch zu übernehmenden Konfigurationsfelder |
| GnTimeSync.h/.cpp | Kein allgemeiner NTP-Dienst: Dashboard-/api/times/sync, Schalt-/Fütterzeiten, Identity und Gerätezuordnung; sogar feste Dashboard-URL |
| GnUdpStatus.h/.cpp | Aquarium-Status, Temperatur-/Lade-/Heizdaten, Watch-/Peer-Verarbeitung |
| GnUdpCompact.h, GnUdpSources.h | Konkrete Aquarium-Paketfelder, Beschreibungsrevisionen, Quellenbestätigungen und Info-Requests |
| GnUdpControl.h | Socket-Helfer enthält Aquarium-Serviceports 44501–44504 und 900-Byte-Grenze; in aktueller Form hierher, ein später rein allgemeiner Socket-Baustein wäre abtrennbar |
| UdpStatus.md, UdpStatus.DE.md | Dokumentation genau dieses Protokolls |

Die zugehörigen vorhandenen Tests sind mit ihren Quellen umgezogen:
`test_heat_pi*.py`, `test_charge_run_on.py`, `test_manual_charge_cancel.py`,
`test_udp_*.py`, `test_identity_sources.py`. Ihre Include-/Quellpfade sind
entsprechend angepasst.

## Grenzfall

`GnPowerMeterCycle.h` bündelt allgemein V/A/W-Fenster, enthält aber fest eingebaute
7500-ms-/15000-ms-Fristen für den aktuellen Messvertrag. Ohne Parametrisierung
ist diese Fassung ebenfalls nach GnAqPrjsLib verschoben. Ein allgemeiner wiederverwendbarer
Messzyklus mit expliziter Zeitkonfiguration könnte später in GnBasicLib bleiben.

## Universell belassen

`GnWireJson.h` ist ein plattformübergreifender ArduinoJson/json-c-Adapter ohne
Aquarium-Felddefinitionen. Seine allgemeinen JSON-Typprüfungen gehören in GnBasicLib;
Paketfelder, Validierungsregeln und Paket-Codecs dagegen nach GnAqPrjsLib.

Bei den übrigen Plattform-/Grundlagen-Dateien wurden keine vergleichbaren
Aquarium-Feldverträge gefunden: GnBasics/GnPlatform/GNPlatformESP/GnBasicsAVR,
GnString, GnHash, GnCrc8, GnMemory, GnTime, GnCoopTimer, Closures, UUID,
GnRotEncoder, WlanNanny/MiniWlanNanny, WdtDelay, AsyncWebServerEx,
SpecialPrint/SpecialLog sowie ESP-Info-/Fehlernamen und allgemeine Config-Flags.
Dies ist eine fachliche Abgrenzung, kein vollständiges Qualitätsaudit dieser Helfer.

## Angepasste Integration / Verifikation

- MultiTimer/Makefile: Include-Verzeichnisse sowie explizite GnUdpStatus-,
  GnHeaterControl- und GnDeviceIdentity-Quellen/Objektregeln.
- TmrSwA8T/Sw/main/CMakeLists.txt und TmrSwShelly/Sw/main/CMakeLists.txt:
  GnBasicLib-Quellenliste aufteilen, beide Include-Verzeichnisse berücksichtigen.
- Feeder/FeederSimple-Builds: Bibliotheksentdeckung/Include-Pfade und tatsächlich
  verwendete projektbezogene Quellen prüfen.
- Dashboard/Makefile: Include-Pfade und Header-Abhängigkeiten aufteilen.
- Gemeinsame Feeder/TmrSw-Quellen, Sync-/UDP-Code und Test-Harnesses mit hart
  kodierten Library-Pfaden, Kopier-/Installskripte und vorhandene Dokumentation.
- Library-Metadaten/Abhängigkeit auf GnBasicLib passend zur tatsächlichen
  Buildintegration ergänzen; keine universellen Grundlagen zurück von Aq abhängig machen.
- Native Builds/Tests und die betroffenen Firmware-Builds nach dem Umzug prüfen.

Die Buildlisten und Test-Harnesses sind lokal aufgeteilt. Vor dem ersten
Pi-Quelltransfer müssen alte gleichnamige Aquarium-Header in der dortigen
GnBasicLib wiederherstellbar aus dem Include-Pfad genommen werden; die Skripte
löschen keine Remote-Dateien. Der Umzug enthält keine Änderung an der
fachlichen Regelungs-/Schaltlogik.
