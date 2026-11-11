// FeederWebSocket.h, Version: 2.39
#pragma once

#include <GnBasics.h>
#include <memory>

class TFeederWebSocket : public TSingleton<TFeederWebSocket>, public TLoopInstance
 {
  friend class TSingleton<TFeederWebSocket>;

 private:
                                   TFeederWebSocket(void);
                                   ~TFeederWebSocket(void);
  class TImplementation;
  std::unique_ptr<TImplementation> FImplementation;

 public:
  bool                             Setup( unsigned Port, TGNString & Error );
  void                             Loop(void) override;
 };

#define FeederWebSocket TFeederWebSocket::Instance()
