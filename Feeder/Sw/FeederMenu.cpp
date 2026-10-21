// FeederMenu.cpp, Version: 2.31
//****************************************************************
// Includes
//****************************************************************

#include "FeederMenu.h"
#include "FeederConfig.h"
#include "FeederWifi.h"
#include "FeederServo.h"
#include "FeederTemperature.h"
#include <WifiKit8Oled.h>
#include <GnRotEncoder.h>

//****************************************************************
// TFeederStatusMenu
//****************************************************************

TFeederStatusMenu::TFeederStatusMenu(void)
: FBlinkTimer( Closure( this, &TFeederStatusMenu::BlinkTimerCb ), 250, false )
 {
  ModeChgExit();
 }

/*virtual*/ void TFeederStatusMenu::Perform_(void)
 {
  // Line 1: Wifi-Mode
  //    Mode: AP+STA   NTP:OK
  //    012345678901234567890
  Oled.XyPrintf( 0, LineY( 0 ), "Mode:          NTP:%s", FeederWifi.TimeIsValid() ? "OK" : "--" );
                                
  Oled.setDrawColor( FBlinkColor );
  Oled.XyPrintf( 36, LineY( 0 ), "%s%s%s", ( FMode & WIFI_AP ) ? "AP" : "", ( FMode == WIFI_AP_STA ) ? "+" : "", ( FMode & WIFI_STA ) ? "STA" : "" );
  Oled.setDrawColor( 1 );
  
  // Line 2: ST-IP
  //    StIp: 123.456.789.123  WIFI_STA && CONNECTED
  //    StIp: connecting       !CONNECTED
  //    StIp: N/A              !WIFI_STA
  //    012345678901234567890
  Oled.XyPrintf( 0, LineY( 1 ), "StIp: %s", ( FeederConfig.WifiMode & WIFI_STA ) ? ( WiFi.status() == WL_CONNECTED ) ? WiFi.localIP().toString().c_str() : "connecting" : "N/A" );

  // Line 3: AP-IP
  //    ApIp: 123.456.789.123
  //    012345678901234567890
  Oled.XyPrintf( 0, LineY( 2 ), "ApIp: %s", ( FeederConfig.WifiMode & WIFI_AP ) ? WiFi.softAPIP().toString().c_str() : "N/A" );
 }

void TFeederStatusMenu::ModeChgEnter(void)
 {
  FInModeChg = true;
  FMode = FeederConfig.WifiMode;
  FBlinkColor = 1;
  FBlinkTimer.Start();
 }

void TFeederStatusMenu::ModeChgExit(void)
 {
  FInModeChg = false;
  FMode = FeederConfig.WifiMode;
  FBlinkColor = 1;
  FBlinkTimer.Stop();
 }

void TFeederStatusMenu::BlinkTimerCb( TGnCoopTimerBase *, uint16_t )
 {
  FBlinkColor = 1 - FBlinkColor; // Toggle Blink: 0 -> 1, 1 -> 0
 }

/*virtual*/ void TFeederStatusMenu::Enter(void)
 {
  ModeChgExit();
 }

/*virtual*/ void TFeederStatusMenu::Exit(void)
 {
  ModeChgExit();
 }
  
/*virtual*/ bool TFeederStatusMenu::HandleKey( int8_t key )
 {
  if( !FInModeChg )
   {
    if( key != 'D' )
      return false;
    ModeChgEnter();
   }
   else
   {
    switch( key )
     {
      case 'R':
       if( ++FMode > WIFI_AP_STA )
         FMode = WIFI_STA; 
       break;
      case 'L':
       if( --FMode < WIFI_STA )
         FMode = WIFI_AP_STA; 
       break;
      case 'D':
       if( FeederConfig.WifiMode != FMode )
        {
         FeederConfig.WifiMode = TWifiMode( FMode );
         FeederConfigInstance.Save();
         ESP.restart();
        }
       ModeChgExit();
       break;
      default:
       break;
     }
   }
  return true;
 }

//****************************************************************
// TFeederMFeedMenu
//****************************************************************

/*virtual*/ void TFeederMFeedMenu::Perform_(void)
 {
  Oled.setFont( u8g2_font_profont29_mf );
  Oled.setDrawColor( FeederServo.Running() ? 0 : 1 );
  Oled.drawCentered( 32, " Feed " );
 }

/*virtual*/ bool TFeederMFeedMenu::HandleKey( int8_t key )
 {
  if( key != 'D' )
    return false;
  if( !FeederServo.Running() )
    FeederServo.Feed();
  return true;
 }

//****************************************************************
// TFeederTemperatureMenu
//****************************************************************

/*virtual*/ void TFeederTemperatureMenu::Perform_(void)
 {
  char text[ 16 ];
  if( FeederTemperature.Valid() )
   {
    const int temperature = static_cast<int>( FeederTemperature.Value() + 0.5F );
    snprintf( text, sizeof( text ), "%d %s", temperature, FeederTemperature.Unit() );
   }
  else
    snprintf( text, sizeof( text ), "-- %s", FeederTemperature.Unit() );

  Oled.setFont( u8g2_font_profont29_mf );
  const u8g2_uint_t textWidth = Oled.getUTF8Width( text );
  const int x = textWidth < 128 ? ( 128 - textWidth ) / 2 : 0;
  Oled.drawUTF8( static_cast<u8g2_uint_t>( x ), 32, text );
 }

