// RoomTemperature.h, Version: 1.00
#pragma once
#include <GnBasics.h>
#include <GnCoopTimer.h>
#include <GnUdpStatus.h>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <future>

class TRoomTemperature : public TLoopInstance
 {
 public:
  struct TSample
   {
    bool Valid = false;
    float Celsius = 0;
    time_t UpdatedAt = 0;
    TGNString SensorId, Error;
   };
  explicit TRoomTemperature( std::filesystem::path Root = "/sys/bus/w1/devices",
                            std::chrono::milliseconds Interval = std::chrono::seconds(10) );
  bool Setup( std::filesystem::path const & ConfigFile, TGNString & Error );
  bool Enabled(void) const { return FEnabled; }
  TSample Sample(void) const;
  TGnUdpTemperature UdpTemperature(void) const;
  static TSample Read( std::filesystem::path const & Root, TGNString const & SensorId );

 protected:
  void Loop(void) override;

 private:
  std::filesystem::path FRoot;
  std::chrono::milliseconds FInterval;
  std::chrono::steady_clock::time_point FNextRead{}, FCompletedAt{};
  std::future<TSample> FRead;
  TSample FSample;
  TGNString FSensorId;
  uint32_t FRevision = 0;
  bool FEnabled = false;
 };
