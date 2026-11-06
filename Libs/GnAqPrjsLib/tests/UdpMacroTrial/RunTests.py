#!/usr/bin/env python3
# RunTests.py, Version: 1.03
"""Exercise real preprocessing, C++ types and non-C generation from one definition."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parent

def Check(Output,DataName,Limit):
  subprocess.run([sys.executable,str(ROOT/'Generate.py'),'--out',str(Output)],check=True)
  Schema=json.loads((Output/'Schema.json').read_text())
  Spec=importlib.util.spec_from_file_location('generated_schema',Output/'Schema.py')
  Module=importlib.util.module_from_spec(Spec);Spec.loader.exec_module(Module)
  assert Schema['Types']==Module.TYPES
  Info=Module.TYPES['TGnUdpInfo']['Fields']
  assert [F['Name'] for F in Info]==['Version','Type','ID','InfoRevision','Field','Chunk','Chunks',DataName]
  assert Info[-1]['Rule']==f'Utf8Bytes({Limit})'
  assert Info[0]['Value']==2 and Info[1]['Value']=='Info'
  assert 'Range(1,64)' in (Output/'Metadata.preprocessed.jsonl').read_text()
  assert f'<td>{DataName}</td>' in (Output/'Schema.html').read_text()
  for Name in ['Structs.h','Structs.wrapper.h','Metadata.wrapper.h','Metadata.preprocessed.jsonl','Schema.json','Schema.py','Schema.html','Dispatcher.h']:
    assert 'Generated file, do not modify.' in '\n'.join((Output/Name).read_text().splitlines()[:3])
    assert str(ROOT) not in '\n'.join((Output/Name).read_text().splitlines()[:3])
    assert 'Example.inc.h' in '\n'.join((Output/Name).read_text().splitlines()[:3])
  Source=Output/'Check.cpp'
  Source.write_text('''#include "Structs.h"
#include <cassert>
#include <type_traits>
#include "Dispatcher.h"
struct TReceiver {
 unsigned Infos=0, Requests=0;
 void RxMsg_Info(TGnWireValue Message) { ++Infos; assert(Message.Get("Marker").UInt()==42); }
 void RxMsg_InfoRequest(TGnWireValue) { ++Requests; }
};
static_assert(std::is_same_v<decltype(TGnUdpInfo::Chunk), uint32_t>);
static_assert(std::is_same_v<decltype(TGnUdpInfoRequest::Fields), std::vector<TGNString>>);
static_assert(std::is_same_v<decltype(TExampleStatus::Measurement), TExampleMeasurement>);
static_assert(std::is_same_v<decltype(TExampleStatus::Measurements), TJsonField<std::vector<TExampleMeasurement>>>);
static_assert(std::is_same_v<decltype(TExampleStatus::Temperature), TJsonField<double>>);
int main() {
  TReceiver Receiver;
  TGnWireDocument Packet(R"({"Type":"Info","Marker":42})");
  assert(DispatchUdpMessage(Packet.Root(),Receiver));
  assert(Receiver.Infos==1 && Receiver.Requests==0);
  TGnWireDocument Unknown(R"({"Type":"Future"})");
  assert(!DispatchUdpMessage(Unknown.Root(),Receiver));
  TGnWireDocument Invalid(R"({"Type":3})");
  assert(!DispatchUdpMessage(Invalid.Root(),Receiver));
  TGnUdpInfo Info;
  assert(Info.Version==2 && Info.Type=="Info");
  TExampleStatus Status;
  assert(!Status.Temperature.Present);
  Status.Temperature.Present=true;
  Status.Temperature.IsNull=true;
  assert(Status.Temperature.Present && Status.Temperature.IsNull);
  Status.Temperature.IsNull=false;
  Status.Temperature.Value=28.0625;
  assert(Status.Temperature.Value==28.0625);
  Status.Measurements.Present=true;
  Status.Measurements.Value.push_back(Status.Measurement);
  assert(Status.Measurements.Value.size()==1);
}
'''.replace('  TGnUdpInfo Info;',f'  static_assert(std::is_same_v<decltype(TGnUdpInfo::{DataName}), TGNString>);\n  TGnUdpInfo Info;'))
  subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-DSYSTEM_DUMMY','-I'+str(Path(__file__).resolve().parents[3]/'GnBasicLib'),str(Source),'-ljson-c','-o',str(Output/'Check')],check=True)
  subprocess.run([str(Output/'Check')],check=True)

def CheckSelections(Work):
  for Group,Expected in [('Do_Observer_RX','Info'),('Do_Observer_TX','InfoRequest'),
                         ('Do_Service_TX','Info'),('Do_Service_RX','InfoRequest')]:
    Output=Work/Group
    subprocess.run([sys.executable,str(ROOT/'Generate.py'),'--out',str(Output),'--select',Group],check=True)
    Types=json.loads((Output/'Schema.json').read_text())['Types']
    assert list(Types)==['TGnUdp'+Expected]
    Dispatcher=(Output/'Dispatcher.h').read_text()
    assert 'case TUdpMessageType::'+Expected+':' in Dispatcher
    assert Dispatcher.count('case TUdpMessageType::')==1
    Driver=Output/'Dispatch.cpp'
    Driver.write_text('#include "Dispatcher.h"\n#include <cassert>\n'
      +'struct TRx { unsigned Calls=0; void RxMsg_'+Expected+'(TGnWireValue) { ++Calls; } };\n'
      +'int main() { TGnWireDocument Packet(R"({"Type":"'+Expected+'"})"); TRx Rx; '
      +'assert(DispatchUdpMessage(Packet.Root(),Rx)); assert(Rx.Calls==1); }\n')
    subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-DSYSTEM_DUMMY',
      '-I'+str(Path(__file__).resolve().parents[3]/'GnBasicLib'),str(Driver),'-ljson-c',
      '-o',str(Output/'Dispatch')],check=True)
    subprocess.run([str(Output/'Dispatch')],check=True)
    # With macros defined again but no selection, a second include emits nothing.
    Wrapper=Work/(Group+'.h')
    Text=(Output/'Structs.wrapper.h').read_text()
    Wrapper.write_text(Text+'\n'+Text.replace('#define '+Group+'\n',''))
    Result=subprocess.run(['gcc','-E','-P','-x','c',str(Wrapper)],capture_output=True,text=True)
    assert Result.returncode==0
    assert Result.stdout.count('struct TGnUdp'+Expected+' {')==1

def CheckMissingMacros(Work):
  Defines={
    'JD_BEGIN':'#define JD_BEGIN(Name,Description)',
    'JD_CONST':'#define JD_CONST(Type,Name,Value)',
    'JD':'#define JD(Type,Name,Presence,Rule,Description)',
    'JD_END':'#define JD_END(Name)',
  }
  for Missing in Defines:
    Wrapper=Work/(Missing+'.h')
    Wrapper.write_text('\n'.join(Text for Name,Text in Defines.items() if Name!=Missing)
      +'\n#include '+json.dumps(str(ROOT/'Example.inc.h'))+'\n')
    Result=subprocess.run(['gcc','-E','-P','-x','c',str(Wrapper)],text=True,capture_output=True)
    assert Result.returncode!=0
    assert 'Define '+Missing+' before including this file' in Result.stderr

if __name__=='__main__':
  with tempfile.TemporaryDirectory(prefix='udp-macro-test-') as Directory:
    Work=Path(Directory)
    CheckSelections(Work)
    CheckMissingMacros(Work)
    Check(Work/'original','Data',400)
    # Change only the definition; all generated targets must follow it.
    shutil.copy(ROOT/'Generate.py',Work/'Generate.py')
    Definition=(ROOT/'Example.inc.h').read_text().replace('TGNString, Data,','TGNString, Payload,').replace('Utf8Bytes(400)','Utf8Bytes(399)')
    (Work/'Example.inc.h').write_text(Definition)
    ROOT=Work
    Check(Work/'changed','Payload',399)
  print('PASS: preprocessing, C++ compile/run, Python/JSON parity, HTML, optional/null/arrays and single-source change, four macro guards and selected JSON-Type dispatchers')
