// FeederDriveBase.h, Version: 1.02
#pragma once

#include <GnBasics.h>
#include <vector>

class TFeederDrive : public TLoopInstance
 {
 protected:
  void                 Loop(void) override                               {}

 public:
  virtual              ~TFeederDrive(void)                               = default;
  virtual void         Setup(void)                                       = 0;
  virtual bool         Feed( uint8_t Portions = 1 )                      = 0;
  virtual bool         Stop(void)                                        = 0;
  virtual bool         Running(void) const                               = 0;
  virtual bool         PinUsed( int Pin ) const                          = 0;
  virtual bool         SetSettings( String const & Json )                { (void)Json; return false; }
  virtual std::vector<TGNString> Capabilities(void) const                      = 0;
  virtual bool         HasInput(void) const                              { return false; }
  virtual bool         InputState(void) const                            { return false; }
  virtual char const * Error(void) const                                 { return ""; }
  virtual void         TimeSynchronized(void)                            {}
  virtual void         SetNetworkStatus( bool Connected, bool TimeValid,
                                         uint8_t Tick )                  { (void)Connected; (void)TimeValid; (void)Tick; }
 };
