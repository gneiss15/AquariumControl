// FeederConfig.h, Version: 2.43
//****************************************************************

#ifndef FeederConfigH  // sentry
#define FeederConfigH

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

//****************************************************************
// typedefs
//****************************************************************

// from ESP8266WiFiType.h: WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3
typedef enum WiFiMode TWifiMode;

class TTemperatureUnit
 {
 private:
  enum class TValue : uint8_t
   {
    Invalid,
    Celsius,
    Fahrenheit
   };

 public:
               TTemperatureUnit(void) = default;
               TTemperatureUnit( char const * text )
                {
                 if( text != nullptr && text[ 0 ] != '\0' && text[ 1 ] == '\0' )
                  {
                   if( text[ 0 ] == 'C' || text[ 0 ] == 'c' )
                     FValue = TValue::Celsius;
                   else if( text[ 0 ] == 'F' || text[ 0 ] == 'f' )
                     FValue = TValue::Fahrenheit;
                  }
                }
  bool         Valid(void) const                                      { return FValue != TValue::Invalid; }
  bool         Fahrenheit(void) const                                 { return FValue == TValue::Fahrenheit; }
  char const * Text(void) const
                {
                 return FValue == TValue::Celsius ? "C" :
                        FValue == TValue::Fahrenheit ? "F" : "";
                }

 private:
  TValue       FValue = TValue::Invalid;
 };

static_assert( sizeof( TTemperatureUnit ) == 1 );

typedef struct
 {
  #define CD( htmlType, typ, name, defValue, ... ) typ name;
  #include "FeederConfig.inc.h"
 }
 TConfig;

//****************************************************************
// TFeederConfig
//****************************************************************

class TFeederConfig : public TSingleton<TFeederConfig>
 {
  friend class TSingleton<TFeederConfig>;
 private:
              TFeederConfig(void);
 public:
  TConfig     FConfig;

 private:
  void        SetWifiMode(void);
  bool        Load( String const jsonStr );

  template<class TJsonValue, class T>
  static bool LoadValue( TJsonValue const & jsonValue, T & value )
               {
                if( !jsonValue.template is<T>() )
                  return false;
                value = jsonValue.template as<T>();
                return true;
               }
  template<class TJsonValue>
  static bool LoadValue( TJsonValue const & jsonValue, TTemperatureUnit & value )
               {
                if( !jsonValue.template is<char const *>() )
                  return false;
                TTemperatureUnit newValue( jsonValue.template as<char const *>() );
                if( !newValue.Valid() )
                  return false;
                value = newValue;
                return true;
               }

  template<class TJsonValue, class T>
  static void SaveValue( TJsonValue jsonValue, T const & value )
               {
                jsonValue.set( value );
               }
  template<class TJsonValue>
  static void SaveValue( TJsonValue jsonValue, TTemperatureUnit value )
               {
                jsonValue.set( value.Text() );
               }

 public:
  bool        Setup(void);
  bool        Save(void);
  bool        Set( String const jsonStr );
 };
#define FeederConfig   TFeederConfig::Instance().FConfig
#define FeederConfigInstance   TFeederConfig::Instance()

//****************************************************************
#endif // sentry
