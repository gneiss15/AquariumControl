// IWebServer.h, Version: 1.06
#pragma once
#include <GnBasics.h>
#include "WebApplication.h"
#include <memory>

class TIWebServer : public TLoopInstance, public TSingleton<TIWebServer>
 {
  friend class TSingleton<TIWebServer>;

 private:
                                   TIWebServer(void);
                                   ~TIWebServer(void);
  class TImplementation;
  std::unique_ptr<TImplementation> Implementation;

 public:
  bool                             Setup( unsigned Port, TWebApplication & Application, TGNString & Error );
  void                             Loop(void) override;
 };

#define IWebServer TIWebServer::Instance()
