# test_udp_compact.py, Version: 1.09
"""Exercise compact-status decoding with packet-shaped fixtures."""
import json
import pathlib
import subprocess
import tempfile
import sys

lib = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(lib.parents[1] / 'HeatHistory'))
from Messages import TMsgType, ValidateMessage
from UdpDescriptions import UdpDescriptions

with tempfile.TemporaryDirectory(prefix='aq-compact-') as directory:
  tmp = pathlib.Path(directory)
  (tmp / 'test.cpp').write_text(r'''#define Do_Device_RX
#include <GnBasics.h>
#include <GnUdpCompact.h>
#include <GnUdpSources.h>
#include <cassert>
#include <iostream>
int main(void) {
 std::string Text;
 while(std::getline(std::cin,Text)) {
  TGnWireDocument Input(Text);auto O=Input.Root();auto Compact=O.Get("Packet").Json();
  TGnUdpInfoParts Parts;for(auto const & P:O.Get("Parts").Members())Parts[P.first]="{"+P.second.Text()+"}";
  TGnUdpDescriptions Cache;TGNString Request;
  assert(Cache.Expand(Compact,1000,Request).empty()&&!Request.empty());
  TGnWireDocument Query(Request);std::vector<TGNString> Fields;
  for(auto Field:Query.Root().Get("Fields").Items())Fields.push_back(Field.Text());
  auto Replies=GnUdpInfoReplies(O.Get("Packet").Get("ID").Text(),
    O.Get("Packet").Get("InfoRev").Text(),Parts,Fields);
  assert(Replies.size()==Fields.size());
  for(auto const & Reply:Replies)assert(Cache.AcceptInfo(Reply));
  auto Full=Cache.Expand(Compact,2000,Request);assert(!Full.empty()&&Request.empty());
  TGnWireDocument Next(Compact);auto Channel=Next.Root().Get("Status").Get("Chs").At(0);
  Channel.Bool("State",false);Channel.UInt("Bits",2);
  auto Incremental=Cache.Expand(Next.Root(),3000,Request);assert(!Incremental.empty()&&Request.empty());
  TGnWireDocument Cross;Cross.Root().Text("full",Full);Cross.Root().Text("incremental",Incremental);
  auto Rows=Cross.Root().NewArray("replies");for(auto const & Reply:Replies)Rows.AddText(Reply);
  std::cout<<Cross.Root().Json()<<"\n";
 }
 TGnUdpSources Sources;TGNString Request;
 const std::string Id="02:11:22:33:44:55@3",Rev="2026-10-27T10:00:00.000Z";
 assert(Sources.Resolve("Temp",Id,Rev,0,Request)==Id&&Request.empty());
 assert(Sources.Resolve("Missing","",Rev,0,Request).empty()&&!Request.empty());
 assert(Sources.Resolve("Missing","",Rev,1,Request).empty()&&Request.empty());
 assert(Sources.Accept(GnUdpSourceReply("02:11:22:33:44:66@4","Temp",Id,Rev)));
 assert(Sources.Resolve("Temp","",Rev,0,Request)==Id);
 assert(Sources.Accept(GnUdpSourceReply("02:11:22:33:44:66@4","Temp","02:11:22:33:44:77@3",Rev)));
 assert(Sources.Resolve("Temp","",Rev,0,Request).empty());
 assert(Sources.Resolve("Temp",Id,Rev,0,Request)==Id);
}
''')
  subprocess.run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-DSYSTEM_DUMMY',
                  '-I' + str(lib), '-I' + str(lib.parent / 'GnBasicLib'),
                  str(tmp / 'test.cpp'), '-ljson-c', '-o', str(tmp / 'test')], check=True)

  device = '02:11:22:33:44:55@1'
  channel_id = device + '/0'
  revision = '0123456789abcdef'
  parts = {
    'Root': json.dumps({'Kind': 'Feeder'}),
    'Identity': json.dumps({'Ver': 1, 'Func': 'FeederAq1', 'Prepared': True}),
    'Channel:' + channel_id: json.dumps({'Name': 'Floor Aq1', 'HwCh': 0, 'HasIn': True, 'HasOut': True,
                                         'NextChange': None, 'StateLabels': None, 'Err': None}),
    'Measurement:Temperature': json.dumps({'Name': 'Water', 'Unit': '°C', 'Decimals': 2}),
  }
  # InState=true, AutomaticState=false, OvrActive=true, OvrState=true.
  bits = 3 | 8 | 48 | 192
  compact = {
    'Ver': 3, 'Type': int(TMsgType.Status), 'ID': device, 'Compact': True,
    'InfoRev': revision,
    'Status': {
      'BootCnt': 3, 'CurrentTime': 1793090900, 'Healthy': True,
      'Chs': [{'Id': channel_id, 'State': True, 'Bits': bits, 'OvrUntil': 1793099999}],
      'Meas': [{'Kind': 'Temperature', 'Value': 24.4, 'Valid': True, 'UpdatedAt': 1793090900}],
    },
  }
  assert ValidateMessage(compact)
  payload = json.dumps({'Packet': compact, 'Parts': parts}, ensure_ascii=False)
  run = subprocess.run([str(tmp / 'test')], input=payload + '\n', text=True, capture_output=True)
  if run.returncode:
    raise AssertionError(run.stderr)
  row = json.loads(run.stdout.splitlines()[0])
  cpp_full = json.loads(row['full'])
  cpp_incremental = json.loads(row['incremental'])
  decoder = UdpDescriptions()
  sample, request = decoder.OnUdpPacket(compact, 1, 10)
  assert sample is None and request
  for reply in row['replies']:
    sample, request = decoder.OnUdpPacket(json.loads(reply), 2, 20)
  assert sample is not None and sample[0] == cpp_full and request is None
  assert sample[1:] == (1, 10)
  decoded_channel = sample[0]['Status']['Chs'][0]
  assert decoded_channel['InState'] is True and decoded_channel['AutomaticState'] is False
  assert decoded_channel['OvrActive'] is True and decoded_channel['OvrState'] is True
  incremental = dict(compact)
  incremental['Status'] = dict(compact['Status'])
  incremental['Status']['Chs'] = [dict(compact['Status']['Chs'][0], State=False, Bits=2)]
  sample, request = decoder.OnUdpPacket(incremental, 3, 30)
  assert sample is not None and sample[0] == cpp_incremental and request is None
  inc_channel = sample[0]['Status']['Chs'][0]
  assert inc_channel['InState'] is False
  assert inc_channel['AutomaticState'] is False and inc_channel['OvrActive'] is True and inc_channel['OvrState'] is True
  print('Compact C++/Python decoder: definition-shaped fields, validity-mask merge, Info parts and source resolution passed.')
