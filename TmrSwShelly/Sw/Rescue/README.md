# Shelly Produktions-Rescue

Eigenständige Produktionsquellen für den vom Benutzer nach abgeschlossener Migration
freigegebenen Rescue. Einstieg im Projektstamm: `./EspRescueCompile.sh`.

`main/` enthält tatsächliche Hardware-/SDK-Geometrieprüfung, Main-Aktivierungsprüfung
und den bisherigen Diagnosedienst. `Runtime/` enthält dessen noch gelinkte Geometrie-/
Unsicherheitsfunktionen sowie die Arduino-Kompatibilitätsschicht. Die Laufzeitlogik
wurde beim Herauslösen unverändert übernommen; Stock-Rollback bleibt gesperrt.
Die gemeinsamen Update-/Signaturquellen kommen aus Aq/Libs/Common.

Die Ziel-Partitionstabelle wird ausschließlich aus ../../partitions.csv erzeugt;
ihr eingebetteter Inhalt wurde bytegleich mit dem bisherigen Stand verglichen.
Generierte private WLAN-Header und Schlüsselmetadaten bleiben unter generated/build/rescue;
private Originaldateien liegen außerhalb von Prjs. Der Build lädt oder schreibt kein Gerät.
