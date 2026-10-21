// FeederMenu.h, Version: 1.01
//****************************************************************

#ifndef FeederMenuH  // sentry
#define FeederMenuH

//****************************************************************
// Includes
//****************************************************************

#include <GnBasics.h>
#include <GnCoopTimer.h>
#ifdef ESP32
  #include <WiFi.h>
  #include <WiFiType.h>
 #else
  #include <ESP8266WiFi.h>
#endif
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "Menu.h"

//****************************************************************
// TFeederStatusMenu
//****************************************************************

class TFeederStatusMenu : public TBaseMenu
 {
  typedef TBaseMenu Base;
  friend class TFeederMenu;
 protected:
                      TFeederStatusMenu(void);
                      ~TFeederStatusMenu(void) {}

 private:
  bool                FInModeChg;
  uint8_t             FMode;
  TGnCoopTimerMs      FBlinkTimer;
  uint8_t             FBlinkColor;  // 1: Blink Off, 0: Blink On

 private:
  void                ModeChgEnter(void);
  void                ModeChgExit(void);
  void                BlinkTimerCb( TGnCoopTimerBase * Tmr, uint16_t IntervalsElapsed );

 protected:
  virtual void        Perform_(void);

 public:
  virtual void        Enter(void);
  virtual void        Exit(void);
  
  virtual bool        HandleKey( int8_t key );
 };

//****************************************************************
// TFeederTimeMenu
//****************************************************************

class TFeederTimeMenu : public TBaseMenu
 {
  typedef TBaseMenu Base;
  friend class TFeederMenu;
 protected:
                      TFeederTimeMenu(void) {};
                      ~TFeederTimeMenu(void) {}

  virtual void        Perform_(void);

 public:
  //virtual void      Enter(void) {}
  //virtual void      Exit(void) {}
  
  //virtual bool      HandleKey( int8_t key );
 };

//****************************************************************
// TFeederMFeedMenu
//****************************************************************

class TFeederMFeedMenu : public TBaseMenu
 {
  typedef TBaseMenu Base;
  friend class TFeederMenu;
 protected:
                      TFeederMFeedMenu(void) {};
                      ~TFeederMFeedMenu(void) {}

  virtual void        Perform_(void);

 public:
  //virtual void      Enter(void) {}
  //virtual void      Exit(void) {}
  
  virtual bool        HandleKey( int8_t key );
 };

//****************************************************************
// TFeederTemperatureMenu
//****************************************************************

class TFeederTemperatureMenu : public TBaseMenu
 {
  typedef TBaseMenu Base;
  friend class TFeederMenu;
 protected:
                      TFeederTemperatureMenu(void) {};
                      ~TFeederTemperatureMenu(void) {}

  virtual void        Perform_(void);
 };

//****************************************************************
// TFeederMenu
//****************************************************************

class TFeederMenu : public TLoopInstance
 {
 private:
                      TFeederMenu(void);
                      ~TFeederMenu(void) {}
  static TFeederMenu  Instance;                        

 private:
  static constexpr uint8_t TemperatureMenuIdx = 3;
  static TBaseMenu *  FMenus[];
  uint8_t             FMenuIdx;

 private:
  bool                MenuAvailable( uint8_t menuIdx ) const;
  void                RotCb( int16_t & absCnt, int16_t relCnt );
  void                BtnCb( bool up );

  void                HandleKey( int8_t key );

 protected:

 public:
  void                Loop(void);

  void                SelectMenu( uint8_t menuIdx );
  void                NextMenu(void);
  void                PrevMenu(void);
 };


//****************************************************************
#endif // sentry
