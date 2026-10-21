// GnCoopTimer.h, Version: 1.04
//****************************************************************

#ifndef GnCoopTimerH  // sentry
#define GnCoopTimerH

//****************************************************************
// Description
//
// Classes to implement Coop(erative)Timers
//****************************************************************

//****************************************************************
// Usage:
//
// Instantiate an TGnCoopTimerUs/Ms os S with the needed interval
// (in µs, ms or s) and supply a callback, that should be called
// when the timer elapses.
//
// Callbacks:
// - void Cb( TGnCoopTimerBase * tmr, uint16_t intervalsElapsed )
//   Will be called whenever the Timer elapses.
//   tmr:              the timer that calls the callback
//   intervalsElapsed: normaly 1, but may be greater if Loop
//                     (see below) was not called in time or if
//                     timer was resumed when more than one
//                     interval has elapsed
//
// Methods:
// - TimerCb       change/query the aktual callback at any time
// - Interval change/query the aktual interval at any time
// - Enabled         query the state of the timer
// - Start           Timer will fire at now + interval
// - Stop            Stop is the same as Pause
// - Resume          Timer will fire (even when prev. stopped) at
//                   the same time as if it wasn't Stopped/Paused 
// - ReStart         Timer will fire after the time remaining when
//                   he was paused/stopped
//
// Remarks:
//
// All TGnCoopTimer... classes are derived from TLoopInstance,
// which implies, that inside "loop" there is a call to:
// LoopInstances.Loop();
// This enshures, that all instances of TGnCoopTimer... are
// regulary called.
//****************************************************************

//****************************************************************
// Includes
//****************************************************************

#include <GnBasics.h>
#include <Closures.h>

//****************************************************************
// Sw Configuration
//****************************************************************

//****************************************************************
// Public Types & Vars
//****************************************************************

typedef uint32_t TGnCoopTimer_t;
constexpr TGnCoopTimer_t TGnCoopTimer_MAX = UINT32_MAX / 2;

//****************************************************************
// TGnCoopTimerBase (Base class)
//****************************************************************

class TGnCoopTimerBase;
typedef TClosure<void( TGnCoopTimerBase * tmr, uint16_t intervalsElapsed )> TGnCoopTimerCb;

class TGnCoopTimerBase : TLoopInstance
 {
 protected:
  // implemented in derived classes to define the Unit for one Tick (us, ms, s...)
  virtual 
   TGnCoopTimer_t     GetAkt(void)                                                    = 0;
  virtual void        Loop(void) override;
  
 protected:
  TGnCoopTimerCb      FTimerCb;
  TGnCoopTimer_t      FInterval;
  TGnCoopTimer_t      FStop;      // GetAkt() at Stop called
  TGnCoopTimer_t      FNextEvent; // Fire Time
  bool                FEnabled;

 protected:
                      TGnCoopTimerBase(void)                                          {}
                      ~TGnCoopTimerBase(void)                                         {}
  void                Init( TGnCoopTimerCb timerCb,
                            TGnCoopTimer_t interval,
                            bool enabled );
 public:
  void                TimerCb( TGnCoopTimerCb timerCb )                               { FTimerCb = timerCb; }
  TGnCoopTimerCb      TimerCb(void)                                                   { return FTimerCb; }

                      // Timer will fire at last now + new interval
  void                Interval( TGnCoopTimer_t interval );
  TGnCoopTimer_t      Interval(void)                                                  { return FInterval; }
  
  bool                Enabled(void)                                                   { return FEnabled; }

                      // Timer will fire at now + interval
  void                Start(void);
                      // Stop is the same as Pause
  void                Stop(void);
                      // Timer will fire (even when prev. stopped) at the same time as if it wasn't Stopped/Paused 
  void                Resume(void)                                                    { FEnabled = true; }
                      // Timer will fire after the remaining time when he was paused/stopped
  void                ReStart(void);
 };

//****************************************************************
// TGnCoopTimerUs, TGnCoopTimerMs & TGnCoopTimerS
//****************************************************************

