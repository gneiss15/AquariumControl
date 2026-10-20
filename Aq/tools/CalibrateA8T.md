# A8T-Leistungsmessung mit Shelly-Referenz

Das Skript `CalibrateA8T.py` vergleicht einen Shelly und einen Nous A8T in zwei
Messreihen mit vertauschter Reihenfolge. Der Shelly dient als Referenz; nur die
Kalibrierwerte des A8T werden angepasst. Standardmäßig zeigt das Skript die
berechneten Werte an, ohne sie zu schreiben.

## Voraussetzungen

- Beide Geräte laufen mit der passenden TmrSw-Firmware und sind im selben
  erreichbaren Netzwerk.
- Der Shelly und der Nous A8T schalten die verwendete Last zuverlässig.
- Python 3 auf dem PC.
- Für den Schreibvorgang ist die lokale Kalibrierungsseite des A8T erreichbar.

## Ablauf

1. Verbraucher und Geräte bei getrennter Netzversorgung in der Reihenfolge
   Shelly → Nous A8T → Verbraucher verbinden.
2. Das Skript starten. Shelly mit dem Netz verbinden und die Aufforderung
   bestätigen. Das Skript schaltet zuerst das Shelly-Relais ein, wartet dann
   auf den dadurch versorgten A8T und schaltet diesen ebenfalls ein.
3. Nach der ersten Messung die Netzversorgung trennen, die Reihenfolge zu
   Nous A8T → Shelly → Verbraucher tauschen und den NOUS wieder mit dem Netz
   verbinden. Nach der Aufforderung schaltet das Skript zuerst den NOUS ein,
   wartet auf den Shelly dahinter und schaltet diesen ebenfalls ein.
4. Das Skript nimmt die zweite Messreihe auf, berechnet daraus neue A8T-Werte
   und speichert die Rohmessungen als CSV.
5. Die Werte prüfen. Wenn du beim gleichen Durchlauf schreiben möchtest,
   starte das Skript mit `--apply`; nach den Messungen verlangt es zusätzlich
   die ausdrückliche Schreibbestätigung `A8T SCHREIBEN`.

Der Override ist temporär und wird beim Beenden des Skripts aufgehoben. Bei
Abbruch oder Verbindungsverlust die Geräte lokal kontrollieren. Änderungen an
der Verkabelung nur im spannungsfreien Zustand durchführen.

## Aufruf

```text
python3 Aq/tools/CalibrateA8T.py --shelly http://SHELLY-IP --nous http://NOUS-IP
```

Optionen:

- `--seconds`: Messdauer pro Verkabelungsreihenfolge (Standard: 30 Sekunden)
- `--interval`: Abstand zwischen Messungen in Sekunden (Standard: 1 Sekunde)
- `--min-power`: Mindestleistung für gültige Messpaare in Watt (Standard: 100 W)
- `--output`: CSV-Zieldatei
- `--apply`: berechnete Kalibrierwerte nach zusätzlicher Bestätigung auf den A8T
  schreiben und zurücklesen. Ohne diese Option bleibt es bei der Berechnung.

Das Skript schreibt niemals auf den Shelly. Das Überschreiben seiner
Werkskalibrierung ist für diesen Ablauf nicht vorgesehen.
