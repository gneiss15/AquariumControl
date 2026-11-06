# test_udp_compact.py, Version: 1.05
"""Real codec roundtrips and byte budgets using recorded public status examples."""
import json
import pathlib
import subprocess
import tempfile
lib = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='aq-compact-') as directory:
  tmp = pathlib.Path(directory)
  (tmp/'test.cpp').write_text(r'''#include <GnBasics.h>
#include <GnUdpCompact.h>
#include <GnUdpSources.h>
#include <cassert>
#include <iostream>
int main(void) {
 std::string Text;
 while(std::getline(std::cin,Text)) {
  TGnWireDocument Original(Text);auto O=Original.Root();TGnUdpInfoParts Parts;
  auto Compact=GnUdpCompactStatus(O.Get("ID").Text(),O.Get("Status").Json(),Parts);assert(!Compact.empty());
  TGnUdpDescriptions Cache;TGNString Request;
  assert(Cache.Expand(Compact,1000,Request).empty()&&!Request.empty());
  TGnWireDocument Q(Request);std::vector<TGNString> Fields;for(auto V:Q.Root().Get("Fields").Items())Fields.push_back(V.Text());
  auto Replies=GnUdpInfoReplies(O.Get("ID").Text(),GnUdpInfoHash(Parts),Parts,Fields);
  assert(Replies.size()==Fields.size());
  for(auto const & R:Replies){
   assert(Cache.AcceptInfo(R));
   TGnWireDocument Info(R);assert(!Info.Root().Has("Chunk")&&!Info.Root().Has("Chunks"));
  }
  if(O.Get("Status").Get("Channels").At(0).Get("Error").Text().size()==2000)
   { bool Large=false;for(auto const & R:Replies)Large=Large||R.size()>1472;assert(Large); }
  auto Full=Cache.Expand(Compact,2000,Request);assert(!Full.empty()&&Request.empty());
  TGnWireDocument Decoded(Full);auto S=Decoded.Root().Get("Status");
  assert(S.Get("DeviceId").Text()==O.Get("ID").Text());
  auto Before=O.Get("Status").Get("Channels").Items(),After=S.Get("Channels").Items();assert(Before.size()==After.size());
  for(size_t I=0;I<Before.size();++I)for(auto Key:{"Id","Name","State","InputState","HasInput","HasOutput","AutomaticState","OverrideActive","OverrideUntil","NextChange","Error"})
    if(Before[I].Has(Key)&&Before[I].Get(Key).Json()!=After[I].Get(Key).Json()){std::cerr<<Key<<" before="<<Before[I].Get(Key).Json()<<" after="<<After[I].Get(Key).Json()<<"\n";assert(false);}
  assert(!Cache.Expand(Compact,100000000,Request).empty()&&Request.empty()); // No time expiry.
  TGnWireDocument Cross;Cross.Root().Text("compact",Compact);Cross.Root().Text("full",Full);
  auto ReplyRows=Cross.Root().NewArray("replies");for(auto const & R:Replies)ReplyRows.AddText(R);
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
 assert(Sources.Resolve("Temp","",Rev,0,Request).empty()); // Equal revision conflict never guessed.
 assert(Sources.Resolve("Temp",Id,Rev,0,Request)==Id); // Confirmed local fallback.
}
''')
  subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-DSYSTEM_DUMMY','-I'+str(lib),'-I'+str(lib.parent/'GnBasicLib'),str(tmp/'test.cpp'),'-ljson-c','-o',str(tmp/'test')],check=True)
  audit = lib.parents[1]/'HeatHistory/generated/PiRollout-20261027/udp-size-audit.json'
  # Use a self-contained eight-channel fixture; private/live data is never needed.
  status = dict(DeviceId='02:11:22:33:44:55@1', Identity=dict(Version=3,DeviceId='02:11:22:33:44:55@1',BootCounter=3,Mode='MAC',Function='MultiTimer',Sources={}),CurrentTime=1793090900,Healthy=True,
    Channels=[dict(Id=i,Name='HeizungBodenAq'+str(i),HardwareChannel=i,HasInput=True,HasOutput=True,CapabilityId=1,State=True,AutomaticState=False,InputState=True,OverrideActive=True,OverrideState=True,OverrideUntil=1793099999,NextChange=None,Error=None) for i in range(8)], Measurements=[dict(Id='Temperature',Value=24.4,Valid=True,UpdatedAt=1793090900,Unit='°C',Name='Raumtemperatur')])
  status['Channels'][0]['Error']='x'*2000
  fixtures = [dict(Version=2,Type='Status',ID=status['DeviceId'],Status=status)]
  # Temporary learning event must fit the ordinary budget; permanent control is not exempt.
  laptop=dict(DeviceId='2C:BC:BB:C8:B5:F0@4',Identity={'BootCounter':21,'DeviceType':'TmrSwA8T'},CurrentTime=1793190000,TimeSynchronized=True,
    Channels=[dict(Id=0,Name='LaptopSw',State=False,AutomaticState=False,OverrideActive=False)],
    PowerMeter=dict(Voltage=230.1,Current=0,ActivePower=0,Valid=True),HeatControl=None,
    ChargeLearning=dict(Schema=1,Sequence=1,CompletedAt=1793190000,Profile='LaptopCharge',BeforePercent=40,AfterPercent=50,
      AppliedSecondsPerPercent=60,NewSecondsPerPercent=59,ChargeMilliseconds=600000,Persistence='RamOnly'))
  fixtures.append(dict(Version=2,Type='Status',ID=laptop['DeviceId'],Status=laptop))
  heater=dict(laptop);heater.pop('ChargeLearning');heater['HeatControl']=dict(Schema=1,Mode='Active',Profile='HeatProfileAq1',
    Config=dict(FloorDevice='B8:27:EB:7B:96:D7@1',FloorChannel='HeizungBodenAq1'),FloorRequest=True)
  fixtures.append(dict(Version=2,Type='Status',ID=heater['DeviceId'],Status=heater))
  if audit.exists(): fixtures += list(json.loads(audit.read_text())['examples'].values())
  run = subprocess.run([str(tmp/'test')],input='\n'.join(json.dumps(x,ensure_ascii=False) for x in fixtures),text=True,capture_output=True)
  if run.returncode: raise AssertionError(run.stderr)
  import sys
  sys.path.insert(0,str(lib.parents[1]/'HeatHistory'))
  from UdpDescriptions import UdpDescriptions
  for line in run.stdout.splitlines():
    row=json.loads(line); packet=json.loads(row['compact']); decoder=UdpDescriptions()
    sample, request=decoder.OnUdpPacket(packet,1,10)
    assert sample is None and request
    for reply in row['replies']:
      sample, request=decoder.OnUdpPacket(json.loads(reply),2,20)
    assert sample is not None and sample[0]==json.loads(row['full']) and request is None
    assert sample[1:]==(1,10) # Descriptions must not make held readings fresh.
    sample, request=decoder.OnUdpPacket(packet,1000000,10000000)
    assert sample[0]==json.loads(row['full']) and request is None
    changed=dict(packet,InfoRevision='0123456789abcdef')
    assert decoder.OnUdpPacket(changed,1000001,10000001)[0] is None
    assert decoder.OnUdpPacket(json.loads(row['replies'][0]),1000002,10000002)==(None,None)
  print(f'Compact C++/Python codec: {len(fixtures)} fixtures, full channel roundtrips, no TTL and source conflict handling passed.')
