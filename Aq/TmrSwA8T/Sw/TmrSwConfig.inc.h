// TmrSwConfig.inc.h, Version: 3.01
// CD: Config Definition
// CD( htmlType, typ, name, defValue [, extraHtmlText] )
// CD_Div( htmlType ) just for HTML to make an optical diversition betwwen parameters

// htmlType: (see although Index.js, function CD)
// t: text, creates a input type=text
// T: text, creates a input type=password
// n: int,  creates a input type=number
// p: int,  creates a input type=number		Pin-Number	0..15
// P: int,  creates a input type=number		Percent-Number	0..100
// u: uint, creates a input type=number		0..10000
// b: bool, creates a checkbox
// w: enum, creates a select for WiFiMode_t
// d: pseudo type, creates a padding line	(CD_Div)

// Arduino ESP32 WiFiMode_t: WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3
// typedef enum WiFiMode TWifiMode;  
// WiFiMode_t: WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3
// see: https://siytek.com/esp8266-ap-and-station-mode/
//#define WlanApChannel 1   // Don't fix the Channel
//#define WlanApHidden  true
//#define WlanApMaxCon  1

#ifndef CD
  #error CD must be defined before including TmrSwConfig.inc.h
#endif
#ifndef CD_Div
  #define CD_Div
#endif

CD( w, WiFiMode_t, WifiMode,           WIFI_STA )
CD_Div

CD( t, String,     WlanSsid,           "MyWLAN2" )
CD( T, String,     WlanPw,             "" )
CD( t, String,     WlanSsid2,          "MyWLAN", "<span name=\"AlternateSSID\"></span>" )
CD( T, String,     WlanPw2,            "" )
CD( t, String,     NtpServerAdr,       "192.168.99.1" )
CD( t, String,     TZ,                 "CET-1CEST,M3.5.0,M10.5.0/3", "<span name=\"see\"></span>: <a href=\"https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv\"><span name=\"TZ_Link\"></span></a>" )
CD_Div

// Ap: Acess-Point
CD( t, String,     ApSsid,             "LightTimerAq3" )
CD( T, String,     ApPw,               "CHANGE_ME" )
CD_Div

// FTP: pw for FTP
CD( T, String,     FtpPw,              "CHANGE_ME" )
CD_Div

// Hostname: Used for Wlan, Ap & FTP
CD( t, String,     Hostname,           "LightTimerAq3" )

#undef CD
#undef CD_Div
