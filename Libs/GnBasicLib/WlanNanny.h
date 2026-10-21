// WlanNanny.h, Version: 1.02
#pragma once

#include <GnPlatform.h>

#if defined( GNPlatformESP32 )

//****************************************************************
// Includes
//****************************************************************

#include <GnBasics.h>

//****************************************************************
// TWlanNanny
//****************************************************************

constexpr uint32_t REJOIN_MIN_MS = 3000;   // 3 s
constexpr uint32_t REJOIN_MAX_MS = 60000;  // 60 s

class TWlanNanny : public TLoopInstance, public TSingleton<TWlanNanny>
 {
 public:
  void                  Init(void);          // Instatiation
  void                  Init( const TGNString & Hostname, const TGNString & WifiSsid,
                              const TGNString & WifiPassword );
 private:
  virtual void          Loop(void) override;

  TGNString             FHostname;
  TGNString             FWifiSsid;
  TGNString             FWifiPassword;
 };
#define WlanNanny       TWlanNanny::Instance()

#endif // ESP32
