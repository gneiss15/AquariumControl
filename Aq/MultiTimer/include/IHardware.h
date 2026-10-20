// IHardware.h, Version: 2.29
#pragma once
#include <GnBasics.h>
#include "Types.h"
#include <memory>

class TIHardware : public TLoopInstance, public TSingleton<TIHardware>
 {
  friend class TSingleton<TIHardware>;

 private:
                                   TIHardware(void);
                                   ~TIHardware(void);
  class TImplementation;
  std::unique_ptr<TImplementation> FImplementation;

 public:
  bool                             Setup( const TChannelConfigs & channels, TGNString & error );
  bool                             SetPin( unsigned channelNr, bool on, TGNString & error, bool firstTime = false );
  bool                             GetPin( unsigned channelNr, bool & active, TGNString & error );
  bool                             GetOutputState( unsigned channelNr, bool & on, TGNString & error );
  bool                             RequestShutdown( TGNString & error );
  bool                             CancelShutdown( TGNString & error );
  void                             Loop(void) override;
  void                             EnableRuntimeOutput(void);
 };

#define IHardware TIHardware::Instance()
