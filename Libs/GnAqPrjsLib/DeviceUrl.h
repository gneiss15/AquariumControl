// DeviceUrl.h, Version: 1.00
#pragma once
#include <GnBasics.h>
#if defined(GNPlatformESP)
  #if defined(ESP32)
    #include <WiFi.h>
  #else
    #include <ESP8266WiFi.h>
  #endif
#else
  #include <ifaddrs.h>
  #include <arpa/inet.h>
  #include <net/if.h>
#endif

inline TGNString DeviceHost(void)
 {
#if defined(GNPlatformESP)
  return WiFi.localIP().toString().c_str();
#else
  ifaddrs * Interfaces=nullptr;
  if(getifaddrs(&Interfaces)!=0)return "127.0.0.1";
  TGNString Host;
  for(auto I=Interfaces;I;I=I->ifa_next)
   {
    if(!I->ifa_addr || I->ifa_addr->sa_family!=AF_INET ||
       !(I->ifa_flags&IFF_UP) || (I->ifa_flags&IFF_LOOPBACK))continue;
    char Text[INET_ADDRSTRLEN];
    if(!inet_ntop(AF_INET,&reinterpret_cast<sockaddr_in *>(I->ifa_addr)->sin_addr,Text,sizeof(Text)))continue;
    if(Host.empty())Host=Text;
    if(TGNString(I->ifa_name).find("wlan")==0) {Host=Text;break;}
   }
  freeifaddrs(Interfaces);
  return Host.empty()?"127.0.0.1":Host;
#endif
 }
inline TGNString DeviceUrl(char const * Scheme,unsigned Port,char const * Path)
 {return TGNString(Scheme)+"://"+DeviceHost()+":"+std::to_string(Port)+Path;}
