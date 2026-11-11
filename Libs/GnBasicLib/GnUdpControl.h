// GnUdpControl.h, Version: 1.00
#pragma once
#include <GnBasics.h>
#include <memory>
#if defined(GNPlatformESP)
#if defined(ESP32)
#include <WiFi.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <unistd.h>
#else
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#endif
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

// Service-specific control ports avoid SO_REUSEADDR ambiguity on hosts with
// several applications. Observers without a service identity use an ephemeral port.
inline uint16_t GnUdpDescriptionPort(TGNString const & Id)
 {
  return Id.size()==19 && Id[17]=='@' && Id[18]>='1' && Id[18]<='4' ? 44500+Id[18]-'0' : 0;
 }
class TGnUdpControl
 {
#if defined(GNPlatformESP) && !defined(ESP32)
  WiFiUDP Udp;
  bool Ready=false;
#else
  int Fd=-1;
#endif
 public:
  bool Setup(uint16_t Port=0)
   {
#if defined(GNPlatformESP) && !defined(ESP32)
    if(Ready)return true;
    Ready=Udp.begin(Port);return Ready;
#else
    if(Fd>=0)return true;
    Fd=socket(AF_INET,SOCK_DGRAM,0);if(Fd<0)return false;
    int Enabled=1;setsockopt(Fd,SOL_SOCKET,SO_BROADCAST,&Enabled,sizeof(Enabled));
    sockaddr_in A{};A.sin_family=AF_INET;A.sin_addr.s_addr=htonl(INADDR_ANY);A.sin_port=htons(Port);
    if(bind(Fd,reinterpret_cast<sockaddr*>(&A),sizeof(A))<0){close(Fd);Fd=-1;return false;}
    return true;
#endif
   }
  ~TGnUdpControl(void)
   {
#if defined(GNPlatformESP) && !defined(ESP32)
    Udp.stop();
#else
    if(Fd>=0)close(Fd);
#endif
   }
  bool Send(TGNString const & Text,TGNString const & Address,uint16_t Port=44444)
   {
    if(Text.empty()||Text.size()>900||!Setup())return false;
#if defined(GNPlatformESP)
    IPAddress Ip;
    if(Address.empty())
     {auto Local=WiFi.localIP(),Mask=WiFi.subnetMask();for(unsigned I=0;I<4;++I)Ip[I]=Local[I]|uint8_t(~Mask[I]);}
    else if(!Ip.fromString(Address.c_str()))return false;
#if defined(ESP32)
    sockaddr_in A{};A.sin_family=AF_INET;A.sin_port=htons(Port);A.sin_addr.s_addr=inet_addr(Ip.toString().c_str());
    return sendto(Fd,Text.data(),Text.size(),0,reinterpret_cast<sockaddr*>(&A),sizeof(A))==static_cast<ssize_t>(Text.size());
#else
    if(!Udp.beginPacket(Ip,Port))return false;
    return Udp.write(reinterpret_cast<uint8_t const *>(Text.data()),Text.size())==Text.size()&&Udp.endPacket();
#endif
#else
    auto To=[&](in_addr Ip){sockaddr_in A{};A.sin_family=AF_INET;A.sin_port=htons(Port);A.sin_addr=Ip;return sendto(Fd,Text.data(),Text.size(),0,reinterpret_cast<sockaddr*>(&A),sizeof(A))==static_cast<ssize_t>(Text.size());};
    if(!Address.empty()){in_addr Ip{};if(inet_pton(AF_INET,Address.c_str(),&Ip)!=1)return false;return To(Ip);}
    ifaddrs * Interfaces=nullptr;if(getifaddrs(&Interfaces)!=0)return false;bool Sent=false;
    for(auto * I=Interfaces;I;I=I->ifa_next)
      if(I->ifa_addr&&I->ifa_addr->sa_family==AF_INET&&(I->ifa_flags&IFF_UP)&&(I->ifa_flags&IFF_BROADCAST)&&I->ifa_broadaddr)
        Sent=To(reinterpret_cast<sockaddr_in*>(I->ifa_broadaddr)->sin_addr)||Sent;
    freeifaddrs(Interfaces);return Sent;
#endif
   }
  bool Read(TGNString & Text,TGNString & Address,uint16_t & Port)
   {
    Text.clear();
#if defined(GNPlatformESP) && !defined(ESP32)
    if(!Ready)return false;
    int N=Udp.parsePacket();if(N<=0)return false;
    Address=Udp.remoteIP().toString().c_str();Port=Udp.remotePort();
    if(N>900){while(Udp.available())Udp.read();return true;}
    char Buffer[901];int Count=Udp.read(reinterpret_cast<unsigned char*>(Buffer),N);
#else
    if(Fd<0)return false;
    char Buffer[902];sockaddr_in From{};socklen_t Size=sizeof(From);
    int Count=recvfrom(Fd,Buffer,901,MSG_DONTWAIT,reinterpret_cast<sockaddr*>(&From),&Size);
    if(Count<0)return false;
    char Ip[INET_ADDRSTRLEN];inet_ntop(AF_INET,&From.sin_addr,Ip,sizeof(Ip));Address=Ip;Port=ntohs(From.sin_port);
#endif
    if(Count>0&&Count<=900)Text.assign(Buffer,size_t(Count));
    return true;
   }
 };
