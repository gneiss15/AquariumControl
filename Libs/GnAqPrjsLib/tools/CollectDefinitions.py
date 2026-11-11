# CollectDefinitions.py, Version: 1.02
"""Run the C preprocessor and collect its macro records; no target code generation."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parent.parent
MACROS = (Path(__file__).parent / 'Templates/CollectMacros.inc.h').read_text()

def Collect(Select, Definition='MessageDefinition.inc.h'):
    with tempfile.TemporaryDirectory(prefix='aq-udp-schema-') as Tmp:
        Wrapper = Path(Tmp) / 'Definitions.h'
        Wrapper.write_text(''.join(('#define ' + Name + '\n' for Name in Select)) + MACROS + '\n#include ' + json.dumps(str(ROOT / Definition)) + '\n')
        Text = subprocess.check_output([os.environ.get('CPP', 'gcc'), '-E', '-P', '-x', 'c', str(Wrapper)], text=True)
    Types = {}
    Bits = {}
    Current = None
    for Line in Text.splitlines():
        if not Line.strip():
            continue
        Row = json.loads(Line)
        if 'ValueText' in Row:
            Literal = Row.pop('ValueText')
            if '::' in Literal:
                Enum, Member = Literal.split('::')
                Row['Value'] = next((Item['Value'] for Item in Types[Enum]['Enum'] if Item['Name'] == Member))
            else:
                Row['Value'] = json.loads(Literal)
        if Row['Kind'] == 'Alias':
            if Current is not None or Row['Name'] in Types:
                raise ValueError('Duplicate/nested alias '+Row['Name'])
            Types[Row['Name']] = {'Description': Row['Description'], 'Fields': [], 'Alias': Row['Type'], 'Rule': Row['Rule']}
            continue
        if Row['Kind'] == 'EnumBegin':
            if Current is not None or Row['Name'] in Types:
                raise ValueError('Duplicate/nested enum '+Row['Name'])
            Current = {'Description': Row['Description'], 'Fields': [], 'Enum': [], 'BaseType': Row['Type']}
            Types[Row['Name']] = Current
            continue
        if Row['Kind'] == 'Enum':
            if Current is None or 'Enum' not in Current or any(Item['Name']==Row['Name'] or Item['Value']==Row['Value'] for Item in Current['Enum']):
                raise ValueError('Invalid/duplicate enum member '+Row['Name'])
            Current['Enum'].append(Row)
            continue
        if Row['Kind'] == 'EnumEnd':
            if Current is not Types.get(Row['Name']) or 'Enum' not in Current:
                raise ValueError('Mismatched enum end '+Row['Name'])
            Current = None
            continue
        if Row['Kind'] == 'Message':
            continue
        if Row['Kind'] == 'Bit':
            if Row['Name'] in Bits:
                raise ValueError('Duplicate bit')
            Position = len(Bits) * 2
            Bits[Row['Name']] = {
                'Value': 1 << Position,
                'Mask': 1 << (Position + 1),
                'Description': Row['Description'],
            }
        elif Row['Kind'] == 'Value':
            if Current is not None or Row['Name'] in Types:
                raise ValueError('Duplicate value contract')
            Types[Row['Name']] = {'Description': Row['Description'], 'Fields': [], 'ValueType': Row['Type'], 'Rule': Row['Rule']}
        elif Row['Kind'] in ('Begin', 'TupleBegin'):
            if Current is not None or Row['Name'] in Types:
                raise ValueError('Duplicate/nested schema ' + Row['Name'])
            Current = {'Description': Row['Description'], 'Fields': []}
            Types[Row['Name']] = Current
            if Row['Kind'] == 'TupleBegin':
                Current['Shape'] = 'Tuple'
        elif Row['Kind'] == 'Base':
            if Current is None or Current['Fields']:
                raise ValueError('Base must precede own fields')
            for Field in Types[Row['Name']]['Fields']:
                Copy = dict(Field)
                Copy['Inherited'] = True
                if Row['Presence'] == 'Optional' and Copy['Kind'] != 'Const':
                    Copy['Presence'] = 'OptionalNullable' if Copy['Presence'].endswith('Nullable') else 'Optional'
                Current['Fields'].append(Copy)
        elif Row['Kind'] == 'End':
            if Current is not Types.get(Row['Name']):
                raise ValueError('Mismatched schema end')
            Current = None
        else:
            if Current is None:
                raise ValueError('Field outside object')
            Previous = next((F for F in Current['Fields'] if F['Name'] == Row['Name']), None)
            if Previous:
                if not Previous.get('Inherited'):
                    raise ValueError('Duplicate field')
                Current['Fields'][Current['Fields'].index(Previous)] = Row
            else:
                Current['Fields'].append(Row)
    if Current is not None:
        raise ValueError('Unclosed definition')
    Messages = {}
    for Name, Type in Types.items():
        for F in Type['Fields']:
            if F['Kind'] == 'Const' and F['Name'] == 'Type':
                if F['Value'] in Messages:
                    raise ValueError('Duplicate message Type')
                Messages[F['Value']] = Name
    return (Types, Messages, Bits)

def CollectSettings():
    with tempfile.TemporaryDirectory(prefix='aq-transport-') as Tmp:
        Wrapper = Path(Tmp) / 'Transport.h'
        Wrapper.write_text('#define JD_SETTING(Type,Name,Value) {"Name":#Name,"Value":Value}\n#include ' + json.dumps(str(ROOT / 'TransportDefinition.inc.h')) + '\n')
        Text = subprocess.check_output([os.environ.get('CPP', 'gcc'), '-E', '-P', '-x', 'c', str(Wrapper)], text=True)
    return {Row['Name']: Row['Value'] for Row in map(json.loads, filter(str.strip, Text.splitlines()))}
