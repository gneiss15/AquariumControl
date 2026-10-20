#!/usr/bin/env python3
# CreateEspUpdate.py, Version: 1.00

import argparse
from pathlib import Path
import struct

MAGIC = b"ESPUPD1\0"


def ParseArgs():
  Parser = argparse.ArgumentParser(
    description = "Create a combined firmware and compact LittleFS update payload" )
  Parser.add_argument( "Firmware", type = Path )
  Parser.add_argument( "LittleFsUpdate", type = Path )
  Parser.add_argument( "OutputFile", type = Path )
  return Parser.parse_args()


def Main():
  Args = ParseArgs()
  Firmware = Args.Firmware.read_bytes()
  LittleFsUpdate = Args.LittleFsUpdate.read_bytes()

  if not Firmware:
    raise SystemExit( "Firmware file is empty" )
  if not LittleFsUpdate:
    raise SystemExit( "Compact LittleFS update file is empty" )
  if len( Firmware ) > 0xFFFFFFFF:
    raise SystemExit( "Firmware file is too large" )

  Args.OutputFile.parent.mkdir( parents = True, exist_ok = True )
  with Args.OutputFile.open( "wb" ) as Output:
    Output.write( MAGIC )
    Output.write( struct.pack( "<I", len( Firmware ) ) )
    Output.write( Firmware )
    Output.write( LittleFsUpdate )

  print(
    f"Created combined update payload: {Args.OutputFile} "
    f"({len(Firmware)} firmware bytes, {len(LittleFsUpdate)} LittleFS package bytes, "
    f"{Args.OutputFile.stat().st_size} payload bytes)" )


if __name__ == "__main__":
  Main()
