// GnUdpStatus.h, Version: 1.21
#pragma once

#include <GnBasics.h>
#include <functional>
#include <memory>
#include <GnUdpInfoParts.h>
#include <vector>
#include <GnCoopTimer.h>

#include <TransportConfig.h>
inline const uint16_t GnUdpStatusPort = AquariumTransport::UdpPort;
// The supported protocol revision is defined with the message contract.
#include <GnUdpProtocol.h>
#include <GnUdpSchemaSupport.h>
inline bool GnUdpMeasurementFieldValid( TGNString const & Field )
 { return GnMeasurementName( Field ); }

constexpr TGnCoopTimer_t GnUdpStatusPublishIntervalMs = 10000;
constexpr TGnCoopTimer_t GnUdpStatusCheckIntervalMs = 250;
constexpr TGnCoopTimer_t GnUdpTemperatureMaxAgeMs = 20000;

struct TGnChargeLearningPolicy
 {
  bool Allowed = false;
  bool Available = false;
  uint32_t BootCounter = 0;
 };

struct TGnUdpTemperature
 {
  bool  Available = false;
  bool  Valid = false;
  float Celsius = 0.0F;
  uint32_t SampleRevision = 0;
 };

struct TGnUdpFloorStatus
 {
  uint64_t BootCounter = 0;
  uint64_t CurrentTime = 0;
  uint64_t RoomUpdatedAt = 0;
  double   RoomTemperature = 0;
  bool     FloorValid = false;
  bool     FloorOn = false;
  bool     FloorOverride = false;
  bool     RoomValid = false;
 };

class TGnWireValue;

class TGnUdpStatus : public TSingleton<TGnUdpStatus>, public TLoopInstance
 {
  friend class TSingleton<TGnUdpStatus>;

 public:
  // A null Parts argument requests a change signature only.
  using TCompactStatusProvider = std::function<bool(uint64_t &, TGnUdpInfoParts *, TGNString &)>;
  using TTemperatureProvider = std::function<TGnUdpTemperature(void)>;

 private:
                                      TGnUdpStatus(void);
                                      ~TGnUdpStatus(void);
  void                                Loop(void) override;

 public:
  bool                                SetupCompact( TGNString Id, TCompactStatusProvider StatusProvider,
                                                    TTemperatureProvider TemperatureProvider = {} );
  TGNString const &                   Error(void) const;
  void                                RequestAll(void);
  void                                Request( TGNString const & Id );
  void                                Request( std::vector<TGNString> const & Ids );
  void                                WatchFloorStatus( TGNString const & Device, TGNString const & Channel );
  bool                                GetWatchedFloorStatus( TGnUdpFloorStatus & Status ) const;
  bool                                GetHeatPiPeer( TGNString const & Id, TGNString & Status,
                                                     bool & AwaitingInitialStatus );
  bool                                GetTemperature( TGNString const & Id, float & Celsius ) const;
  bool                                GetMeasurement( TGNString const & Id, TGNString const & Field, float & Value ) const;
  TGnChargeLearningPolicy              ChargeLearningPolicy( TGNString const & Id ) const;
  TGnCoopTimer_t                      TemperatureAgeMs( TGNString const & Id ) const;
  TGnCoopTimer_t                      MeasurementAgeMs( TGNString const & Id, TGNString const & Field ) const;

 private:
  class TImplementation;
  std::unique_ptr<TImplementation>    FImplementation;
 };

#define GnUdpStatus TGnUdpStatus::Instance()
