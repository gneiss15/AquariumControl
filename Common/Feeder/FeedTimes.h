// FeedTimes.h, Version: 2.39
//****************************************************************

#ifndef FeedTimesH  // sentry
#define FeedTimesH

//****************************************************************
// Includes
//****************************************************************

#include <GnBasics.h>
#ifdef ESP32
  #include <WiFi.h>
  #include <WiFiType.h>
 #else
  #include <ESP8266WiFi.h>
#endif
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>

//****************************************************************
// typedefs
//****************************************************************

typedef struct
 {
  uint8_t             So : 1;
  uint8_t             Mo : 1;
  uint8_t             Di : 1;
  uint8_t             Mi : 1;
  uint8_t             Do : 1;
  uint8_t             Fr : 1;
  uint8_t             Sa : 1;
  uint8_t             Active : 1;
 }
 TWdayMask;

typedef union
 {
  uint8_t             mask;
  TWdayMask           bits;
 }
 TWdayUnion;

typedef struct
 {
  uint8_t             h;
  uint8_t             m;
  TWdayUnion          wday;
  uint8_t             portions;
 }
 TFeedRecord;

typedef std::vector<TFeedRecord> TFeedRecords;

//****************************************************************
// TFeedTimes
//****************************************************************

class TFeedTimes : public TSingleton<TFeedTimes>, public TLoopInstance
 {
  friend class TSingleton<TFeedTimes>;
 private:
                      TFeedTimes(void);

 private:
  TFeedRecords        FFeedRecords;
  mutable time_t      FNextFeedCacheMinute = 0;
  mutable time_t      FNextFeedTime = 0;
  mutable uint8_t     FNextFeedPortions = 0;
  mutable bool        FNextFeedValid = false;

 private:
  static bool         ParseTime( String const & Value, uint8_t & Hour, uint8_t & Minute );
  static char const * Parse( String const & JsonStr, TFeedRecords & Records );
  static bool         HasActiveTimeConflict( TFeedRecords const & Records );
  char const *        Save( String const & JsonStr );
  bool                Load( String const & JsonStr );
  uint8_t             CheckTime( struct tm const & tm ) const;

 public:
  void                Loop(void) override;
  bool                GetNextFeed( time_t & FeedTime, uint8_t & Portions ) const;
  String              Set( String const & JsonStr );
  bool                Setup(void);
 };
#define FeedTimes   TFeedTimes::Instance()

//****************************************************************
#endif // sentry
