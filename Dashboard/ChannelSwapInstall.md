<!-- ChannelSwapInstall.md, Version: 1.01 -->
# Kanaltausch V7.28 installieren und prüfen

Noch keine Übertragung durch Codex. Lokale Builds und Prüfungen bestanden; echter Kanaltausch steht aus. Kalibrierung und Partitionen wurden nicht geändert. Snapshot: Prjs.V7.28.BeforeChannelAssignments.20261014.zip.

## 1. NS-Firmware aktualisieren

Am PC im Gerätebrowser jeweils nur Main-Firmware hochladen:

- alle NOUS: `Aq/TmrSwA8T/generated/bin/esp32_Arduino2/TmrSwA8T.ino.bin_signed` → Firmware 1.16;
- beide Shellys: `Aq/TmrSwShelly/generated/bin/esp32c3_Arduino2/TmrSwShelly.ino.bin_signed` → Firmware 1.12.

Kein Rescue-/FS-Upload für diesen Codewechsel erforderlich. Nach Neustart auf erreichbare Geräte und neue Firmwareversion achten. Noch keinen Kanaltausch starten.

## 2. MultiTimer aktualisieren

Am PC:

```bash
cd /mnt/SD_256GB/Prjs/Aq/MultiTimer
./CopyToRasPi.sh
```

Danach auf dem Pi:

```bash
cd ~/Prjs/MultiTimer
make
sudo systemctl restart MultiTimer.service
```

Erwartet: MultiTimer 2.56/Web 2.31. Der Transfer erhält laufende Belegung, Pläne, Identity und ausstehende Transaktionsdateien.

## 3. Dashboard aktualisieren

Am PC:

```bash
cd /mnt/SD_256GB/Prjs/Aq/Dashboard
./CopyToRasPi.sh
```

Den vorhandenen Datenabgleich beachten; aktive DeviceAssignments-Laufzeitdaten werden nicht überschrieben. Danach auf dem Pi:

```bash
cd ~/Prjs/Dashboard
make
sudo systemctl restart DashboardServer.service
```

Browser neu laden. Erwartet: Server 1.23/Data 2.69. Gerätezuordnung enthält MultiTimer1@1..8 aus Main 1..8 sowie die NS-Endpunkte. Fehlende Kanaltausch-Firmware wird kenntlich gemacht; ungleiche Fähigkeiten sind keine zulässigen Partner.

## 4. HeatHistory aktualisieren

Am PC nach dem Quellupdate:

```bash
systemctl --user restart HeatHistory.service
```

HeatHistory 1.15 folgt Boden-/Hauptheizung an ihrem tatsächlich zugeordneten Gerät. Für den Codewechsel ist keine Dienst-Neuinstallation erforderlich.

## 5. Reales Verhalten prüfen

Erst Verbraucher von den beiden zu tauschenden Schaltausgängen trennen, Geräte versorgt lassen. HW-Endpunkte auswählen und tauschen. Bis zur Meldung „Zuordnung von allen Teilnehmern bestätigt“ warten. Danach MT-Seite aktivieren: Listboxen und Belegung müssen die neue Funktion an der unveränderten Main-Steckdose zeigen. Erst dann Verbraucher passend anschließen. Bei Temperaturgebertausch vorher Anschluss und gewünschte Funktionszuordnung prüfen.

Offene MT-Formulare mit ungespeicherten Eingaben werden nicht überschrieben; bei Änderung erscheint ein Hinweis. Nach Speichern/Verwerfen kontrolliert neu laden. Gerätetauschgründe werden nicht erfasst.

Nach tatsächlichem Tausch am PC abgleichen:

```bash
cd /mnt/SD_256GB/Prjs
python3 Aq/tools/ReadDeviceStates.py --apply
```

Vor späterem FS-Upload betroffene Hardwareprojekte neu bauen. Alte FS-Abbilder würden die vorherige Channels-Belegung enthalten. Der Kanaltausch selbst benötigt keinen FS-Upload und keinen Austausch von Hostnamen/Zugangsdaten.
