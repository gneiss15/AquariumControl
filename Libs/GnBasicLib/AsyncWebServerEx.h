// AsyncWebServerEx.h, Version: 1.02
#pragma once

#include <GnPlatform.h>

#if defined( GNPlatformESP32 )

#include <GnBasics.h>
#include <ESPAsyncWebServer.h>
#include <vector>

//****************************************************************
// TWsKeepAlive
//****************************************************************
class TWsKeepAlive
 {
 public:
                        // Default: Ping alle 20s, 4s auf PONG warten, nach 2x Timeout schließen
                        TWsKeepAlive( uint32_t pingIntervalMs = 20000, uint32_t pongTimeoutMs = 4000, uint8_t maxMisses = 2 );
                        // Debug: Ping alle 20s, 60s auf PONG warten, nach 255x Timeout schließen
                        //TWsKeepAlive( uint32_t pingIntervalMs = 20000, uint32_t pongTimeoutMs = 60000, uint8_t maxMisses = 255 );
  void                  OnEvent( AsyncWebSocket * ws, AsyncWebSocketClient * c, AwsEventType t );
  void                  Poll( AsyncWebSocket * ws );

 private:
  uint32_t              FPingIntervalMs;
  uint32_t              FRealPongTimeoutMs;
  uint32_t              FNextPingMs;
  bool                  FHavePinged;

  typedef struct
   {
    uint32_t            Id;
    uint32_t            PongMs;
   }
   TPongs;
  std::vector<TPongs>   FPongs;
 };

//****************************************************************
// TAwsEventHandlerEx
//****************************************************************
class TAsyncWebSocketEx;
class TAsyncWebServerEx;
using TAwsEventHandlerEx = std::function<void( TAsyncWebSocketEx * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t * data, size_t len )>;

//****************************************************************
// TAsyncWebSocketEx
//****************************************************************
class TAsyncWebSocketEx : public AsyncWebSocket
 {
  friend class TAsyncWebServerEx;
 public:
                        TAsyncWebSocketEx
                         (
                          const String & tag,
                          const String & url
                         )                                                     : AsyncWebSocket( url ), FWsKeepAlive(), FTag( tag ) {}
  const String &        Tag(void)                                              { return FTag; }
  void                  OnEvent
                         (
                          AsyncWebSocket * s,
                          AsyncWebSocketClient * c,
                          AwsEventType t
                         )                                                     { FWsKeepAlive.OnEvent( s, c, t ); }
 private:
  TWsKeepAlive          FWsKeepAlive;
  String                FTag;
 };

//****************************************************************
// TAsyncWebServerEx
//****************************************************************
class TAsyncWebServerEx : public AsyncWebServer, public TLoopInstance
 {
  friend class TAsyncWebSocketEx;
  typedef std::vector<TAsyncWebServerEx*> TRegistry;
 public:

                        TAsyncWebServerEx
                         (
                          const String & tag,
                          uint16_t port,
                          const String & wsUrl = "/"
                         )                                                     : AsyncWebServer( port ), TLoopInstance(), FLastCleanupClients( 0 ), FSocket( tag, wsUrl )
                                                                                { addHandler( &FSocket ); Registry.push_back( this ); }
                        ~TAsyncWebServerEx(void);

  void                  onEvent( TAwsEventHandlerEx handler );
  
  // Anzahl verbundener WS-Clients dieses Servers
  inline uint32_t       ClientCount(void) const                                { return FSocket.count(); }
  inline const AsyncWebSocket &
                        Socket(void) const                                     { return FSocket; }
  inline AsyncWebSocket &
                        Socket(void)                                           { return FSocket; }
  static void           CloseAll(void);

 private:
  virtual void          Loop(void) override;
  void                  CloseSelf(void);
 private:
  uint32_t              FLastCleanupClients;
  TAsyncWebSocketEx     FSocket;
  static TRegistry      Registry;
 private:
  using AsyncWebServer::addHandler;
 };

#endif // ESP32
