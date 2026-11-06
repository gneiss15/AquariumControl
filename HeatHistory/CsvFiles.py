# CsvFiles.py, Version: 1.00
"""Keep recording files open; flush each complete row for concurrent readers."""
import csv
import os
from pathlib import Path

class CsvFiles:
  def __init__(self):
    self.files={}

  def create(self,path,header):
    path=Path(path)
    stream=path.open('x',encoding='utf-8',newline='')
    self.files[path]=(stream,csv.writer(stream,delimiter=';',lineterminator='\n'))
    self.write(path,header)

  def write(self,path,row,durable=False):
    path=Path(path)
    if path not in self.files:
      stream=path.open('a',encoding='utf-8',newline='')
      self.files[path]=(stream,csv.writer(stream,delimiter=';',lineterminator='\n'))
    stream,writer=self.files[path]
    writer.writerow(row)
    stream.flush()
    if durable:os.fsync(stream.fileno())

  def close(self,path):
    entry=self.files.pop(Path(path),None)
    if entry:entry[0].close()

  def close_all(self):
    for path in list(self.files):self.close(path)