//****************************************************************
// TFeederTimeMenu
//****************************************************************

// Font Names:
// u8g2_xxx_<Type>x   -> Type:
//  t Transparent gylphs with variable width
//  m Monospace/fixed width glyphs
//  h Glyphs with variable width and common height
//  8 Monospace/fixed width glyphs in a 8x8 box

// u8g2_xxx_x<Char Set>x   -> Char Set:
//  e Extended: Glyphs with unicode 32 to 701 are included in the font (v2.16.x will also include big ß)
//  f Full: Glyphs with unicode 32 to 255 are included in the font
//  r Restricted: Only characters from 32 to 127 are included
//  u Uppercase: Numbers and uppercase letters
//  n Numbers and some extra glyphs for date and time printing are included
//  _something  Special selection of glyphs. See the font picture for details

/*virtual*/ void TFeederTimeMenu::Perform_(void)
 {
  if( FeederWifi.TimeIsValid() )
   {
    time_t timeNow;
    struct tm tm;                     // the structure tm holds time information in a more convient way
  
    time( &timeNow );                 // read the current time
    localtime_r( &timeNow, &tm );
  
    // DrawDate( tm );

    // 5x8_mf         OK
    // 6x10_mf
    // profont10_mf
    // crox1cb_mf
    static char const * WdayStrs[ 7 ] = { "So", "Mo", "Di", "Mi", "Do", "Fr", "Sa" };
    // Assemble date string.
    String dateStr = String( WdayStrs[ tm.tm_wday ] ) + ", " + TwoDigitStr( tm.tm_mday ) + "." + TwoDigitStr( tm.tm_mon ) + "." + String( tm.tm_year + 1900 );
    if( tm.tm_isdst > 0 )
      dateStr += " DST";
    Oled.drawCentered( 7, dateStr.c_str() );

    // DrawTime( tm );

    // 10x20_mf                             OK, etwas zu klein
    // t0_22_mf t0_22b_mf
    // profont22_mf profont29_mf
    // crox1cb_mf crox1c_mf 1,2,3
    // inr21_mf inr24_mf inr27_mf inr30_mf
    Oled.setFont( u8g2_font_profont29_mf );
    Oled.drawCentered( 32, TimeString( tm.tm_hour, tm.tm_min, tm.tm_sec ).c_str() );

   }
   else
   {
    Oled.setFont( u8g2_font_profont29_mf );
    Oled.drawCentered( 32, "--:--:--" );
   }
 }

//****************************************************************
// TFeederMenu
//****************************************************************

TFeederMenu TFeederMenu::Instance;                        

TBaseMenu * TFeederMenu::FMenus[] =
 {
  new TFeederStatusMenu(),
  new TFeederTimeMenu(),
  new TFeederMFeedMenu(),
  new TFeederTemperatureMenu()
 };

TFeederMenu::TFeederMenu(void)
: FMenuIdx( 0 )
 {
  GnRotEncoder.Attach( FeederConfig.ClkPin, FeederConfig.DirPin, FeederConfig.BtnPin );
  GnRotEncoder.RotCb( Closure( this, &TFeederMenu::RotCb ) );
  GnRotEncoder.BtnCb( Closure( this, &TFeederMenu::BtnCb ) );
  Oled.clearDisplay();
  Oled.StartScreenSaver();
 }

void TFeederMenu::RotCb( int16_t & absCnt, int16_t relCnt )
 {
  if( relCnt != 0 )
    HandleKey( relCnt > 0 ? 'R' : 'L' );
 }

void TFeederMenu::BtnCb( bool up )
 {
  HandleKey( up ? 'U' : 'D' );
 }

void TFeederMenu::HandleKey( int8_t key )
 {
  if( Oled.InPowerSave() )
   {
    Oled.StartScreenSaver();
    return; 
   }
  
  if( !FMenus[ FMenuIdx ]->HandleKey( key ) )
   {
    switch( key )
     {
      case 'R':
       NextMenu();
       break;
      case 'L':
       PrevMenu();
       break;
      default:
       break;
     }
   }
 }

void TFeederMenu::Loop(void)
 {
  FMenus[ FMenuIdx ]->Perform();
 }

bool TFeederMenu::MenuAvailable( uint8_t menuIdx ) const
 {
  return menuIdx != TemperatureMenuIdx || FeederTemperature.Enabled();
 }

void  TFeederMenu::SelectMenu( uint8_t menuIdx )
 {
  if( menuIdx >= TableSize( FMenus ) || !MenuAvailable( menuIdx ) )
    return;
  FMenus[ FMenuIdx ]->Exit();
  FMenuIdx = menuIdx;
  FMenus[ FMenuIdx ]->Enter();
 }

void TFeederMenu::NextMenu(void)
 {
  uint8_t idx = FMenuIdx;
  do
   {
    if( ++idx >= TableSize( FMenus ) )
      idx = 0;
   }
  while( !MenuAvailable( idx ) && idx != FMenuIdx );
  SelectMenu( idx );
 }
 
void TFeederMenu::PrevMenu(void)
 {
  uint8_t idx = FMenuIdx;
  do
   {
    if( idx == 0 )
      idx = TableSize( FMenus ) - 1;
     else
      --idx;
   }
  while( !MenuAvailable( idx ) && idx != FMenuIdx );
  SelectMenu( idx );
 }
