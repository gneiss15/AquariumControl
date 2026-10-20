#!/usr/bin/env python3
# CreateLittleFsUpdate.py, Version: 1.02

import argparse
from pathlib import Path
import struct

MAGIC = b"LFSUPD3\0"
MAX_FILE_COUNT = 64
MAX_PATH_BYTES = 31
TEMP_SUFFIX = ".espupd"
TEMP_SUFFIX_BYTES = len( TEMP_SUFFIX.encode( "utf-8" ) )


def ParseArgs():
  Parser = argparse.ArgumentParser(
    description = "Create compact LittleFS update payload source" )
  Parser.add_argument( "SourceDir", type = Path )
  Parser.add_argument( "FileSystemSize", type = int )
  Parser.add_argument( "OutputFile", type = Path )
  return Parser.parse_args()


def Main():
  Args = ParseArgs()
  SourceDir = Args.SourceDir.resolve()
  if not SourceDir.is_dir():
    raise SystemExit( f"Source directory not found: {SourceDir}" )
  if Args.FileSystemSize <= 0 or Args.FileSystemSize > 0xFFFFFFFF:
    raise SystemExit( "Invalid filesystem size" )

  Entries = sorted( SourceDir.iterdir(), key = lambda Entry: Entry.name )
  Directories = [ Entry for Entry in Entries if Entry.is_dir() ]
  if Directories:
    raise SystemExit(
      "Compact LittleFS update currently supports root files only: " +
      ", ".join( Entry.name for Entry in Directories ) )

  Files = [ Entry for Entry in Entries if Entry.is_file() ]
  if len( Files ) > MAX_FILE_COUNT:
    raise SystemExit( f"Too many LittleFS files: {len(Files)} > {MAX_FILE_COUNT}" )

  Records = []
  for SourceFile in Files:
    PathText = "/" + SourceFile.name
    if PathText.endswith( TEMP_SUFFIX ):
      raise SystemExit( f"Reserved LittleFS update suffix in path: {PathText}" )
    PathBytes = PathText.encode( "utf-8" )
    if len( PathBytes ) < 2 or len( PathBytes ) + TEMP_SUFFIX_BYTES > MAX_PATH_BYTES:
      raise SystemExit(
        f"LittleFS path is too long for temporary {TEMP_SUFFIX} suffix: {PathText}" )
    Data = SourceFile.read_bytes()
    if len( Data ) > 0xFFFFFFFF:
      raise SystemExit( f"File too large: {SourceFile}" )
    Records.append( ( PathBytes, Data ) )

  # Larger files first makes the ESP-side flash-first staging plan effective:
  # the largest payloads get the first opportunity to use free LittleFS space.
  Records.sort( key = lambda Record: ( -len( Record[1] ), Record[0] ) )

  DataBytes = sum( len( Data ) for _, Data in Records )
  if DataBytes > 0xFFFFFFFF:
    raise SystemExit( "Total LittleFS data size is too large" )

  Args.OutputFile.parent.mkdir( parents = True, exist_ok = True )
  with Args.OutputFile.open( "wb" ) as Output:
    Output.write( MAGIC )
    Output.write( struct.pack( "<IHI", Args.FileSystemSize, len( Records ), DataBytes ) )

    # The complete table precedes all file contents. The ESP therefore knows
    # every path and size before the first file data byte arrives.
    for PathBytes, Data in Records:
      Output.write( struct.pack( "<BI", len( PathBytes ), len( Data ) ) )
      Output.write( PathBytes )

    # Pass 1 lets the ESP compare every target file with the currently
    # installed LittleFS content. Pass 2 contains the same bytes again; the
    # ESP stages only files that Pass 1 found to be new or changed.
    for _ in range( 2 ):
      for _, Data in Records:
        Output.write( Data )

  print(
    f"Created compact LittleFS update: {Args.OutputFile} "
    f"({len(Records)} files, {DataBytes} data bytes/pass, "
    f"{Args.OutputFile.stat().st_size} package bytes)" )


if __name__ == "__main__":
  Main()
