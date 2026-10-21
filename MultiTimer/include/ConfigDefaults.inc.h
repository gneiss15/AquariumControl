// ConfigDefaults.inc.h, Version: 1.01
// CD( htmlType, Type, Name, DefaultValue [, ExtraHtmlText] )

#ifndef CD
  #error CD must be defined before including ConfigDefaults.inc.h
#endif
#ifndef CD_Div
  #define CD_Div
#endif

CD( w, int,       WifiMode,     WIFI_STA )
CD_Div

CD( t, TGNString, WlanSsid,     "MyWLAN" )
CD( T, TGNString, WlanPw,       "" )
CD( t, TGNString, NtpServerAdr, "192.168.99.1" )
CD( t, TGNString, TimeZone,     "CET-1CEST,M3.5.0,M10.5.0/3" )
CD_Div

CD( t, TGNString, ApSsid,       "MultiTimer" )
CD( T, TGNString, ApPw,         "" )
CD_Div

CD( t, TGNString, Hostname,     "MultiTimer" )

#undef CD
#undef CD_Div
