// FeederConfig.inc.h, Version: 1.10
// CD: Config Definition
// CD( htmlType, typ, name, defValue [, extraHtmlText] )
// CD_Div( htmlType ) just for HTML to make an optical separation between parameters


#ifndef CD
  #error CD must be defined before including FeederConfig.inc.h
#endif
#ifndef CD_Div
  #define CD_Div
#endif

CD( w, WiFiMode_t, WifiMode,           WIFI_STA )
CD_Div

CD( t, String,           WlanSsid,           "MyWLAN2" )
CD( T, String,           WlanPw,             "" )
CD( t, String,           WlanSsid2,          "MyWLAN", "<span name=\"AlternateSSID\"></span>" )
CD( T, String,           WlanPw2,            "" )
CD( t, String,           NtpServerAdr,       "192.168.99.1" )
CD( t, String,           TZ,                 "CET-1CEST,M3.5.0,M10.5.0/3", "<span name=\"see\"></span>: <a href=\"https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv\"><span name=\"TZ_Link\"></span></a>" )
CD_Div

CD( t, String,           ApSsid,             "FeederAq2" )
CD( T, String,           ApPw,               "CHANGE_ME" )
CD_Div

CD( T, String,           FtpPw,              "CHANGE_ME" )
CD_Div

CD( t, String,           Hostname,           "FeederAq2" )
CD_Div

CD( p, int,              ButtonPin,              0, "0-15" )       // D3
CD( p, int,              HomePin,                4, "0-15" )       // D2
CD( p, int,              MotorPin,              12, "0-15" )       // D6
CD( u, TGnCoopTimer_t,   ButtonDebounceMs,      30, "20-1000" )
CD( u, TGnCoopTimer_t,   ButtonLongPressMs,   5000, "1000-10000" )
CD( u, TGnCoopTimer_t,   ButtonResetPressMs, 10000, "2000-10000" )
CD( u, TGnCoopTimer_t,   HomeDebounceMs,        20, "10-1000" )
CD( u, TGnCoopTimer_t,   HomeLeaveTimeoutMs,  1000, "100-10000" )
CD( u, TGnCoopTimer_t,   RotationTimeoutMs,   7500, "500-10000" )
CD( u, TGnCoopTimer_t,   HomingTimeoutMs,     7500, "500-10000" )
CD_Div

CD( b, bool,             DallasPresent,        true )
CD( p, int,              DallasPin,             14, "0-15" )       // D5
CD( f, TTemperatureUnit, TemperatureUnit,    "C" )

#undef CD
#undef CD_Div
