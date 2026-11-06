// GnHeatPi.h, Version: 1.07
#pragma once
#include <GnBasics.h>
#include <algorithm>
#include <cmath>
#include <deque>

// Transport-independent PI calculation; adapters own relay and peer requests.
struct TGnHeatPiConfig
 {
  bool Enabled = false, Active = false;
  double Setpoint = 0, FloorWatts = 0, MainMaxWatts = 0;
  double CapacityWhPerK = 0, LossWattsPerK = 0, Kp = 0;
  uint32_t TiSeconds = 0, WindowSeconds = 600, DelaySeconds = 0;
  TGNString FloorDevice, FloorChannel, MainDevice, MainChannel;
 };
struct TGnHeatPiInput
 {
  uint64_t Now = 0;
  bool TemperatureValid = false, PowerValid = false, FloorValid = false, RoomValid = false;
  bool MainOn = false, FloorOn = false, FloorOverride = false, Manual = false, ScheduleAllowed = true;
  double Temperature = 0, MainWatts = 0, RoomTemperature = 0;
  uint32_t TemperatureAgeMs = 0, PowerAgeMs = 0, RoomAgeMs = 0, FloorAgeMs = 0;
  uint64_t PeerBoot = 0;
 };
struct TGnHeatPiStatus
 {
  bool Available = false, Valid = false, Learning = false;
  bool FloorRequest = false, MainRequest = false, FloorAvailable = false;
  bool ActualMainOn = false, ActualFloorOn = false;
  double Temperature = 0, Error = 0, P = 0, I = 0, FeedForward = 0, Demand = 0;
  double FloorDemand = 0, MainDemand = 0, ActualWatts = 0;
  double Capacity = 0, Loss = 0, Kp = 0;
  uint32_t TiSeconds = 0, Samples = 0, LearnedWindows = 0;
  uint32_t WindowAgeSeconds = 0;
  char const * Reason = "disabled";
  char const * LearningReason = "disabled";
 };
