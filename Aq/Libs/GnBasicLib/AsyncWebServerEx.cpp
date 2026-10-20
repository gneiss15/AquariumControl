// AsyncWebServerEx.cpp, Version: 1.02

#include <GnPlatform.h>

#if defined( GNPlatformESP32 )
#include <AsyncWebServerEx.h>
#include "SpecialPrint.h"

//****************************************************************
// TWsKeepAlive
//****************************************************************

TWsKeepAlive::TWsKeepAlive( uint32_t pingIntervalMs, uint32_t pongTimeoutMs, uint8_t maxMisses )
: FPingIntervalMs( pingIntervalMs )
, FRealPongTimeoutMs( pongTimeoutMs * (uint32_t)maxMisses )
, FNextPingMs( 0 )
, FHavePinged( false )
 {}

void TWsKeepAlive::OnEvent( AsyncWebSocket *, AsyncWebSocketClient * c, AwsEventType t )
 {
  uint32_t id = c->id();
  switch( t )
   {
    case WS_EVT_CONNECT:
      FPongs.push_back( TPongs( id, millis() + FPingIntervalMs ) ); // <— NICHT sofort pingen
      FHavePinged = false;
      //LSPrintf( "[WS] keepalive tracked (id=%u)\n", id );
      break;
    case WS_EVT_DISCONNECT:
      for( size_t i = 0;  i < FPongs.size();  ++i )
        if( FPongs[ i ].Id == id )
         {
          FPongs.erase( FPongs.begin() + i );
          break;
         }
      //LSPrintf( "[WS] keepalive untrack (id=%u)\n", id );
      break;
    case WS_EVT_PONG:
      for( size_t i = 0;  i < FPongs.size();  ++i )
        if( FPongs[ i ].Id == id )
         {
          FPongs[ i ].PongMs = millis();
          break;
         }
      break;
    default:
      break;
   }
 }

void TWsKeepAlive::Poll( AsyncWebSocket * ws )
 {
  uint32_t now = millis();

  // zyklisch PING an alle Clients
  if( (int32_t)( now - FNextPingMs ) >= 0 )
   {
    FNextPingMs = now + FPingIntervalMs;
    FHavePinged = true; // <— ab jetzt dürfen wir überhaupt PONG-Timeouts werten
    for( TPongs p : FPongs )
      if( auto cl = ws->client( p.Id ) )
        cl->ping();
   }

  // Clients ohne PONG nicht brutal kicken – nur, wenn wir schon mind. 1x aktiv gepingt haben
  if( FHavePinged && FRealPongTimeoutMs > 0 )
   {
    // Clients zu lange ohne PONG -> schließen
    for( size_t i = 0;  i < FPongs.size(); )
     {
      TPongs pong = FPongs[ i ];
      if( now - pong.PongMs > FRealPongTimeoutMs )
       {
        // Statt sofort close(): nur sanft auslisten ODER ganz lassen
        // Falls du kicken willst, dann:
        // if( auto cl = ws->client( pong.Id ) )
        //   cl->close();
        FPongs.erase( FPongs.begin() + i );
       }
       else
        ++i;
     }
   }
 }

//****************************************************************
// TAsyncWebServerEx
//****************************************************************

void TAsyncWebServerEx::onEvent( TAwsEventHandlerEx handler )
 {
  // Adapter-Lambda mit *Basis*-Signatur:
  FSocket.onEvent
   (
    [handler]
     (
      AsyncWebSocket * s,
      AsyncWebSocketClient * c,
      AwsEventType t,
      void * arg,
      uint8_t * data,
      size_t len
     )
     {
      // Wir wissen: s ist FSocket -> sicher static_cast
      auto * sex = static_cast<TAsyncWebSocketEx*>( s );
      sex->OnEvent( s, c, t );
      #if 0
      if( t == WS_EVT_CONNECT )
        LSPrintf( "[WS] %s connect (id=%u, from=%s, clients=%u)\n", sex->Tag().c_str(), c->id(), c->remoteIP().toString().c_str(), sex->count() );
       else if( t == WS_EVT_DISCONNECT )
        LSPrintf( "[WS] %s disconnect (id=%u, clients=%u)\n", sex->Tag().c_str(), c->id(), sex->count() );
      #endif
      handler( sex, c, t, arg, data, len );
     }
   );
 }

TAsyncWebServerEx::~TAsyncWebServerEx(void)
 {
  for( size_t i = 0;  i < Registry.size();  ++i )
   {
    if( Registry[ i ] == this )
	 {
	  Registry[ i ] = nullptr;
	  break;
	 }
   }
 }

void TAsyncWebServerEx::Loop(void)
 {
  const uint32_t now = millis();
  if( now - FLastCleanupClients >= 30000 ) // alle 30 s
   {           
    FLastCleanupClients = now;
    FSocket.cleanupClients();
   }
  FSocket.FWsKeepAlive.Poll( &FSocket );
 }

// Registry of TAsyncWebServerEx instances
TAsyncWebServerEx::TRegistry      TAsyncWebServerEx::Registry;

void TAsyncWebServerEx::CloseAll(void)
 {
  for( auto * s : Registry )
    if( s )
	  s->CloseSelf();
 }
void TAsyncWebServerEx::CloseSelf(void)
 {
  FSocket.closeAll();
 }

#endif // ESP32
