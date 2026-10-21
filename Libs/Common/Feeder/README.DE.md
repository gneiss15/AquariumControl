<!-- README.DE.md, Version: 1.06 -->
# Feeder Common

[English](README.md)

Dieses Verzeichnis ist bewusst keine Arduino-Library. Die Projektdateien in `Feeder` und `FeederSimple` binden die gemeinsamen Header und CPP-Dateien über dünne Wrapper ein.

Zeitplan, Konfiguration, Status-API, Temperatur, Zeitabgleich, WebSocket und die Laufwerkschnittstelle liegen gemeinsam hier. Die WLAN-/Webserver-Implementierung von `Feeder` bindet die gemeinsame Quelle ein; `FeederSimple` hat dafür eine projektspezifische Erweiterung mit seiner Diagnose-Webseite.

Die lesbaren Webquellen für beide Geräte liegen in `WebSrc/`. Beide PreBuild-Skripte erzeugen daraus beim Build die minifizierten Webdateien; FeederSimple entfernt dabei ausschließlich die servo-spezifische Datei `ServoSteps.json`.

Hardwarezugriffe erfolgen ausschließlich über die projektspezifische Datei `FeederDrive.h` und die gemeinsame Schnittstelle `TFeederDrive`.
