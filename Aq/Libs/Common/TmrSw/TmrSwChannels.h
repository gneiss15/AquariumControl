// TmrSwChannels.h, Version: 1.04
#pragma once
#include <GnBasics.h>
#include <GnHeaterControl.h>

struct TTmrSwChannelConfig
 {
  TGNString    Id;
  THeaterRole  Role = THeaterRole::Timer;
  TGNString    Profile;
 };

class TTmrSwChannels : public TSingleton<TTmrSwChannels>
 {
  friend class TSingleton<TTmrSwChannels>;
 private:
                            TTmrSwChannels(void) = default;

  TTmrSwChannelConfig       FChannel;
  THeaterProfiles           FProfiles;

 private:
  static bool               Parse( String const & Json, TTmrSwChannelConfig & Channel,
                                   THeaterProfiles & Profiles, String & Error );

 public:
  static bool               Validate( String const & Json, TTmrSwChannelConfig & Channel, THeaterProfiles & Profiles, String & Error )
                              { return Parse( Json, Channel, Profiles, Error ); }
  bool                      Setup(void);
  String                    Set( String const & Json );
  const TTmrSwChannelConfig & Channel(void) const              { return FChannel; }
  const THeaterProfiles &   Profiles(void) const               { return FProfiles; }
 };
#define TmrSwChannels TTmrSwChannels::Instance()
