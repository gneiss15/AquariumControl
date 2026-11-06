#!/usr/bin/env python3
# GenerateUdp.py, Version: 1.24
"""Generate Python, browser codecs and documentation from preprocessed definitions.

C++ uses the definitions directly during its normal compilation. This tool does
not create C++ source files or headers.
"""
import argparse
import json
import os
from pathlib import Path
import re
import tempfile

from CollectDefinitions import Collect, CollectSettings, ROOT
from GenerateDocumentation import GenerateMarkdown, GenerateHtml
from GenerateJsonBrowser import Generate as GenerateBrowser

TEMPLATES = Path(__file__).parent/'Templates'


def Write(Target,Text):
  """Publish atomically, retaining mtime when the generated content is unchanged."""
  Target.parent.mkdir(parents=True,exist_ok=True)
  if Target.exists() and Target.read_text()==Text:
    return
  Descriptor,Temporary=tempfile.mkstemp(prefix='.'+Target.name+'.',dir=Target.parent)
  try:
    with os.fdopen(Descriptor,'w') as Stream:
      Stream.write(Text)
    os.replace(Temporary,Target)
  finally:
    if os.path.exists(Temporary):
      os.unlink(Temporary)


def ReadDefinitions(Args):
  Select=Args.select or ['Do_ContractAll']
  if any(not re.fullmatch(r'Do_[A-Za-z0-9_]+|DoMsg_[A-Za-z0-9_]+',Name) for Name in Select):
    raise ValueError('Invalid selection')
  Types,Messages,Bits=Collect(Select,'MessageDefinition.inc.h')
  Versions={Field['Value'] for Name in Messages.values() for Field in Types[Name]['Fields']
            if Field['Kind']=='Const' and Field['Name'] in ('Version','Ver')}
  if len(Versions)!=1:
    raise ValueError('Inconsistent UDP protocol version')
  return Types,Messages,Bits


def RenderPython(Types,Messages,Bits,Notice):
  Model={'TYPES':Types,'MESSAGES':Messages,'BITS':Bits,'ACCEPT_NULL':True}
  Text='# '+Notice+'\n'
  Text+=''.join(Name+' = '+repr(Value)+'\n' for Name,Value in Model.items())
  Text+='from enum import IntEnum\n'
  for Name,Type in Types.items():
    if 'Enum' in Type:
      Values={Item['Name']:Item['Value'] for Item in Type['Enum']}
      Text+=f'{Name} = IntEnum({Name!r}, {Values!r})\n'
  Text+=(TEMPLATES/'JsonRuntime.py').read_text()
  FactoryTemplate=(TEMPLATES/'PythonFactory.py.txt').read_text()
  Enum=Types.get('TMsgType',{}).get('Enum',[])
  for Tag,Name in Messages.items():
    Constants={Field['Name']:Field['Value'] for Field in Types[Name]['Fields'] if Field['Kind']=='Const'}
    Factory=next((Item['Name'] for Item in Enum if Item['Value']==Tag),str(Tag))
    Text+=FactoryTemplate.format(Factory=Factory,Constants=repr(Constants))
  Text+='\n'+''.join(Key+' = '+repr(Value)+'\n' for Key,Value in CollectSettings().items())
  return Text


def PublishCode(Args,Python,Browser):
  Out=Args.out or ROOT
  BrowserName='Messages.js'
  PythonName='Messages.py'
  Write(Out/BrowserName,Browser)
  Write(Out/PythonName,Python)
  if Args.out is not None:
    return
  Aquarium=ROOT.parents[1]
  for Consumer in ['Dashboard','MultiTimer/WebSrc','Common/Feeder/WebSrc','Common/TmrSw/WebSrc','HeatHistory']:
    if (Aquarium/Consumer).is_dir():
      Write(Aquarium/Consumer/BrowserName,Browser)
  for Consumer in ['HeatHistory','LaptopBattery','RoomTemperature']:
    if (Aquarium/Consumer).is_dir():
      Write(Aquarium/Consumer/PythonName,Python)


def PublishDocumentation(Args,Types,Messages,Bits,Notice):
  Out=(Args.out or ROOT)/'generated'/'MessageSchema'
  Write(Out/'Schema.html',GenerateHtml(Types,Notice))
  Write(Out/'Schema.md',GenerateMarkdown(Types,Messages,Bits,Notice))
  Model={'Generated':Notice,'Types':Types,'Messages':Messages,'Bits':Bits}
  Write(Out/'Schema.json',json.dumps(Model,indent=2)+'\n')


def main():
  Parser=argparse.ArgumentParser(description=__doc__)
  Parser.add_argument('--select',action='append',help='Definition selection macro; repeat for multiple selections')
  Parser.add_argument('--out',type=Path,help='Write to a staging folder instead of publishing to consumers')
  Args=Parser.parse_args()
  Types,Messages,Bits=ReadDefinitions(Args)
  Definition='MessageDefinition.inc.h and its domain definitions'
  Notice='Generated file, do not modify. Modifications must be made in '+Definition+'.'
  Python=RenderPython(Types,Messages,Bits,Notice)
  Browser=GenerateBrowser(Types,True,Notice)
  PublishCode(Args,Python,Browser)
  PublishDocumentation(Args,Types,Messages,Bits,Notice)
  print('Generated',len(Messages),'message codecs and',len(Types),'type definitions')


if __name__=='__main__':
  main()
