// GnChargeRunOn.h, Version: 1.05
#pragma once
#include <GnBasics.h>
#include <GnTemporaryDiagnostics.h>
#include <cmath>
#include <cstdint>

// Volatile state of one output. Configuration and learned factors belong to its profile.
class TGnChargeRunOn
 {
 public:
#if GN_DEBUG_CHARGE_LEARNING
  struct TLearningStep
   {
    float BeforePercent = 0, AfterPercent = 0;
    uint32_t AppliedFactor = 0, NewFactor = 0;
    uint64_t ChargeMs = 0;
    bool SaveRequired = false;
   };
  bool TakeLearningStep(TLearningStep & Step)
   { if(!FStepReady)return false; Step=FStep; FStepReady=false; return true; }
#endif
  void Reset(void) { *this = TGnChargeRunOn(); }
  bool TakeLearned( uint32_t & Factor )
   {
    if( !FLearned ) return false;
    Factor = FLearnedFactor; FLearned = false; return true;
   }

  bool Update( TGNString const & Key, float Cutoff, uint32_t Factor, uint64_t Now,
               bool Valid, bool Missing, float Percent, uint32_t Age,
               bool OutputOn, bool AutomaticState, bool Allowed,
               bool LearningAllowed = true, uint32_t BootCounter = 0, bool BootCounterKnown = true )
   {
    if( !Allowed || Factor == 0 ||
        ( Valid && ( !std::isfinite( Percent ) || Percent < 0 || Percent > 100 ) ) )
     { Reset(); return false; }
    if( FKey != Key || FCutoff != Cutoff || FFactor != Factor )
     { Reset(); FKey = Key; FCutoff = Cutoff; FFactor = FWorkingFactor = Factor; }
    if( !LearningAllowed || !BootCounterKnown )
     { FCycleLearning = false; FLearned = false; }
    if( FTickKnown && OutputOn ) FOnMs += Now - FLastTick;
    FLastTick = Now; FTickKnown = true;
    if( Valid )
     {
      if( FRunning || FAwaiting )
       {
        const float Gain = Percent - FStartPercent;
        if( FCycleLearning && static_cast<uint32_t>( BootCounter - FStartBootCounter ) == 1 && Gain >= 1 && FOnMs > 0 )
         {
          const double Measured = static_cast<double>( FOnMs ) / 1000.0 / Gain;
          const double Learned = 0.75 * FWorkingFactor + 0.25 * Measured;
          if( std::isfinite( Learned ) && Learned > 0 && Learned <= UINT32_MAX )
           {
#if GN_DEBUG_CHARGE_LEARNING
            const uint32_t AppliedFactor = FWorkingFactor;
#endif
            FWorkingFactor = static_cast<uint32_t>( std::floor( Learned ) );
            if( FWorkingFactor == 0 ) FWorkingFactor = 1;
            const uint64_t Difference = FWorkingFactor > FFactor ? FWorkingFactor - FFactor : FFactor - FWorkingFactor;
            // Keep small changes in RAM and compare accumulated learning with the saved factor.
            if( Difference * 100 >= static_cast<uint64_t>( FFactor ) * 5 )
             { FLearnedFactor = FWorkingFactor; FLearned = true; }
#if GN_DEBUG_CHARGE_LEARNING
            FStep = {FStartPercent,Percent,AppliedFactor,FWorkingFactor,FOnMs,FLearned};
            FStepReady = true;
#endif
           }
         }
        FRunning = FAwaiting = FArmed = false;
       }
      // Do not move the baseline repeatedly while rereading the same cached packet.
      const uint64_t SampleAt = Age <= Now ? Now - Age : 0;
      if( !FArmed || SampleAt > FSampleAt + 2 )
       {
        FSampleAt = SampleAt; FStartPercent = Percent; FOnMs = 0;
        FArmed = OutputOn && AutomaticState && Percent < Cutoff;
        FCycleLearning = FArmed && LearningAllowed && BootCounterKnown;
        FStartBootCounter = BootCounter;
       }
      if( !OutputOn || !AutomaticState ) FArmed = false;
      return AutomaticState;
     }
    // An explicitly invalid report is not a missing sender and cannot start a run-on.
    if( !Missing ) { Reset(); return false; }
    if( FArmed )
     {
      FArmed = false;
      if( !OutputOn ) return false;
      FRunning = true; FRunStart = Now;
      // Time already charged while waiting for expiry belongs to the same budget.
      FDurationMs = static_cast<double>( Cutoff - FStartPercent ) * FWorkingFactor * 1000.0 - FOnMs;
      if( FDurationMs < 0 ) FDurationMs = 0;
     }
    if( FRunning )
     {
      if( !OutputOn || static_cast<double>( Now - FRunStart ) >= FDurationMs )
       { FRunning = false; FAwaiting = true; return false; }
      return true;
     }
    return false;
   }

 private:
#if GN_DEBUG_CHARGE_LEARNING
  TLearningStep FStep;
  bool FStepReady = false;
#endif
  TGNString FKey;
  float FCutoff = 0, FStartPercent = 0;
  uint32_t FStartBootCounter = 0;
  bool FCycleLearning = false;
  uint32_t FFactor = 0, FWorkingFactor = 0, FLearnedFactor = 0;
  uint64_t FLastTick = 0, FSampleAt = 0, FOnMs = 0, FRunStart = 0;
  double FDurationMs = 0;
  bool FTickKnown = false, FArmed = false, FRunning = false, FAwaiting = false, FLearned = false;
 };
