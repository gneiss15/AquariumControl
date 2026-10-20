// OtaHandler.h, Version: 1.00
#pragma once

#include <GnBasics.h>

class TGnOtaHandler : public TLoopInstance, public TSingleton<TGnOtaHandler>
 {
 public:
                        TGnOtaHandler( uint32_t normalWdtMs = 3000, uint32_t otaWdtMs = 10000 );
  void                  Begin( const char * hostName = nullptr, const char * password = nullptr );

 private:
  void                  SetWdtTimeout_( uint32_t timeoutMs );
  void                  SetWdtTimeout( bool active = false );

 private:
  virtual void          Loop(void) override;

 private:
  bool                  FOtaActive;
  uint32_t              FNormalWdtMs;
  uint32_t              FOtaWdtMs;
 };
#define GnOtaHandler TGnOtaHandler::Instance()
