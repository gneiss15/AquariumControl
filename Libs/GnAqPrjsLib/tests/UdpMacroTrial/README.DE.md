# UDP-Makroversuch

[English](README.md)

Kleiner unabhängiger Präprozessorversuch; keine Einbindung in Firmware oder laufende Dienste.

`Example.inc.h` ist die einzige Felddefinition. `Generate.py` schreibt zunächst
Wrapper-Dateien mit den passenden Makrodefinitionen und einem Include dieser Datei.
Danach ruft es `gcc -E -P -x c` auf. Ein Durchlauf erzeugt Structs, der andere
maschinenlesbare Metadaten. Nur die Präprozessorausgabe wird für Python, JSON und
HTML aufbereitet; das Hilfsskript interpretiert nicht den Quelltext der Makrodatei.

Voraussetzungen: Python3, gcc und g++. Aus Prjs:

```bash
python3 Aq/Libs/GnAqPrjsLib/tests/UdpMacroTrial/Generate.py --out Aq/Libs/GnAqPrjsLib/generated/UdpSchemaDraft/MacroTrial
python3 Aq/Libs/GnAqPrjsLib/tests/UdpMacroTrial/RunTests.py
```

Ausgaben: `Structs.h`, `Schema.py`, `Schema.json`, `Schema.html` und die Wrapper sowie
rohe Metadatenausgabe zur Kontrolle. Alle tragen einen Generated-Hinweis mit der
Quelldatei. JSON verwendet dafür den ersten Eintrag `Generated`, da Kommentare
kein gültiges JSON sind. C++-Basistyp-Aliase im Generator sind nur Testadapter.

Geprüft: Pflichtfelder, Konstanten, Unterobjekte, Arrays, optionale/nullbare Typen,
identische Python-/JSON-Metadaten, HTML-Erzeugung sowie C++20-Compile/Run mit Warnungen
als Fehler. Der zweite Durchlauf ändert nur die Definitionsdatei (Data -> Payload,
400 -> 399); alle Zielausgaben müssen mitziehen. Tests arbeiten in temporären
Verzeichnissen und hinterlassen keine Binaries im Projekt.

`TExampleMeasurement` und `TExampleStatus` sind ausschließlich Testtypen. Die
Metadaten enthalten bereits Regeln, führen sie aber noch nicht aus. Dieser Versuch
erzeugt noch keine produktiven JSON-Codecs oder vollständige UDP-Spezifikation.
Für den nächsten Schritt können dieselben Include-Durchläufe Read/Write/Validate
bzw. deren Feldbeschreibungen erzeugen. Keine Geräte-/Installationstests behauptet.

## Auswahl und Empfangsverteilung

Vor dem Include können `DoMsg_Info`, `DoMsg_InfoRequest` oder Gruppen gesetzt
werden: `Do_Observer_TX`, `Do_Observer_RX`, `Do_Service_TX`, `Do_Service_RX`.
`Do_All` erzeugt die vollständige Testübersicht. Die Auswahlmakros werden nach
Verwendung zurückgenommen. BEGIN/END/Feld/Konstanten-Makros müssen alle existieren.

```bash
python3 Aq/Libs/GnAqPrjsLib/tests/UdpMacroTrial/Generate.py --out /tmp/UdpObserverRx --select Do_Observer_RX
```

`Dispatcher.h` wird ebenfalls aus den Präprozessormetadaten erzeugt. Der Aufrufer
liest JSON einmal vollständig ein und übergibt das bereits geparste Objekt.
Der Dispatcher liest `Type`, ordnet den Text einem Enum zu (C++ kann kein switch
auf String) und ruft über den erzeugten switch genau `RxMsg_Info` bzw.
`RxMsg_InfoRequest` auf. Kein zusätzliches Byte im UDP-Paket. Unbekannter oder
falsch typisierter Type wird nicht verteilt. Für jede Gruppe wird ein Empfänger
kompiliert, der ausschließlich den ausgewählten Handler besitzt.

Die Handler erhalten hier noch einen JSON-View, keine vollständig validierte
Nachrichtenstruktur; der Feld-Codec ist weiterhin der nächste Umsetzungsschritt.