// Timer-Unit = us
class TGnCoopTimerUs : public TGnCoopTimerBase
 {
 protected:
  virtual 
   TGnCoopTimer_t     GetAkt(void)                                                    { return GnMicros(); }

 public:
                      TGnCoopTimerUs( TGnCoopTimerCb timerCb,
                                      TGnCoopTimer_t interval, 
                                      bool enabled = true )                           { Init( timerCb, interval, enabled ); }
                      ~TGnCoopTimerUs(void)                                           = default;
 };

// Timer-Unit = ms
class TGnCoopTimerMs : public TGnCoopTimerBase
 {
 protected:
  virtual 
   TGnCoopTimer_t     GetAkt(void)                                                    { return GnMillis(); }

 public:
                      TGnCoopTimerMs( TGnCoopTimerCb timerCb,
                                      TGnCoopTimer_t interval,
                                      bool enabled = true )                           { Init( timerCb, interval, enabled ); }
                      ~TGnCoopTimerMs(void)                                           = default;
 };

// Timer-Unit = s
class TGnCoopTimerS : public TGnCoopTimerBase
 {
 protected:
  virtual 
   TGnCoopTimer_t     GetAkt(void)                                                    { return GnSeconds(); }

 public:
                      TGnCoopTimerS( TGnCoopTimerCb timerCb,
                                     TGnCoopTimer_t interval,
                                     bool enabled = true )                            { Init( timerCb, interval, enabled ); }
                      ~TGnCoopTimerS(void)                                            = default;
 };

//****************************************************************
// TGnElapsedTimerBase
//****************************************************************

// Simple elapsed-time measurement without TLoopInstance registration.
class TGnElapsedTimerBase
 {
 protected:
  TGnCoopTimer_t      FStart;

 protected:
  virtual
   TGnCoopTimer_t     GetAkt(void) const                                              = 0;
                      TGnElapsedTimerBase(void)                                       : FStart( 0 ) {}
                      ~TGnElapsedTimerBase(void)                                      {}

 public:
                      // Store the current time as the new measurement start.
  void                Start(void)                                                     { FStart = GetAkt(); }
                      // Time elapsed since the last Start() (or construction).
  TGnCoopTimer_t      Elapsed(void) const                                             { return GetAkt() - FStart; };
 };

//****************************************************************
// TGnElapsedTimerUs, TGnElapsedTimerMs & TGnElapsedTimerS
//****************************************************************

// Timer-Unit = us
class TGnElapsedTimerUs : public TGnElapsedTimerBase
 {
 protected:
  virtual
   TGnCoopTimer_t     GetAkt(void) const                                              { return GnMicros(); }

 public:
                      TGnElapsedTimerUs(void)                                         { Start(); }
                      ~TGnElapsedTimerUs(void)                                        = default;
 };

// Timer-Unit = ms
class TGnElapsedTimerMs : public TGnElapsedTimerBase
 {
 protected:
  virtual
   TGnCoopTimer_t     GetAkt(void) const                                              { return GnMillis(); }

 public:
                      TGnElapsedTimerMs(void)                                         { Start(); }
                      ~TGnElapsedTimerMs(void)                                        = default;
 };

#if !defined( GNPlatformAVR )
class TGnElapsedTimerMs64
 {
 private:
  uint64_t            FStart;

 public:
                      TGnElapsedTimerMs64(void)                                       { Start(); }
                      ~TGnElapsedTimerMs64(void)                                      = default;
  void                Start(void)                                                     { FStart = GnMillis64(); }
  uint64_t            Elapsed(void) const                                             { return GnMillis64() - FStart; }
 };
#endif

// Timer-Unit = s
class TGnElapsedTimerS : public TGnElapsedTimerBase
 {
 protected:
  virtual
   TGnCoopTimer_t     GetAkt(void) const                                              { return GnSeconds(); }

 public:
                      TGnElapsedTimerS(void)                                          { Start(); }
                      ~TGnElapsedTimerS(void)                                         = default;
 };

//****************************************************************
#endif // sentry

