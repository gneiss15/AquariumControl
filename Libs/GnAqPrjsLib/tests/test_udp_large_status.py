# test_udp_large_status.py, Version: 1.08
"""Exercise complete datagrams and the watched peer using the real socket receive path."""
import pathlib,subprocess,tempfile
lib=pathlib.Path(__file__).resolve().parents[1]
program=r'''
#include <GnBasics.h>
#include "Udp.cpp"
#include <cassert>
int main(int argc,char ** argv){
 assert(argc==2);
 TGnUdpControl main, dashboard, observer;
 assert(main.Setup(44501)&&dashboard.Setup(44502)&&observer.Setup());
 assert(GnUdpDescriptionPort("02:11:22:33:44:55@1")==44501);
 assert(observer.Send("target-main","127.0.0.1",44501));
 TGNString text,ip;uint16_t port;
 assert(main.Read(text,ip,port)&&text=="target-main");
 assert(!dashboard.Read(text,ip,port));
 assert(main.Send("reply",ip.empty()?"127.0.0.1":ip,port));
 assert(observer.Read(text,ip,port)&&text=="reply");
 TGnUdpStatus::TImplementation delayed;
 assert(delayed.Setup("LegacyFunctionName",[](TGnWireValue Status){return Status.Object();},{}));
 delayed.Loop();assert(!delayed.FControlReady); // Identity not initialized yet.
 GnDeviceIdentity.Setup("Test",4,argv[1]);
 delayed.FNetworkCheckKnown=true;delayed.FNetworkCheckDue=false;delayed.FNetworkConnected=false;
 delayed.Loop();assert(delayed.FControlReady&&delayed.FId==GnDeviceIdentity.WireId());
 assert(delayed.FId!="LegacyFunctionName");close(delayed.FSocket);
 TGnUdpStatus::TImplementation u;u.FReady=true;
 u.FSocket=socket(AF_INET,SOCK_DGRAM,0);assert(u.FSocket>=0);
 sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 assert(bind(u.FSocket,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0);
 socklen_t size=sizeof(address);assert(getsockname(u.FSocket,reinterpret_cast<sockaddr*>(&address),&size)==0);
 int sender=socket(AF_INET,SOCK_DGRAM,0);assert(sender>=0);
 const TGNString id="B8:27:EB:7B:96:D7@1";
 u.WatchFloorStatus(id,"0");
 const auto prefix=TGNString("{\"Ver\":")+std::to_string(GnUdpStatusVersion)+",\"Type\":"+
   std::to_string(static_cast<unsigned>(TMsgType::Status))+",\"ID\":\""+id+"\"";
 auto SendStatus = [&]( TGNString const & Packet )
  {
   assert(sendto(sender,Packet.data(),Packet.size(),0,reinterpret_cast<sockaddr*>(&address),sizeof(address))==static_cast<ssize_t>(Packet.size()));
   u.ReceivePackets();
  };
 const auto endpoint = id + "/0";
 TGnUdpChannelBits ActiveOverride{};ActiveOverride.Bit.OvrActive=true;ActiveOverride.Bit.OvrActiveMsk=true;
 TGnUdpChannelBits CalibrationOverride{};CalibrationOverride.Bit.CalOvrActive=true;CalibrationOverride.Bit.CalOvrActiveMsk=true;
 const auto status=TGNString("{\"Padding\":\"")+TGNString(6000,'x')+
   "\",\"BootCnt\":123,\"CurrentTime\":100,\"TimeSynchronized\":true,"
   "\"Chs\":[{\"Id\":\""+endpoint+"\",\"State\":true,\"OvrActive\":false}],"
   "\"Meas\":[{\"Kind\":\"RoomTemperature\",\"Valid\":true,\"Value\":24.5,\"UpdatedAt\":99}]}";
 SendStatus(prefix+",\"Status\":"+status+"}");
 TGnUdpFloorStatus result;
 assert(u.GetWatchedFloorStatus(result) && result.BootCounter==123 && result.CurrentTime==100);
 assert(result.FloorValid && result.FloorOn && !result.FloorOverride);
 assert(result.RoomValid && result.RoomTemperature==24.5 && result.RoomUpdatedAt==99);
 const auto compactPrefix = prefix + ",\"Compact\":true,\"InfoRev\":\"0123456789abcdef\",\"Status\":{";
 const auto compactStatus = TGNString("\"BootCnt\":124,\"CurrentTime\":101,\"TimeSynchronized\":true,\"Chs\":[{\"Id\":\"") + endpoint +
   "\",\"State\":false,\"Bits\":" + std::to_string(ActiveOverride.All) + "}],\"Padding\":\"" + TGNString(9000,'x') + "\"";
 const auto oversized = compactPrefix + compactStatus + "}}";
 SendStatus(oversized);
 assert(u.GetWatchedFloorStatus(result) && result.BootCounter==124 && result.CurrentTime==101);
 assert(!result.FloorValid && !result.FloorOn && result.FloorOverride && !result.RoomValid);
 const auto compactBase = compactPrefix + "\"BootCnt\":124,\"CurrentTime\":102,\"TimeSynchronized\":true,\"Chs\":[{\"Id\":\""+endpoint+"\",";
 SendStatus(compactBase+"\"State\":true,\"Bits\":"+std::to_string(CalibrationOverride.All)+"}]}}");
 assert(u.GetWatchedFloorStatus(result) && result.FloorOn && !result.FloorValid && result.FloorOverride);
 SendStatus(compactBase+"\"State\":null}]}}");
 assert(u.GetWatchedFloorStatus(result) && !result.FloorValid && !result.FloorOverride);
 SendStatus(compactBase+"\"State\":false}]}}");
 assert(u.GetWatchedFloorStatus(result) && result.FloorValid && !result.FloorOn);
 u.FWatchedReceivedAt -= 60001;
 assert(!u.GetWatchedFloorStatus(result));
 SendStatus(compactBase+"\"State\":true}]}}");
 assert(u.GetWatchedFloorStatus(result) && result.FloorValid && result.FloorOn);
 u.WatchFloorStatus(id,"1");
 assert(!u.GetWatchedFloorStatus(result));
 SendStatus(compactBase+"\"State\":true}]}}");
 assert(u.GetWatchedFloorStatus(result) && !result.FloorValid);
 TGnUdpControl control;assert(control.Setup(44599));
 assert(control.Send(oversized,"127.0.0.1",44599));
 TGNString received,peer;
 assert(control.Read(received,peer,port)&&received==oversized);
 u.WatchFloorStatus({},{});assert(!u.GetWatchedFloorStatus(result));close(sender);close(u.FSocket);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp);(p/'GnUdpStatus.h').write_text((lib/'GnUdpStatus.h').read_text().replace(' private:',' public:'))
 (p/'Udp.cpp').write_text((lib/'GnUdpStatus.cpp').read_text().replace(' private:',' public:'));(p/'test.cpp').write_text(program)
 cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-DSYSTEM_DUMMY','-I'+tmp,'-I'+str(lib),'-I'+str(lib.parent/'GnBasicLib'),str(p/'test.cpp')]
 cmd += [str((lib.parent/'GnBasicLib' if n in ('GnBasics','GnTime','GnCoopTimer') else lib)/(n+'.cpp')) for n in ['GnBasics','GnTime','GnCoopTimer','GnDeviceIdentity']]
 subprocess.run(cmd+['-ljson-c','-lpthread','-o',str(p/'test')],check=True);subprocess.run([str(p/'test'),str(p/'Identity.json')],check=True)
print('Large full and compact datagrams, typed floor feedback, override, unknown state and expiry passed')
