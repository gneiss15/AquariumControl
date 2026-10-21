// GnUdpStatus.h, Version: 1.06
#pragma once

#include <GnBasics.h>
#include <functional>
#include <memory>
#include <vector>
#include <GnCoopTimer.h>

constexpr uint16_t GnUdpStatusPort = 44444;
// The completed network migration uses version 2 exclusively.
constexpr unsigned GnUdpStatusVersion = 2;
inline bool GnUdpStatusVersionSupported( unsigned Version ) { return Version == GnUdpStatusVersion; }
inline bool GnUdpMeasurementFieldValid( TGNString const & Field )
 {
  if( Field.empty() || Field.length() > 64 ) return false;
  for( char C : Field )
    if( !( ( C >= 'A' && C <= 'Z' ) || ( C >= 'a' && C <= 'z' ) ||
           ( C >= '0' && C <= '9' ) || C == '_' ) ) return false;
  return Field != "Kind";
 }
constexpr TGnCoopTimer_t GnUdpStatusPublishIntervalMs = 5000;
constexpr TGnCoopTimer_t GnUdpStatusCheckIntervalMs = 250;
constexpr TGnCoopTimer_t GnUdpTemperatureMaxAgeMs = 20000;
constexpr float GnUdpTemperatureChangeThresholdC = 0.2F;

struct TGnUdpTemperature
 {
  bool  Available = false;
  bool  Valid = false;
  float Celsius = 0.0F;
 };

class TGnUdpStatus : public TSingleton<TGnUdpStatus>, public TLoopInstance
 {
  friend class TSingleton<TGnUdpStatus>;

 public:
  using TStatusProvider = std::function<TGNString( uint64_t & Signature )>;
  using TTemperatureProvider = std::function<TGnUdpTemperature(void)>;

 private:
                                      TGnUdpStatus(void);
                                      ~TGnUdpStatus(void);
  void                                Loop(void) override;

 public:
  bool                                Setup( TGNString Id, TStatusProvider StatusProvider,
                                             TTemperatureProvider TemperatureProvider = {} );
  TGNString const &                   Error(void) const;
  void                                RequestAll(void);
  void                                Request( TGNString const & Id );
  void                                Request( std::vector<TGNString> const & Ids );
  bool                                GetTemperature( TGNString const & Id, float & Celsius ) const;
  bool                                GetMeasurement( TGNString const & Id, TGNString const & Field, float & Value ) const;
  TGnCoopTimer_t                      TemperatureAgeMs( TGNString const & Id ) const;

 private:
  class TImplementation;
  std::unique_ptr<TImplementation>    FImplementation;
 };

#define GnUdpStatus TGnUdpStatus::Instance()
