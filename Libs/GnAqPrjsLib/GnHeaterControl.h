// GnHeaterControl.h, Version: 1.08
#pragma once
#include <GnBasics.h>
#include <vector>
#include <GnChargeRunOn.h>
#include <GnHeatPi.h>

enum class THeaterRole
 {
  Timer,
  HeaterMain,
  HeaterFloor
 };

struct THeaterProfile
 {
  TGNString Id;
  TGNString TempId;
  float     ChargeStartPercent = 0.0F;
  float     ChargeStopPercent = 0.0F;
  bool      NoTempOn = false;
  bool      ChargeRunOn = false;
  uint32_t  ChargeSecondsPerPercent = 15;
  TGNString Measurement = "Temperature";
  TGnHeatPiConfig Pi{};
 };
typedef std::vector<THeaterProfile> THeaterProfiles;

class TGnHeaterControl
 {
 public:
                              TGnHeaterControl(void) = default;
                              TGnHeaterControl( const THeaterProfiles & Profiles );
  void                        Profiles( const THeaterProfiles & Profiles ) { FProfiles = Profiles; }
  bool                        AutomaticState( THeaterRole Role, const TGNString & ProfileId,
                                              bool SchedulePresent, bool ScheduleState,
                                              bool & HysteresisStateKnown, bool & HysteresisState,
                                              TGnChargeRunOn * Charge = nullptr, bool OutputOn = false,
                                              bool Automatic = true, TGNString const & Channel = {} ) const;
  static const THeaterProfile * FindProfile( const THeaterProfiles & Profiles, const TGNString & Id );
  static bool                 ParseRole( const TGNString & Text, THeaterRole & Role );
  static const char *         RoleName( THeaterRole Role );

 private:
  THeaterProfiles             FProfiles;
 };
