// Types.h, Version: 2.40
#pragma once
#include <GnBasics.h>
#include <GnHeaterControl.h>
#include <cstdint>
#include <optional>
#include <vector>

using TChannelId = uint8_t;
constexpr TChannelId InvalidChannelId = 0xFF;

using TChannelRole = THeaterRole;

enum class TOutputDriver
 {
  Gpio,
  Mcp23017
 };

struct TOutputConfig
 {
  TOutputDriver Driver = TOutputDriver::Gpio;
  unsigned      Bus = 0;
  unsigned      Address = 0;
  unsigned      Pin = 0;
  bool          ActiveHigh = true;
 };

struct TChannelConfig
 {
  TGNString                    Id;
  TGNString                    PhysPos;
  TGNString                    Profile;
  std::optional<int>           InputPin;
  std::optional<int>           OutputPin;
  std::optional<TOutputConfig> Output;
  bool                         InputActiveHigh = false;
  bool                         OutputActiveHigh = true;
  bool                         InputPullup = true;
  TChannelRole                 Role = TChannelRole::Timer;

  bool                         HasOutput(void) const { return OutputPin || Output; }
 };
typedef std::vector<TChannelConfig> TChannelConfigs;

//****************************************************************
// Weekday and switch records
//****************************************************************

typedef struct
 {
  uint8_t So : 1;
  uint8_t Mo : 1;
  uint8_t Di : 1;
  uint8_t Mi : 1;
  uint8_t Do : 1;
  uint8_t Fr : 1;
  uint8_t Sa : 1;
  uint8_t Active : 1;
 }
 TWdayMask;

typedef union
 {
  uint8_t   mask;
  TWdayMask bits;
 }
 TWdayUnion;

typedef struct
 {
  uint16_t   MinuteOfDay : 15;
  uint16_t   On : 1;
  TWdayUnion wday;
  TChannelId Channel;
 }
 TSwitchRecord;

static_assert( sizeof( TWdayMask ) == 1 );
static_assert( sizeof( TWdayUnion ) == 1 );
static_assert( sizeof( TSwitchRecord ) == 4 );

using TSwitchTime = TSwitchRecord;
typedef std::vector<TSwitchTime> TSwitchTimes;

typedef struct
 {
  uint16_t   StartMinuteOfDay;
  uint16_t   PeriodMinutes;
  uint16_t   OnMinutes;
  TWdayUnion wday;
  TChannelId Channel;
 }
 TSwitchInterval;
typedef std::vector<TSwitchInterval> TSwitchIntervals;

static_assert( sizeof( TSwitchTime ) == 4 );
static_assert( sizeof( TSwitchInterval ) == 8 );
