#!/usr/bin/env python3
# GenerateJs.py, Version: 1.00
"""Generate the browser message codec from the shared .inc.h definitions."""
import argparse
import os
from pathlib import Path
import tempfile

from CollectDefinitions import Collect, ROOT
from GenerateJsonBrowser import Generate


def Write(Target, Text):
  Target.parent.mkdir(parents=True, exist_ok=True)
  if Target.exists() and Target.read_text() == Text:
    return
  Descriptor, Temporary = tempfile.mkstemp(prefix='.' + Target.name + '.', dir=Target.parent)
  try:
    with os.fdopen(Descriptor, 'w') as Stream:
      Stream.write(Text)
    os.replace(Temporary, Target)
  finally:
    if os.path.exists(Temporary):
      os.unlink(Temporary)


def main():
  Parser = argparse.ArgumentParser(description=__doc__)
  Parser.add_argument('--out', type=Path, default=ROOT / 'Messages.js', help='Output JavaScript file')
  Args = Parser.parse_args()

  Types, Messages, _ = Collect(['Do_ContractAll'], 'MessageDefinition.inc.h')
  Versions = {Field['Value'] for Name in Messages.values() for Field in Types[Name]['Fields']
              if Field['Kind'] == 'Const' and Field['Name'] in ('Version', 'Ver')}
  if len(Versions) != 1:
    raise ValueError('Inconsistent UDP protocol version')

  Notice = 'Generated file, do not modify. Modifications must be made in MessageDefinition.inc.h and its domain definitions.'
  Write(Args.out, Generate(Types, True, Notice))
  print('Generated browser codec:', Args.out, '(', len(Messages), 'message codecs )')


if __name__ == '__main__':
  main()
