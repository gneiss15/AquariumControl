// GnHeaterControl.h, Version: 1.02
#pragma once
#include <GnBasics.h>
#include <vector>

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
  float     FloorOn = 0.0F;
  float     FloorOff = 0.0F;
  float     MainOn = 0.0F;
  float     MainOff = 0.0F;
  bool      NoTempOn = false;
  TGNString Measurement = "Temperature";
 };
typedef std::vector<THeaterProfile> THeaterProfiles;

class TGnHeaterControl
 {
 public:
                              TGnHeaterControl(void) = default;
                              TGnHeaterControl( const THeaterProfiles & Profiles );
  void                        Profiles( const THeaterProfiles & Profiles ) { FProfiles = &Profiles; }
  bool                        AutomaticState( THeaterRole Role, const TGNString & ProfileId,
                                              bool SchedulePresent, bool ScheduleState,
                                              bool & HysteresisStateKnown, bool & HysteresisState ) const;
  static const THeaterProfile * FindProfile( const THeaterProfiles & Profiles, const TGNString & Id );
  static bool                 ParseRole( const TGNString & Text, THeaterRole & Role );
  static const char *         RoleName( THeaterRole Role );

 private:
  const THeaterProfiles *     FProfiles = nullptr;
 };
