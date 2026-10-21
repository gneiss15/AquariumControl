// TmrSwWebSocket.h, Version: 1.00
#pragma once

#include <GnBasics.h>
#include <memory>

class TTmrSwWebSocket : public TSingleton<TTmrSwWebSocket>, public TLoopInstance
 {
  friend class TSingleton<TTmrSwWebSocket>;

 private:
                                    TTmrSwWebSocket(void);
                                    ~TTmrSwWebSocket(void);
  class TImplementation;
  std::unique_ptr<TImplementation>  FImplementation;

 public:
  bool                              Setup( unsigned Port, TGNString & Error );
  void                              Loop(void) override;
 };

#define TmrSwWebSocket TTmrSwWebSocket::Instance()
