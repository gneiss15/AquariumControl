#!/usr/bin/env bash
# SerialMonitorReset.sh, Version: 1.00

set -euo pipefail

Port="${1:-/dev/ttyUSB0}"
Baud="${2:-115200}"

python3 - "$Port" "$Baud" <<'PY'
import os
import sys
import time

try:
  import serial
except ImportError as Error:
  raise SystemExit(f"pyserial is required: {Error}")

Port = sys.argv[1]
Baud = int(sys.argv[2])

while not os.path.exists(Port):
  time.sleep(0.05)

Ser = serial.Serial()
Ser.port = Port
Ser.baudrate = Baud
Ser.timeout = 0.1
# Keep GPIO0 released while opening the USB-UART bridge. On the programmers
# used for these ESP8266 projects DTR participates in the auto-program circuit.
Ser.dtr = False
Ser.rts = False
Ser.open()

# Reset through RTS without closing the serial port, so no firmware output is
# lost between reset and opening the monitor.
time.sleep(0.05)
Ser.dtr = False
Ser.rts = True
time.sleep(0.10)
Ser.rts = False

try:
  while True:
    Data = Ser.read(1024)
    if Data:
      sys.stdout.write(Data.decode("utf-8", errors="replace"))
      sys.stdout.flush()
except KeyboardInterrupt:
  pass
finally:
  Ser.close()
PY
