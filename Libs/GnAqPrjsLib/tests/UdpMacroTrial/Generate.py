#!/usr/bin/env python3
# Generate.py, Version: 1.02
"""Generate wrappers, run the C preprocessor, render its metadata for non-C targets."""
import argparse
import html
import json
import os
from pathlib import Path
import pprint
import subprocess

CPP_DEFINES = r'''
#define JD_BEGIN(Name, Description) struct Name {
#define JD_CONST(Type, Name, Value) Type Name = Value;
#define JD_Required(Type) Type
#define JD_Optional(Type) TJsonField<Type>
#define JD_RequiredNullable(Type) TJsonField<Type>
#define JD_OptionalNullable(Type) TJsonField<Type>
#define JD(Type, Name, Presence, Rule, Description) JD_ ## Presence(Type) Name{};
#define JD_END(Name) };
'''
META_DEFINES = r'''
#define JD_BEGIN(Name, Description) {"Kind":"Begin","Name":#Name,"Description":Description}
#define JD_CONST(Type, Name, Value) {"Kind":"Const","Type":#Type,"Name":#Name,"Value":Value}
#define JD(Type, Name, Presence, Rule, Description) {"Kind":"Field","Type":#Type,"Name":#Name,"Presence":#Presence,"Rule":#Rule,"Description":Description}
#define JD_END(Name) {"Kind":"End","Name":#Name}
'''

def Preprocess(Definition, Defines, Wrapper, Selections):
  # The actual wrapper is kept with the outputs so the experiment is inspectable.
  Wrapper.write_text('// Generated file, do not modify. Modifications must be made in '+Definition.name+' or '+Path(__file__).name+'.\n'+''.join('#define '+Selection+'\n' for Selection in Selections)+Defines+'\n#include '+json.dumps(str(Definition))+'\n')
  return subprocess.check_output([os.environ.get('CPP','gcc'),'-E','-P','-x','c',str(Wrapper)],text=True)

def Collect(Text):
  Types={};Current=None
  for Line in Text.splitlines():
    if not Line.strip():continue
    Row=json.loads(Line)
    if Row['Kind']=='Begin':
      if Current is not None or Row['Name'] in Types:raise ValueError('Invalid object start')
      Current={'Description':Row['Description'],'Fields':[]};Types[Row['Name']]=Current
    elif Row['Kind']=='End':
      if Current is not Types.get(Row['Name']):raise ValueError('Mismatched object end')
      Current=None
    else:
      if Current is None:raise ValueError('Field outside object')
      Current['Fields'].append(Row)
  if Current is not None:raise ValueError('Missing object end')
  return Types

def main():
  Parser=argparse.ArgumentParser();Parser.add_argument('--out',type=Path,required=True)
  Parser.add_argument('--select',action='append',choices=['Do_All','Do_Observer_TX','Do_Observer_RX','Do_Service_TX','Do_Service_RX','DoMsg_InfoRequest','DoMsg_Info','Do_TestTypes'])
  Args=Parser.parse_args();Selections=Args.select or ['Do_All'];Root=Path(__file__).resolve().parent;Definition=Root/'Example.inc.h'
  Out=Args.out.resolve();Out.mkdir(parents=True,exist_ok=True)
  Notice='Generated file, do not modify. Modifications must be made in '+Definition.name+'.'
  Cpp=Preprocess(Definition,CPP_DEFINES,Out/'Structs.wrapper.h',Selections)
  Meta=Preprocess(Definition,META_DEFINES,Out/'Metadata.wrapper.h',Selections)
  (Out/'Metadata.preprocessed.jsonl').write_text(json.dumps({'Generated':Notice})+'\n'+Meta)
  Types=Collect(Meta)
  # These aliases are adapters for language types, not repeated packet fields.
  Prefix='''#pragma once
#include <cstdint>
#include <string>
#include <vector>
using TGNString = std::string;
using TStringList = std::vector<TGNString>;
template<class T> struct TJsonField { bool Present=false; bool IsNull=false; T Value{}; };
struct TExampleMeasurement;
using TMeasurementList = std::vector<TExampleMeasurement>;
'''
  (Out/'Structs.h').write_text('// '+Notice+'\n'+Prefix+Cpp)
  (Out/'Schema.json').write_text(json.dumps({'Generated':Notice,'Types':Types},indent=2)+'\n')
  Messages={}
  for Name,Type in Types.items():
    for Field in Type['Fields']:
      if Field['Kind']=='Const' and Field['Name']=='Type':
        Tag=Field['Value']
        if not isinstance(Tag,str) or not Tag.isidentifier() or Tag in Messages:
          raise ValueError('Invalid or duplicate message Type: '+str(Tag))
        Messages[Tag]=Name
  Dispatcher=['// '+Notice,'#pragma once','#include <GnWireJson.h>',
    'enum class TUdpMessageType { Unknown'+''.join(', '+Tag for Tag in Messages)+' };',
    'inline TUdpMessageType UdpMessageType(TGNString const & Type) {']
  for Tag in Messages:
    Dispatcher.append('  if(Type=='+json.dumps(Tag)+')return TUdpMessageType::'+Tag+';')
  Dispatcher+=['  return TUdpMessageType::Unknown;','}',
    'template<class TRx> bool DispatchUdpMessage(TGnWireValue Message, TRx & Receiver) {',
    '  if(!Message.Object())return false;',
    '  const auto Type=Message.Get("Type").Text();',
    '  switch(UdpMessageType(Type)) {']
  for Tag in Messages:
    Dispatcher.append('    case TUdpMessageType::'+Tag+': Receiver.RxMsg_'+Tag+'(Message); return true;')
  Dispatcher+=['    default: return false;','  }','}']
  (Out/'Dispatcher.h').write_text('\n'.join(Dispatcher)+'\n')
  (Out/'Schema.py').write_text('# '+Notice+'\nTYPES = '+pprint.pformat(Types,sort_dicts=False)+'\n')
  Rows=[]
  for Name,Type in Types.items():
    Rows.append('<h2>'+html.escape(Name)+'</h2><p>'+html.escape(Type['Description'])+'</p>')
    Rows.append('<table><tr><th>Field</th><th>Type</th><th>Presence</th><th>Rule</th><th>Description</th></tr>')
    for Field in Type['Fields']:
      Cells=[Field['Name'],Field['Type'],Field.get('Presence','Required'),
             Field.get('Rule','Equal('+json.dumps(Field.get('Value'))+')'),Field.get('Description','')]
      Rows.append('<tr>'+''.join('<td>'+html.escape(Cell)+'</td>' for Cell in Cells)+'</tr>')
    Rows.append('</table>')
  (Out/'Schema.html').write_text('<!-- '+html.escape(Notice)+' -->\n<!doctype html><meta charset="utf-8"><title>UDP macro trial</title>\n'+''.join(Rows))

if __name__=='__main__':main()
