# test_udp_large_status.py, Version: 1.03
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
 assert(delayed.Setup("LegacyFunctionName",[](uint64_t &){return TGNString("{}");},{}));
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
 const TGNString id="B8:27:EB:7B:96:D7@1";u.WatchStatus(id);
 const auto status=TGNString("{\"Padding\":\"")+TGNString(6000,'x')+"\",\"Tail\":123}";
 const auto packet=TGNString("{\"Version\":2,\"Type\":\"Status\",\"ID\":\"")+id+"\",\"Status\":"+status+"}";
 assert(sendto(sender,packet.data(),packet.size(),0,reinterpret_cast<sockaddr*>(&address),sizeof(address))==static_cast<ssize_t>(packet.size()));
 u.ReceivePackets();TGNString result;uint64_t revision=0;
 assert(u.GetWatchedStatus(id,result,revision)&&revision==1);
 auto* json=json_tokener_parse(result.c_str());assert(json);json_object* tail=nullptr;
 assert(json_object_object_get_ex(json,"Tail",&tail)&&json_object_get_int(tail)==123);json_object_put(json);
 const auto oversized=TGNString("{\"Version\":2,\"Type\":\"Status\",\"ID\":\"")+id+"\",\"Status\":{\"Padding\":\""+TGNString(9000,'x')+"\"}}";
 sendto(sender,oversized.data(),oversized.size(),0,reinterpret_cast<sockaddr*>(&address),sizeof(address));u.ReceivePackets();
 assert(u.GetWatchedStatus(id,result,revision)&&revision==2);
 TGnUdpControl control;assert(control.Setup(44599));
 assert(control.Send(oversized,"127.0.0.1",44599));
 TGNString received,peer;
 assert(control.Read(received,peer,port)&&received==oversized);
 u.WatchStatus({});assert(!u.GetWatchedStatus(id,result,revision));close(sender);close(u.FSocket);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp);(p/'GnUdpStatus.h').write_text((lib/'GnUdpStatus.h').read_text().replace(' private:',' public:'))
 (p/'Udp.cpp').write_text((lib/'GnUdpStatus.cpp').read_text().replace(' private:',' public:'));(p/'test.cpp').write_text(program)
 cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-DSYSTEM_DUMMY','-I'+tmp,'-I'+str(lib),'-I'+str(lib.parent/'GnBasicLib'),str(p/'test.cpp')]
 cmd += [str((lib.parent/'GnBasicLib' if n in ('GnBasics','GnTime','GnCoopTimer') else lib)/(n+'.cpp')) for n in ['GnBasics','GnTime','GnCoopTimer','GnDeviceIdentity']]
 subprocess.run(cmd+['-ljson-c','-lpthread','-o',str(p/'test')],check=True);subprocess.run([str(p/'test'),str(p/'Identity.json')],check=True)
print('Complete 6-kB UDP peer status received; complete 9-kB datagram received')
