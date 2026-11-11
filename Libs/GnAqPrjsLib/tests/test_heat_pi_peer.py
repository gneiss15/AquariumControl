# test_heat_pi_peer.py, Version: 1.05
"""Exercise receiver leases and profile/endpoint binding using the production codec."""
import pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
program=r'''
#include <GnHeatPiPeer.h>
#include <MessageDispatch.h>
#include <cassert>

struct Receiver
{
 TGnHeatPiPeers Peers;
 void RxMsg_Status(TGnWireValue Packet, uint64_t Now, int64_t WallTime)
 { Peers.AcceptStatus(Packet,Now,WallTime); }
};

int main()
{
 const char * Main="02:11:22:33:44:55@4";
 const char * Floor="02:11:22:33:44:56@4";
 Receiver Rx;
 std::string Out;
 bool Awaiting=false;
 assert(!Rx.Peers.Get(Main,0,Out,Awaiting) && Awaiting);
 assert(!Rx.Peers.Get(Main,59999,Out,Awaiting) && Awaiting);
 assert(!Rx.Peers.Get(Main,60000,Out,Awaiting) && !Awaiting);
 TGnWireDocument Packet(R"({"Ver":3,"Type":2,"ID":"02:11:22:33:44:55@4",
   "Status":{"TimeSynchronized":true,"CurrentTime":1000,"HeatControl":{
     "Ver":1,"Active":true,"Prof":"Aq1","FloorEndpoint":"02:11:22:33:44:56@4/0","FloorRequest":true}}})");
 auto Root=Packet.Root();
 assert(DispatchMessage(Root,Rx,100,1000));
 assert(Rx.Peers.Get(Main,60100,Out,Awaiting) && !Awaiting);
 bool On=false;
 assert(GnHeatPiFloorRequest(Out,"Aq1",Floor,"0",On) && On);
 assert(!GnHeatPiFloorRequest(Out,"Aq2",Floor,"0",On));
 assert(!GnHeatPiFloorRequest(Out,"Aq1",Floor,"1",On));

 Root.UInt("Ver",2);
 assert(!DispatchMessage(Root,Rx,60000,1000));
 Root.UInt("Ver",3);
 assert(!Rx.Peers.Get(Main,60101,Out,Awaiting) && !Awaiting);

 auto Status=Root.Get("Status");
 auto Control=Status.Get("HeatControl");
 assert(DispatchMessage(Root,Rx,60200,1061));
 assert(!Rx.Peers.Get(Main,60200,Out,Awaiting) && !Awaiting);
 Status.Bool("TimeSynchronized",false);
 assert(DispatchMessage(Root,Rx,60300,1000));
 assert(!Rx.Peers.Get(Main,60300,Out,Awaiting) && !Awaiting);
 Status.Bool("TimeSynchronized",true);
 Control.Bool("Active",false);
 assert(DispatchMessage(Root,Rx,60400,1000));
 assert(Rx.Peers.Get(Main,60400,Out,Awaiting) && !Awaiting);
 assert(GnHeatPiFloorRequest(Out,"Aq1",Floor,"0",On) && !On);
 Status.SetNull("HeatControl");
 assert(DispatchMessage(Root,Rx,120000,1000));
 assert(!Rx.Peers.Get(Main,120401,Out,Awaiting) && !Awaiting);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp);(p/'GnBasics.h').write_text('#pragma once\n#include <cstdint>\n#include <string>\nusing TGNString=std::string;\n');(p/'test.cpp').write_text(program)
 subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-I'+tmp,'-I'+str(root),'-I'+str(root.parent/'GnBasicLib'),str(p/'test.cpp'),'-ljson-c','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PI peer lease/binding tests passed')
