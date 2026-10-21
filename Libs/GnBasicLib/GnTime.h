// GnTime.h, Version: 1.02
#pragma once

//****************************************************************
// Includes
//****************************************************************

#include <stdint.h>
#include <time.h>

//****************************************************************
// Functions
//****************************************************************

constexpr uint16_t GnMinuteOfDay( unsigned hour, unsigned minute )
 {
  return static_cast<uint16_t>( hour * 60 + minute );
 }

constexpr uint16_t GnMinuteOfWeek( uint16_t minuteOfDay, unsigned wday )
 {
  return static_cast<uint16_t>( wday * 24 * 60 + minuteOfDay );
 }

extern uint16_t GnMinuteOfWeek( struct tm const & timeParts );
extern uint16_t GnLocalMinuteOfWeek( time_t timeValue );
extern time_t   GnNextLocalMinuteOfWeek( time_t currentTime, uint16_t minuteOfWeek );
extern time_t   GnTimeGm( struct tm & value );