class TGnHeatPi
 {
 public:
  void Configure( TGnHeatPiConfig const & Config )
   {
    C = Config; Reset(); S.Available = C.Enabled;
    S.Capacity = C.CapacityWhPerK; S.Loss = C.LossWattsPerK;
    S.Kp = C.Kp; S.TiSeconds = C.TiSeconds;
   }
  void Reset(void)
   { S = {}; History.clear(); Delivery.clear(); HavePrevious = false; HaveWindow = false; Last = 0; LastUpdateKnown = false; TemperatureCache = {}; PowerCache = {}; RoomCache = {}; FloorCache = {}; FloorBoot = 0; BudgetKnown = false; FloorRemaining = MainRemaining = 0; }
  void CancelLearning(void) { HaveWindow = false; HavePrevious = false; History.clear(); Delivery.clear(); S.Learning = false; S.LearningReason = "cancelled"; }
  TGnHeatPiStatus const & Status(void) const { return S; }
  void Update( TGnHeatPiInput const & Input )
   {
    if( !C.Enabled ) return;
    TGnHeatPiInput X = Input;
    if( LastUpdateKnown && X.Now >= Last && X.Now - Last < 10000 ) return;
    const double Seconds = LastUpdateKnown && X.Now >= Last ? double( X.Now - Last ) / 1000 : 0;
    Last = X.Now; LastUpdateKnown = true;
    bool Held = false;
    X.TemperatureValid = Measurement(TemperatureCache,X.Now,Input.TemperatureValid,Input.TemperatureAgeMs,X.Temperature,Held);
    const bool PowerGood = Input.PowerValid && std::isfinite(Input.MainWatts) && Input.MainWatts >= 0;
    X.PowerValid = Measurement(PowerCache,X.Now,PowerGood,Input.PowerAgeMs,X.MainWatts,Held);
    X.RoomValid = Measurement(RoomCache,X.Now,Input.RoomValid,Input.RoomAgeMs,X.RoomTemperature,Held);
    if( Input.FloorOverride || ((Input.FloorValid || Input.PeerBoot) && FloorBoot != Input.PeerBoot) )
      FloorCache = {};
    if( Input.FloorValid || Input.PeerBoot ) FloorBoot = Input.PeerBoot;
    X.PeerBoot = FloorBoot;
    double FloorState = Input.FloorOn ? 1 : 0;
    X.FloorValid = Measurement(FloorCache,X.Now,Input.FloorValid && !Input.FloorOverride,Input.FloorAgeMs,FloorState,Held);
    X.FloorOn = X.FloorValid && FloorState > 0;
    const auto CorrectedWatts = [&](uint64_t At)
     {
      const auto Correction = [&](TMeasurement const & Cache, double Watts)
       {
        if( Cache.HoldCorrection == 0 && Cache.Correction == 0 ) return 0.0;
        if( At <= Cache.CorrectionFrom || At >= X.Now ) return 0.0;
        return (Watts-Cache.AnchorBefore)*double(At-Cache.CorrectionFrom)/double(X.Now-Cache.CorrectionFrom);
       };
      return Correction(PowerCache,X.MainWatts) + C.FloorWatts * Correction(FloorCache,FloorState);
     };
    S.Available = true; S.Samples++; S.FloorAvailable = X.FloorValid;
    const bool Finite = std::isfinite(X.Temperature) && std::isfinite(X.MainWatts) && X.MainWatts >= 0;
    S.Valid = X.TemperatureValid && X.PowerValid && Finite;
    S.Learning = false; S.WindowAgeSeconds = 0;
    S.ActualMainOn = X.MainOn; S.ActualFloorOn = X.FloorOn;
    if( !S.Valid || X.Manual || !X.ScheduleAllowed )
     {
      S.Reason = !S.Valid ? "invalid-measurement" : X.Manual ? "manual-override" : "schedule-off";
      S.LearningReason = S.Reason;
      S.Demand = S.FloorDemand = S.MainDemand = 0;
      S.FloorRequest = S.MainRequest = false;
      HavePrevious = HaveWindow = BudgetKnown = false; History.clear(); Delivery.clear();
      return; // Freeze integral and discard learning across unknown/manual periods.
     }
    const double FloorCapacity = X.FloorValid ? C.FloorWatts : 0;
    const double Maximum = FloorCapacity + C.MainMaxWatts;
    S.Temperature = X.Temperature; S.Error = C.Setpoint - X.Temperature;
    S.ActualWatts = X.MainWatts + ( X.FloorValid && X.FloorOn ? C.FloorWatts : 0 );
    S.P = S.Kp * S.Error;
    S.FeedForward = X.RoomValid && std::isfinite(X.RoomTemperature) ? std::max(0.0,S.Loss * (C.Setpoint-X.RoomTemperature)) : 0;
    const bool Continuous = HavePrevious && Seconds > 0 && Seconds <= 15 && Previous.PeerBoot == X.PeerBoot;
    if( Continuous )
     {
      // Integrate temperature error only; measured power belongs to model learning, not positive feedback.
      const double Unsaturated = S.FeedForward + S.P + S.I;
      double Delta = S.Kp * S.Error * Seconds / S.TiSeconds;
      if( ( Unsaturated >= Maximum && Delta > 0 ) || ( Unsaturated <= 0 && Delta < 0 ) ) Delta = 0;
      S.I += Delta;
      S.I = std::max( -Maximum, std::min( Maximum, S.I ) );
     }
    S.Demand = std::max( 0.0, std::min( Maximum, S.FeedForward + S.P + S.I ) );
    S.FloorDemand = std::min( FloorCapacity, S.Demand );
    S.MainDemand = std::max( 0.0, S.Demand - S.FloorDemand );
    // Freeze each period's energy request; carry the signed remainder (watt-seconds).
    // A negative remainder compensates delivery beyond the sampled cutoff.
    if( !Continuous ) BudgetKnown = false;
    if( BudgetKnown )
     {
      uint64_t Cursor = Previous.Now;
      while( Cursor < X.Now )
       {
        const uint64_t End = std::min(X.Now,BudgetEnd);
        const double Interval = double(End-Cursor)/1000;
        // Only account for observed delivery; missing floor feedback contributes no energy.
        FloorRemaining -= Previous.FloorValid && Previous.FloorOn ? C.FloorWatts * Interval : 0;
        MainRemaining -= Previous.MainWatts * Interval;
        Cursor = End;
        if( Cursor == BudgetEnd ) StartBudget(Cursor,FloorCapacity,FloorRemaining+MainRemaining);
       }
     }
    else StartBudget(X.Now,FloorCapacity,0);
    // Correct provisional held delivery once both endpoints are known (at most 60 s).
    // The signed correction follows the remaining budget even across a PWM boundary.
    if( Continuous )
     { MainRemaining -= PowerCache.HoldCorrection; FloorRemaining -= C.FloorWatts * FloorCache.HoldCorrection; }
    if( FloorCapacity == 0 && FloorRemaining > 0 )
     { MainRemaining += FloorRemaining; FloorRemaining = 0; }
    const bool EnergyNeeded = FloorRemaining + MainRemaining > 0;
    S.FloorRequest = EnergyNeeded && FloorCapacity > 0 && FloorRemaining > 0;
    S.MainRequest = EnergyNeeded && C.MainMaxWatts > 0 && MainRemaining > 0;
    S.Reason = Held ? "measurement-held" : !Continuous ? "initial-or-gap" : !X.FloorValid ? "floor-offline" : !X.RoomValid ? "room-unavailable" : "observing";
    const bool Learn = Continuous && X.FloorValid && X.RoomValid && Previous.FloorValid && Previous.RoomValid &&
                       std::isfinite(X.RoomTemperature) && std::isfinite(Previous.RoomTemperature);
    double DelayedWatts = S.ActualWatts;
    uint64_t DelayedAt = X.Now;
    bool DelayReady = C.DelaySeconds == 0;
    S.LearningReason = !Learn ? S.Reason : "waiting-delay";
    if( !Learn ) { HaveWindow = false; History.clear(); Delivery.clear(); }
    else
     {
      if( C.DelaySeconds != 0 && (PowerCache.Correction != 0 || FloorCache.Correction != 0) )
        for( auto & Sample : History ) Sample.second += CorrectedWatts(Sample.first);
      History.push_back({X.Now,S.ActualWatts});
      const uint64_t Lag = uint64_t(C.DelaySeconds)*1000;
      while( History.size()>1 && X.Now-History[1].first>=Lag ) History.pop_front();
      DelayReady = X.Now-History.front().first>=Lag;
      DelayedWatts = History.front().second;
      DelayedAt = History.front().first;
     }
    if( Learn && DelayReady )
     {
      const bool StartingWindow = !HaveWindow;
      if( !HaveWindow )
       { HaveWindow = true; WindowStart = Previous.Now; StartTemperature = Previous.Temperature; Energy = DifferenceHours = 0; Delivery.clear();
         if( C.DelaySeconds != 0 ) { PreviousDelayedWatts = DelayedWatts; PreviousDelayedAt = DelayedAt; }
         Delivery.push_back({Previous.Now,PreviousDelayedAt,PreviousDelayedWatts}); }
      // A correction must belong entirely to this learning window. Never amend an
      // already accepted model window or import energy from before its start.
      const bool Correctable =
        ( TemperatureCache.Correction == 0 || TemperatureCache.CorrectionFrom >= WindowStart ) &&
        ( RoomCache.Correction == 0 || RoomCache.CorrectionFrom >= WindowStart );
      if( !Correctable )
       { WindowStart = X.Now; StartTemperature = X.Temperature; Energy = DifferenceHours = 0; Delivery.clear(); }
      // Correct only delivery already integrated in this window. The source time
      // also handles delays shorter than the missing interval without double counting.
      if( Correctable && !StartingWindow && (PowerCache.Correction != 0 || FloorCache.Correction != 0) && !Delivery.empty() )
       {
        double Before = 0, After = 0;
        for( size_t I=1; I<Delivery.size(); ++I )
          Before += (Delivery[I-1].Watts+Delivery[I].Watts)*0.5*double(Delivery[I].Now-Delivery[I-1].Now)/1000;
        for( auto & Sample : Delivery ) Sample.Watts += CorrectedWatts(Sample.SourceAt);
        for( size_t I=1; I<Delivery.size(); ++I )
          After += (Delivery[I-1].Watts+Delivery[I].Watts)*0.5*double(Delivery[I].Now-Delivery[I-1].Now)/1000;
        Energy += (After-Before)/3600;
        PreviousDelayedWatts = Delivery.back().Watts;
       }
      if( Correctable ) Energy += ( DelayedWatts + PreviousDelayedWatts ) * 0.5 * Seconds / 3600;
      Delivery.push_back({X.Now,DelayedAt,DelayedWatts});
      while( Delivery.size()>2 && X.Now-Delivery[1].Now>60000 ) Delivery.pop_front();
      if( Correctable ) DifferenceHours += ( X.Temperature - X.RoomTemperature + Previous.Temperature - Previous.RoomTemperature ) * 0.5 * Seconds / 3600;
      if( Correctable ) DifferenceHours += ( TemperatureCache.Correction - RoomCache.Correction ) / 3600;
      S.Learning = true; S.LearningReason = Held ? "collecting-held" : "collecting";
      S.WindowAgeSeconds = (X.Now-WindowStart)/1000;
      if( X.Now - WindowStart >= 3600000 && !TemperatureCache.Pending && !PowerCache.Pending && !RoomCache.Pending )
       {
        S.LearningReason = "insufficient-excitation";
        const double Change = X.Temperature - StartTemperature;
        // Only weak, bounded loss adaptation at near-equal water temperature.
        // Capacity learning requires appreciable temperature excursion; reject implausible estimates.
        if( std::abs(Change) <= 0.0626 && DifferenceHours > 0.5 )
         {
          S.LearningReason = "rejected-model-estimate";
          const double Estimate = ( Energy - S.Capacity * Change ) / DifferenceHours;
          if( std::isfinite(Estimate) && Estimate >= C.LossWattsPerK * 0.5 && Estimate <= C.LossWattsPerK * 2 )
           { S.Loss = 0.95 * S.Loss + 0.05 * Estimate; S.LearnedWindows++; S.LearningReason = "accepted-loss"; }
         }
        else if( std::abs(Change) >= 0.25 )
         {
          S.LearningReason = "rejected-model-estimate";
          const double Estimate = ( Energy - S.Loss * DifferenceHours ) / Change;
          if( std::isfinite(Estimate) && Estimate >= C.CapacityWhPerK * 0.5 && Estimate <= C.CapacityWhPerK * 2 )
           {
            S.Capacity = 0.95 * S.Capacity + 0.05 * Estimate;
            S.Kp = C.Kp * S.Capacity / C.CapacityWhPerK;
            S.LearnedWindows++; S.LearningReason = "accepted-capacity";
           }
         }
        HaveWindow = false;
       }
     }
    PreviousDelayedWatts = DelayedWatts; PreviousDelayedAt = DelayedAt; Previous = X; HavePrevious = true;
   }
 private:
  void StartBudget( uint64_t Now, double FloorCapacity, double Carry )
   {
    const double Total = S.Demand * C.WindowSeconds + Carry;
    FloorRemaining = std::min(FloorCapacity*C.WindowSeconds,std::max(0.0,Total));
    MainRemaining = Total-FloorRemaining;
    BudgetEnd = Now + uint64_t(C.WindowSeconds)*1000;
    BudgetKnown = true;
   }
  bool BudgetKnown = false;
  uint64_t BudgetEnd = 0;
  double FloorRemaining = 0, MainRemaining = 0;
  struct TMeasurement
   {
    double Value = 0, Used = 0, Anchor = 0, AnchorBefore = 0;
    double Area = 0, HoldArea = 0, Correction = 0, HoldCorrection = 0;
    uint64_t At = 0, Tick = 0, AnchorTick = 0, CorrectionFrom = 0;
    bool Known = false, Pending = false;
   };
  TMeasurement TemperatureCache, PowerCache, RoomCache, FloorCache;
  uint64_t FloorBoot = 0;
  static bool Measurement( TMeasurement & Cache, uint64_t Now, bool Good, uint32_t Age,
                           double & Value, bool & Held )
   {
    Cache.Correction = Cache.HoldCorrection = 0;
    Good = Good && std::isfinite(Value) && Age <= 60000 && uint64_t(Age) <= Now;
    const bool Fresh = Good && ( !Cache.Known || Now-Age > Cache.At );
    const bool Usable = Good || ( Cache.Known && Now >= Cache.At && Now-Cache.At <= 60000 );
    if( !Usable ) { Cache.Pending = true; return false; }
    if( !Fresh ) { Value = Cache.Value; Held = true; }
    if( !Cache.Known )
     {
      Cache.Known = true; Cache.At = Now-Age; Cache.Value = Value;
      Cache.Used = Cache.Anchor = Value; Cache.Tick = Cache.AnchorTick = Now;
      return true;
     }
    const double Seconds = Now >= Cache.Tick ? double(Now-Cache.Tick)/1000 : 0;
    Cache.Area += (Cache.Used+Value)*0.5*Seconds;
    Cache.HoldArea += Cache.Used*Seconds;
    if( Fresh )
     {
      const double Span = Now >= Cache.AnchorTick ? double(Now-Cache.AnchorTick)/1000 : 0;
      if( Span > 0 && Span <= 60 )
       {
        const double Target = (Cache.Anchor+Value)*0.5*Span;
        Cache.Correction = Target-Cache.Area;
        Cache.HoldCorrection = Cache.Pending ? Target-Cache.HoldArea : 0;
        Cache.CorrectionFrom = Cache.AnchorTick;
        Cache.AnchorBefore = Cache.Anchor;
       }
      Cache.At = Now-Age; Cache.Value = Cache.Anchor = Value;
      Cache.AnchorTick = Now; Cache.Area = Cache.HoldArea = 0; Cache.Pending = false;
     }
    else Cache.Pending = true;
    Cache.Used = Value; Cache.Tick = Now;
    return true;
   }
  std::deque<std::pair<uint64_t,double>> History;
  struct TDelivery { uint64_t Now, SourceAt; double Watts; };
  std::deque<TDelivery> Delivery;
  uint64_t PreviousDelayedAt = 0;
  double PreviousDelayedWatts = 0;
  TGnHeatPiConfig C;
  TGnHeatPiStatus S;
  TGnHeatPiInput Previous;
  bool HavePrevious = false, HaveWindow = false, LastUpdateKnown = false;
  uint64_t Last = 0, WindowStart = 0;
  double StartTemperature = 0, Energy = 0, DifferenceHours = 0;
 };
