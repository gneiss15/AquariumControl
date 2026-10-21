<!-- CalibrateA8T.DE.md, Version: 1.01 -->
# Nous A8T mit Shelly-Referenz kalibrieren

[English](CalibrateA8T.md) · [Skript](CalibrateA8T.py)

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
   Nous A8T → Shelly → Verbraucher tauschen und den Nous wieder mit dem Netz
   verbinden. Nach der Aufforderung schaltet das Skript zuerst den Nous ein,
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
- `--reconnect-timeout`: Wartezeit auf ein Gerät nach dem Umstecken (Standard: 180 Sekunden)
- `--output`: CSV-Zieldatei
- `--apply`: berechnete Kalibrierwerte nach zusätzlicher Bestätigung auf den A8T
  schreiben und zurücklesen. Ohne diese Option bleibt es bei der Berechnung.

Die Kalibrierwerte des Shelly werden nicht verändert. Das Skript schaltet jedoch beide Relais über einen temporären Override ein und hebt diesen anschließend wieder auf. Auch ohne `--apply` werden daher die Relais geschaltet.

Die Messung ist ein Abgleich mit dem verwendeten Shelly als Referenz; dessen Messgenauigkeit bestimmt die Aussagekraft des Vergleichs. Eigene CSV-Messreihen gehören nicht ins öffentliche Repository.

## Berechnen oder übernehmen

Vom Repository-Stamm, mit den tatsächlichen Geräteadressen, zunächst nur messen und berechnen:

```sh
python3 Aq/tools/CalibrateA8T.py --shelly http://SHELLY-IP --nous http://NOUS-IP
```

Zum Übernehmen das Skript mit `--apply` starten. Es führt die Messreihen erneut durch und fordert vor dem Schreiben die Bestätigung `A8T SCHREIBEN` an:

```sh
python3 Aq/tools/CalibrateA8T.py --shelly http://SHELLY-IP --nous http://NOUS-IP --apply
```

Das Skript prüft die Gerätetypen `TmrSwShellyC3V1` und `TmrSwA8T`, verwirft ungültige Messpaare und liest geschriebene A8T-Werte zur Kontrolle zurück. Die Spannungs-, Strom- und Leistungskorrektur wird aus den Medianen beider Messreihen und deren geometrischem Mittel berechnet.
