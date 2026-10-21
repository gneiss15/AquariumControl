<!-- README.DE.md, Version: 1.00 -->
# Shelly Produktions-Rescue

[English](README.md)

Eigenständige Produktionsquellen für Rescue. Buildeinstieg im TmrSwShelly-Projektstamm:

```bash
./EspRescueCompile.sh
```

`main/` enthält Hardware-/SDK-Geometrieprüfung, Main-Aktivierungsprüfung und den Diagnosedienst. `Runtime/` enthält Geometrie-/Unsicherheitsprüfungen sowie die Arduino-Kompatibilitätsschicht. Stock-Rollback ist gesperrt. Die gemeinsamen Update-/Signaturquellen liegen unter `../../../Libs/Common/`, relativ zu diesem Verzeichnis.

Die Ziel-Partitionstabelle wird aus `../../partitions.csv` erzeugt. Generierte private WLAN-Header und Schlüsselmetadaten liegen unter `../../generated/build/rescue/`; private Originaldateien liegen außerhalb des Repositorys. Der Build lädt oder schreibt kein Gerät.
