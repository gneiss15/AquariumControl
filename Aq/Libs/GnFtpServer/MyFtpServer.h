// MyFtpServer.h, Version: 1.05
#pragma once

#include <GnBasics.h>

#include <SimpleFTPServer.h>

#if defined( ESP32 )
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

class TMyFtpServer : public TSingleton<TMyFtpServer>, public TLoopInstance
 {
  friend class TSingleton<TMyFtpServer>;

 private:
                            TMyFtpServer(void);

 public:
  void                      Loop(void) override;
  void                      SetConfig( String const & User, String const & Password, uint8_t WifiMode );
  void                      FileSystemMounted( bool Mounted );

 private:
  FtpServer                 FFtpServer;
  String                    FUser;
  String                    FPassword;
  uint8_t                   FWifiMode;
  bool                      FConfigured;
  bool                      FFileSystemMounted;
  bool                      FStarted;
  IPAddress                 FLastIp;
 };

#define MyFtpServer TMyFtpServer::Instance()
