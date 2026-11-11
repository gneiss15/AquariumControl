// FeedbackMonitor.h, Version: 1.02
#pragma once
#include <GnBasics.h>
#include <json-c/json.h>
#include <optional>
#include <vector>

// Compare independently observed boolean feedback with a movable switch function.
// The caller supplies monotonic milliseconds; no wall clock or browser is needed.
class TFeedbackMonitor
 {
 public:
  void Configure( json_object * Configuration );
  void Bind( json_object * Registry );
  void Receive( json_object * Packet, uint64_t Now );
  json_object * Status( uint64_t Now );
 private:
  struct TSignal
   {
    std::optional<bool> Value;
    bool Invalid = false;
    std::optional<uint64_t> Received;
   };
  struct TMonitor
   {
    TGNString Id, SwitchFunction, Source, Kind, Field, SwitchId, SwitchChannelId, SourceId;
    uint64_t GraceMs = 60000;
    TSignal Switch, Feedback;
    std::optional<uint64_t> MismatchSince;
    std::optional<bool> MismatchDirection;
   };
  std::vector<TMonitor> Monitors;
 };
