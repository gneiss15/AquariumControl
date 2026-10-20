// WebSocketServer.h, Version: 1.01
#pragma once
#include <GnBasics.h>
#include "StatusApi.h"
#include <memory>

class TWebSocketServer : public TLoopInstance, public TSingleton<TWebSocketServer>
 {
  friend class TSingleton<TWebSocketServer>;

 private:
                                   TWebSocketServer(void);
                                   ~TWebSocketServer(void);
  class TImplementation;
  std::unique_ptr<TImplementation> Implementation;

 public:
  bool                             Setup( unsigned Port, TStatusApi & StatusApi, TGNString & Error );
  void                             Loop(void) override;
 };

#define WebSocketServer TWebSocketServer::Instance()
