<!-- README.DE.md, Version: 1.00 -->
# GnBasicLib

[English](README.md)

Gemeinsame Grundfunktionen für die Firmware und Anwendungen des Projekts.

## GPIO-Funktionen

| Name | Aufgabe |
| --- | --- |
| `Set...` | Ausgangslatch setzen. |
| `Clr...` | Ausgangslatch löschen. |
| `Toggle...` | Ausgangslatch umschalten. |
| `GetOut...` | Ausgangslatch lesen. |
| `GetIn...` | Pinzustand lesen. |
| `...Pin` | Pin als Nummer angeben. |
| `...PinM` | Pin als Bitmaske angeben. |

Pinmodi sind unter anderem `PinMode_Output`, `PinMode_OutputOpenDrain`, `PinMode_Input` und `PinMode_InputPullUp`.

## CPU-spezifische Funktionen

`GetCycleCount` liefert den CPU-Zykluszähler.
