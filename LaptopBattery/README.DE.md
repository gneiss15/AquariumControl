<!-- README.DE.md, Version: 1.01 -->
# LaptopBattery

[English](README.md)

Linux-Sender für Akkustand, Netzversorgung und Ladezustand. Er sendet alle zehn Sekunden einen UDP-Broadcast an Port 44444. Dashboard zeigt die Messwerte an; ein mit dem Ladeprofil belegter Schaltausgang kann damit das Laptop-Netzteil steuern.

## Installation auf dem Laptop

Python 3.10 oder neuer, Bash und systemd werden benötigt; keine zusätzlichen Python-Pakete.

1. Dieses Verzeichnis auf den Laptop kopieren.
2. Die Pfade in `BatteryUdpSender.py` am Laptop prüfen:

   | Messwert | Voreinstellung |
   | --- | --- |
   | Akkustand | `/sys/class/power_supply/BAT1/capacity` |
   | Netzversorgung | `/sys/class/power_supply/ACAD/online` |
   | Akkuzustand | `/sys/class/power_supply/BAT1/status` |
   | WLAN-MAC | `/sys/class/net/wlp3s0/address` |

   Broadcast-Adresse: `192.168.99.255`. Bei anderer Hardware bzw. anderem Netz diese Einstellungen vor der Installation anpassen.

3. Im Projektverzeichnis als normaler Laptop-Benutzer ausführen, **ohne vorangestelltes sudo**:

   ```bash
   ./Install.sh
   ```

Der Installer prüft die Hardwarepfade, verwendet sudo für die Systeminstallation und startet den Dienst `BatteryUdpSender.service` unter dem aufrufenden Benutzer. Dienstname und Senderpfad bleiben gleich. `Install.sh` ersetzt die vorhandenen Programmdateien und die Unit direkt; eine vorherige Deinstallation ist nicht nötig. Bestehende Lernzustände werden erhalten. Derselbe Aufruf dient sowohl zur Erstinstallation als auch zum Update.

Installiert werden `/usr/local/bin/BatteryUdpSender.py`, `ChargeLearning.py`, `ChargeLearning.sh`, die Unit unter `/etc/systemd/system/` und `Versions.txt` unter `/usr/local/share/LaptopBattery/`.

```bash
systemctl status BatteryUdpSender.service
journalctl -u BatteryUdpSender.service -n 30 --no-pager
```

Die passenden Empfänger benötigen die Bootzähler-Unterstützung: Nous ab Firmware 1.29, Shelly ab 1.25, MultiTimer ab 2.67.

## Lernschalter

Als derselbe Benutzer, unter dem der Dienst läuft, **ohne sudo**:

```bash
ChargeLearning.sh status
ChargeLearning.sh off      # Lernen AUS
ChargeLearning.sh on       # Lernen AN
```

`aus` und `ein` sind ebenfalls möglich; `toggle` wechselt den aktuellen Zustand. Im Quellverzeichnis funktioniert `./ChargeLearning.sh ...`.

Bei erstmaliger Einrichtung ist Lernen AN. AUS bleibt über Neustarts erhalten. Umschalten funktioniert auch unterwegs ohne Netzwerk und wird im nächsten UDP-Paket gemeldet, spätestens nach etwa zehn Sekunden, solange der Dienst läuft. AUS verwirft die laufende oder noch auf Rückmeldung wartende Lernmessung. Der normale Ladenachlauf verwendet weiterhin den vorhandenen Faktor.

Nach manuellem AUS erst wieder im Hausnetz einschalten: Mit weiterhin AUS verbunden mindestens ein Sendeintervall abwarten, damit der Regler den alten Lernzyklus verwirft; danach AN. Der Schalter selbst erhöht keinen Zähler.

## Startzähler und Lernmessung

Der Sender speichert den letzten Linux-Boot-Identifier und einen `uint32`-Startzähler. Ein neuer Systemstart erhöht den Zähler genau einmal. Neustarts des Senderdienstes, AC-Wechsel sowie der Lernschalter erhöhen ihn nicht. Standby und Ruhezustand zählen nicht als neuer Systemstart. Damit jeder Start erfasst wird, muss der Dienst auch unterwegs automatisch laufen.

Der Regler merkt sich den Zähler zur letzten Akkumeldung vor dem Nachlauf und prüft ihn bei Rückkehr:

- Differenz genau **1**: erwarteter nächster Start; die Messung darf bei erteilter Lernfreigabe ausgewertet werden.
- Differenz **0**, **2 oder mehr**: keine passende Neustartfolge; die Lernmessung wird verworfen.

Die Differenz wird modulo `2^32` berechnet. `4294967295 → 0` ergibt 1 und ist gültig; `4294967294 → 0` ergibt 2 und verwirft die Messung. Null ist ein gültiger Zählerstand.

**Besonderheiten:** Externes Laden bei durchgehend ausgeschaltetem Laptop ist nicht erkennbar. In diesem Fall den Laptop während der Reise mindestens einmal starten und vor dem normalen Start zu Hause wieder ausschalten oder Lernen manuell AUS lassen. Auch ein einziger Start unterwegs mit anschließender Rückkehr im weiterhin eingeschalteten Zustand ist von einem normalen Rückkehrstart nicht unterscheidbar; dafür den Lernschalter AUS verwenden. Der Zähler erkennt zusätzliche Starts, nicht den Aufenthaltsort oder jeden Ladevorgang.

## Persistenz und UDP

Lernschalter, Startzähler und letzter Boot-Identifier liegen in `~/.local/state/LaptopBattery/ChargeLearning.json`. Sender und Bedien-Skript verwenden dieselbe Datei. Schreibvorgänge erfolgen mit Dateisperre und atomarem Austausch; normale Sendungen schreiben nichts. Der Boot-Identifier bleibt lokal. Die Zustandsdatei bei Updates und Neuinstallation erhalten. Bei beschädigter oder unlesbarer Datei bleibt die Akkuanzeige erhalten, Lernen wird gesperrt und der Fehler im Journal gemeldet; die defekte Datei wird nicht automatisch ersetzt.

```json
{"Version":2,"Type":"Status","ID":"<WLAN-MAC>@3","Status":{"Kind":"LaptopPower","BatteryPercent":63,"AcOnline":true,"BatteryState":"Charging","ChargeLearningAllowed":true,"ChargeBootCounter":17}}
```

Die MAC wird zur Laufzeit gelesen und großgeschrieben. Akkustand und AC sind bei Lesefehlern `null`; Akkuzustand ist `Charging`, `Discharging`, `Idle` oder `Unknown`. Ohne gültige Lernfreigabe und Bootzähler lernen die passenden Empfänger keinen Faktor.

## Tests

```bash
python3 tests/test_sender.py
python3 tests/test_install.py
```

Die Tests verwenden temporäre Zustände, simulierte Sensor-Dateien und echtes UDP ausschließlich über Loopback. Geprüft werden Bootwechsel, Dienstneustart, Überlauf, persistentes AUS/AN, parallele Bedienung, defekte Zustände und die gesendeten Felder. Ein Test am tatsächlichen Laptop ist zusätzlich erforderlich.
