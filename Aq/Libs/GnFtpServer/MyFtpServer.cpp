// MyFtpServer.cpp, Version: 1.02
#include <GnBasics.h>
#include "MyFtpServer.h"

namespace
 {
IPAddress const InvalidIp( 0, 0, 0, 0 );
 }

TMyFtpServer::TMyFtpServer(void)
: FWifiMode( WIFI_OFF )
, FConfigured( false )
, FFileSystemMounted( false )
, FStarted( false )
, FLastIp( InvalidIp )
 {
 }

void TMyFtpServer::SetConfig( String const & User, String const & Password, uint8_t WifiMode )
 {
  FUser = User;
  FPassword = Password;
  FWifiMode = WifiMode;
  FConfigured = true;
 }

void TMyFtpServer::FileSystemMounted( bool Mounted )
 {
  FFileSystemMounted = Mounted;
 }

void TMyFtpServer::Loop(void)
 {
  if( !FConfigured || !FFileSystemMounted )
    return;

  IPAddress CurrentIp;
  if( WiFi.status() == WL_CONNECTED )
    CurrentIp = WiFi.localIP();
   else if( FWifiMode & WIFI_AP )
    CurrentIp = WiFi.softAPIP();
   else
    return;

  if( CurrentIp == InvalidIp )
    return;

  if( !FStarted )
   {
    FFtpServer.begin( FUser.c_str(), FPassword.c_str() );
    FStarted = true;
   }

  if( FLastIp != CurrentIp )
   {
    FFtpServer.setLocalIp( CurrentIp );
    FLastIp = CurrentIp;
   }

  FFtpServer.handleFTP();
 }
