// README.txt, Version: 1.13

EspUpdate

Gemeinsame Webaktualisierung für Feeder, FeederSimple, TmrSw und ESP32-Projekte
wie TmrSwShelly.

WebSrc enthält den gemeinsamen HTML-, CSS- und JavaScript-Teil. Die lesbaren
Projektseiten enthalten nur die Marker EspUpdateCommonHead und
EspUpdateCommonBody. PrepareWebSrc.py erzeugt daraus beim Pre-Build die
vollständige Projektwebseite.

Die Update-Seite besitzt genau zwei Bereiche:

- "Update package" für eine einzelne *.update.signed-Datei.
- "Update Firmware and/or Filesystem" mit je einem optionalen Feld für
  *.bin.signed bzw. *.bin.gz.signed und *.littlefs.signed. Es kann nur Firmware, nur LittleFS oder
  beides gemeinsam ausgewählt werden.

Für den kombinierten Upload gibt es zwei alternative Wege:

- *.update.signed ist der bevorzugte kompakte Ein-Datei-Upload. Das als Ganzes
  signierte Paket enthält die Firmware und nur die tatsächlich im data-
  Verzeichnis vorhandenen LittleFS-Dateien. Die Firmware wird zunächst in den
  OTA-Bereich geschrieben, aber erst nach erfolgreicher Signaturprüfung des
  vollständigen Pakets zum Start vorgemerkt. Danach erfolgt genau ein Neustart.
- Alternativ können im zweiten Bereich *.bin.signed bzw. *.bin.gz.signed und *.littlefs.signed
  gemeinsam hochgeladen werden. Für reine Firmware-Updates ist *.bin.gz.signed wegen
  der kleineren OTA-Belegung die bevorzugte Variante. Dabei ist *.littlefs.signed das vollständige
  partitionsgroße LittleFS-Abbild. Derselbe Bereich erlaubt bei Bedarf auch
  ein gezieltes Update nur der Firmware oder nur des Filesystems.

ESP8266-Firmware wird beim Build standardmäßig sowohl als *.bin.signed als auch
als *.bin.gz.signed erzeugt. Die Firmware-Signatur wird im Web-Updater selbst
streamend über SHA-256/RSA geprüft. Signatur und Längenfeld werden nicht in den
OTA-Bereich geschrieben. Update.begin() erhält Payload-Größe + 1; erst nach
erfolgreicher eigener Verifikation wird Update.end(true) aufgerufen. Damit wird
der automatische RSA/ECC-Signaturverifier des ESP8266-Cores nicht verwendet.
Der projektspezifische RSA-Public-Key wird beim Build als EspUpdateSigning.h
erzeugt.

Für *.update.signed wird beim Build intern ein kompaktes LittleFS-Dateipaket im
Format LFSUPD3 erzeugt. Dieses interne Paket wird nicht als eigenes
Web-Upload-Artefakt ausgegeben. Sein Aufbau ist:

- Kennung LFSUPD3\0
- konfigurierte LittleFS-Partitionsgröße
- Anzahl Dateien
- Summe aller Datei-Inhaltsgrößen
- vollständige Dateitabelle mit Pfadlänge, Inhaltslänge und Pfad für jede Datei
- danach die Inhalte aller Dateien zweimal in derselben Reihenfolge

Die vollständige Tabelle steht damit bereits fest, bevor das erste Datei-
Inhaltsbyte empfangen wird. Im ersten Datenpass vergleicht der ESP jede Datei
byteweise mit dem aktuell installierten LittleFS. Dateien mit anderer Größe,
fehlende Dateien oder Dateien mit abweichendem Inhalt werden als geändert
markiert. Erst danach erstellt der ESP den Staging-Plan ausschließlich für
neue/geänderte Dateien:

- Im zweiten Datenpass werden identische Dateien nur konsumiert und nicht
  gestaged.
- Möglichst viele geänderte Dateien werden zunächst als <Name>.espupd in den
  noch freien LittleFS-Bereich geschrieben.
- Nur der Anteil geänderter Dateien, für dessen temporäre Flash-Kopie der freie
  LittleFS-Platz nicht reicht, wird im RAM gehalten.
- Die Build-Seite ordnet größere Dateien zuerst an; die Firmware plant anhand
  der vollständigen Tabelle ebenfalls größenorientiert.
- Für die Abschätzung des temporären Flash-Bedarfs wird blockweise gerechnet
  und mindestens ein LittleFS-Block als Reserve freigehalten.
- Für RAM-Staging bleibt zusätzlich eine Heap-Reserve erhalten.

Bestehende Nutzdateien werden während des Empfangs nicht verändert. Die
Signatur umfasst Firmware, Dateitabelle und sämtliche Dateiinhalte gemeinsam.
Bei Abbruch oder ungültiger Signatur werden die noch nicht aktivierte Firmware
und alle *.espupd-Dateien verworfen.

