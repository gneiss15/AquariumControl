// GnTime.cpp, Version: 1.02
//****************************************************************
// Includes
//****************************************************************

#include <GnBasics.h>
#include <GnTime.h>

#include <stdint.h>

//****************************************************************
// Internal functions
//****************************************************************

static bool IsLeapYear( int year )
 {
  return year % 4 == 0 && ( year % 100 != 0 || year % 400 == 0 );
 }

static int64_t DaysBeforeYear( int year )
 {
  int64_t days = 0;
  if( year >= 1970 )
    for( int currentYear = 1970; currentYear < year; ++currentYear )
      days += IsLeapYear( currentYear ) ? 366 : 365;
  else
    for( int currentYear = year; currentYear < 1970; ++currentYear )
      days -= IsLeapYear( currentYear ) ? 366 : 365;
  return days;
 }

//****************************************************************
// Functions
//****************************************************************

uint16_t GnMinuteOfWeek( struct tm const & timeParts )
 {
  return GnMinuteOfWeek( GnMinuteOfDay( timeParts.tm_hour, timeParts.tm_min ),
                         static_cast<unsigned>( timeParts.tm_wday ) );
 }

uint16_t GnLocalMinuteOfWeek( time_t timeValue )
 {
  struct tm localTime{};
  localtime_r( &timeValue, &localTime );
  return GnMinuteOfWeek( localTime );
 }

time_t GnNextLocalMinuteOfWeek( time_t currentTime, uint16_t minuteOfWeek )
 {
  constexpr uint16_t minutesPerDay = 24 * 60;
  constexpr uint16_t minutesPerWeek = 7 * minutesPerDay;

  if( minuteOfWeek >= minutesPerWeek )
    return static_cast<time_t>( -1 );

  struct tm localTime{};
  localtime_r( &currentTime, &localTime );
  const uint16_t currentMinuteOfWeek = GnMinuteOfWeek( localTime );
  const unsigned eventDay = minuteOfWeek / minutesPerDay;
  int dayOffset = static_cast<int>(
    ( eventDay + 7 - static_cast<unsigned>( localTime.tm_wday ) ) % 7 );
  if( dayOffset == 0 && minuteOfWeek <= currentMinuteOfWeek )
    dayOffset = 7;

  localTime.tm_mday += dayOffset;
  localTime.tm_hour = ( minuteOfWeek % minutesPerDay ) / 60;
  localTime.tm_min = minuteOfWeek % 60;
  localTime.tm_sec = 0;
  localTime.tm_isdst = -1;
  return mktime( &localTime );
 }

time_t GnTimeGm( struct tm & value )
 {
  static uint16_t const daysBeforeMonth[] =
   { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

  const int year = value.tm_year + 1900;
  int dayOfYear = daysBeforeMonth[ value.tm_mon ] + value.tm_mday - 1;
  if( value.tm_mon > 1 && IsLeapYear( year ) )
    ++dayOfYear;

  const int64_t days = DaysBeforeYear( year ) + dayOfYear;
  int weekDay = static_cast<int>( ( days + 4 ) % 7 );
  if( weekDay < 0 )
    weekDay += 7;

  value.tm_yday = dayOfYear;
  value.tm_wday = weekDay;
  value.tm_isdst = 0;

  return static_cast<time_t>( days * 24 * 60 * 60
                            + value.tm_hour * 60 * 60
                            + value.tm_min * 60
                            + value.tm_sec );
 }