Erst nach erfolgreicher Signaturprüfung wird committed:

1. Temporäre <Name>.espupd-Dateien ersetzen per remove + rename ihre bisherigen
   Zieldateien. Danach existieren regulär keine aktuellen *.espupd-Dateien mehr.
2. Nicht mehr in der neuen Dateitabelle vorhandene alte Dateien werden entfernt,
   damit ihr Platz für RAM-gepufferte Dateien zur Verfügung steht.
3. RAM-gepufferte Dateien werden geschrieben.
4. Erst nach erfolgreichem Filesystem-Commit wird die Firmware mit
   Update.end(true) für den nächsten Start vorgemerkt.

Verwaiste *.espupd-Dateien eines abgebrochenen früheren Uploads werden beim
Setup und unmittelbar vor jedem neuen Upload entfernt.

Das kompakte Dateipaket ist bewusst kein gzip-komprimiertes LittleFS-Abbild.
Der ESP8266-Core kann gzip-Dateisystemabbilder über eboot entpacken, benötigt
dafür aber ATOMIC_FS_UPDATE und damit einen Bootloader-Updateauftrag. Der
kombinierte Upload benötigt denselben einzelnen eboot-Auftrag bereits für die
Firmware. Das dateibasierte Paket vermeidet diesen Konflikt und reduziert die
Netzübertragung trotzdem auf Firmware plus tatsächlich benötigte LittleFS-
Dateien.

Ein Stromausfall während des Staging lässt die bisherigen Nutzdateien bestehen.
Der eigentliche Commit nach erfolgreicher Signaturprüfung ist dagegen noch
nicht vollständig transaktionssicher; die neue Firmware wird aber erst ganz am
Ende zum Start vorgemerkt.


Fortschrittsanzeige

Die Webformulare werden per XMLHttpRequest übertragen. Während des Uploads zeigt
die Seite Prozent sowie übertragene und gesamte Bytes an.

Wenn JavaScript das Formular sendet, ergänzt es das Multipart-Feld
_EspUpdateProgress=1. Der Server antwortet dann als gestreamte Textantwort. Beim
kompakten *.update.signed wird der eigentliche LittleFS-Commit erst im normalen
POST-Handler nach vollständig eingelesenem Multipart-Request ausgeführt. Dadurch
kann derselbe HTTP-Request während des Commits Statuszeilen zurückliefern:

- PROCESS
- Files changed: <geändert>, deleted: <gelöscht>, total: <gesamt>
- FILES <erledigt> <gesamt>
- FIRMWARE
- RESTART
- ERROR <Text>

Nach Pass 1 werden die Vergleichszähler beim anschließenden Verarbeiten als eine
Zeile ausgegeben. Nach jeder erfolgreich aktivierten bzw. aus RAM geschriebenen
geänderten LittleFS-Datei wird FILES aktualisiert; identische Dateien zählen
dabei bereits als erledigt. Obsolete Dateien werden nicht einzeln in FILES
gezählt. Nach RESTART wartet der Browser auf /Index.html und lädt die Seite nach
Wiedererreichbarkeit automatisch neu.

Ohne _EspUpdateProgress bleibt die bisherige HTML-Ergebnisantwort erhalten.


Recovery

SetupRecovery() bietet Firmware und vollständiges LittleFS gemeinsam in einem
Multipart-Request an. Es kann Firmware, LittleFS oder beides ausgewählt werden;
bei Firmware + LittleFS erfolgt genau ein Neustart nach beiden erfolgreichen
Uploads. Config.json bleibt ein separater Recovery-Upload. Das kompakte
*.update.signed/LFSUPD3-Paket ist im Recovery-Modus bewusst nicht zulässig, da
es ein lesbares bestehendes LittleFS für Vergleich und Staging voraussetzt.


ESP32

Der ESP32-Recovery-Uploader akzeptiert DeviceType-typisierte Firmware-,
LittleFS- und Config-Artefakte. Der 64-Byte-DeviceType-Header wird vor dem
Flashen geprüft und nicht mit in die Zielpartition geschrieben. Der ESP32-
WebUpdater verändert dabei weder Bootloader noch Partitionstabelle. SetupRecovery() und
SetupStartupError() stehen auch auf ESP32 zur Verfügung. Offensichtliche
Full-Flash-Artefakte (*.bootloader.bin, *.partitions.bin, *.boot_app0.bin,
*.factory.bin, *.merged.bin) werden vom Firmware-Upload abgewiesen. Die
signierten LFSUPD3-/Combined-Update-Wege bleiben ESP8266-spezifisch.
