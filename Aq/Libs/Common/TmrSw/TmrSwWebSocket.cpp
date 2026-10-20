// TmrSwWebSocket.cpp, Version: 2.45
#include <GnBasics.h>
#include <GnCoopTimer.h>
#include "TmrSwWebSocket.h"

#include "TmrSwStatusApi.h"
#include "WebSocketProtocol.h"
#if defined( ESP32 )
  #include <WiFi.h>
 #else
  #include <ESP8266WiFi.h>
#endif
#include <cstdio>
#include <ctime>
#include <memory>
#include <utility>
#include <vector>

namespace
 {
constexpr size_t   MaxClients = 2;
constexpr size_t   MaxHandshakeSize = 8 * 1024;
constexpr TGnCoopTimer_t HandshakeTimeoutMilliseconds = 1000;
constexpr TGnCoopTimer_t StatusCheckMilliseconds = 250;
constexpr TGnCoopTimer_t PingMilliseconds = 20000;

 } // namespace

class TTmrSwWebSocket::TImplementation
 {
 public:
                                          TImplementation(void);
                                          ~TImplementation(void);
  bool                                    Setup( unsigned Port, TGNString & Error );
  void                                    Loop(void);

 private:
  struct TWebSocketClient
   {
    WiFiClient Client;
    TGNString  ReceiveBuffer;
   };

  static bool                             ReadHandshake( WiFiClient & Client, TGNString & Request );
  static TGNString                        BuildEvent( unsigned long long Sequence, TGNString const & Status );
  static bool                             SendFrame( WiFiClient & Client, uint8_t OpCode, TGNString const & Payload );
  void                                    AcceptClient(void);
  bool                                    HandleClientData( TWebSocketClient & Client );
  void                                    Broadcast( uint8_t OpCode, TGNString const & Payload );
  void                                    CheckStatus(void);
  void                                    StatusTimerCb( TGnCoopTimerBase * Tmr, uint16_t IntervalsElapsed );
  void                                    PingTimerCb( TGnCoopTimerBase * Tmr, uint16_t IntervalsElapsed );

 private:
  std::unique_ptr<WiFiServer>             FServer;
  std::vector<TWebSocketClient>           FClients;
  uint64_t                                FLastSignature = 0;
  unsigned long long                      FSequence = 0;
  TGnCoopTimerMs                          FStatusTimer;
  TGnCoopTimerMs                          FPingTimer;
  bool                                    FStatusDue = false;
  bool                                    FPingDue = false;
 };

TTmrSwWebSocket::TImplementation::TImplementation(void)
: FStatusTimer( Closure( this, &TImplementation::StatusTimerCb ), StatusCheckMilliseconds, false )
, FPingTimer( Closure( this, &TImplementation::PingTimerCb ), PingMilliseconds, false )
 {
 }

TTmrSwWebSocket::TImplementation::~TImplementation(void)
 {
  for ( TWebSocketClient & Client : FClients )
    Client.Client.stop();
  FClients.clear();
  if ( FServer )
    FServer->stop();
 }

bool TTmrSwWebSocket::TImplementation::Setup( unsigned Port, TGNString & Error )
 {
  if ( FServer )
   {
    Error = F( "WebSocket server already started" );
    return false;
   }

  TmrSwStatusApi.StatusJson( FLastSignature );
  FServer = std::make_unique<WiFiServer>( static_cast<uint16_t>( Port ) );
  FServer->setNoDelay( true );
  FServer->begin();
  FStatusDue = false;
  FPingDue = false;
  FStatusTimer.Start();
  FPingTimer.Start();
  Error.clear();
  return true;
 }

bool TTmrSwWebSocket::TImplementation::ReadHandshake( WiFiClient & Client, TGNString & Request )
 {
  TGnElapsedTimerMs HandshakeTimer;
  Request.clear();
  while ( Client.connected() && HandshakeTimer.Elapsed() < HandshakeTimeoutMilliseconds )
   {
    while ( Client.available() > 0 )
     {
      const int Value = Client.read();
      if ( Value < 0 )
        break;
      Request.push_back( static_cast<char>( Value ) );
      if ( Request.length() > MaxHandshakeSize )
        return false;
      if ( Request.ends_with( "\r\n\r\n" ) )
        return true;
     }
    delay( 1 );
   }
  return false;
 }

TGNString TTmrSwWebSocket::TImplementation::BuildEvent( unsigned long long Sequence, TGNString const & Status )
 {
  char Header[128];
#if defined( GNPlatformESP )
  snprintf_P( Header, sizeof( Header ),
              PSTR( "{\"Type\":\"Status\",\"Sequence\":%llu,\"Timestamp\":%lld,\"Status\":" ),
              Sequence, static_cast<long long>( std::time( nullptr ) ) );
#else
  std::snprintf( Header, sizeof( Header ),
                 "{\"Type\":\"Status\",\"Sequence\":%llu,\"Timestamp\":%lld,\"Status\":",
                 Sequence, static_cast<long long>( std::time( nullptr ) ) );
#endif
  TGNString Event;
  const auto Build = [&]( auto & Output )
   {
    Output += Header;
    Output.append( Status );
    Output += '}';
   };
  if( !GnBuildStringExact( Event, Build ) )
    return TGNString();
  return Event;
 }

bool TTmrSwWebSocket::TImplementation::SendFrame( WiFiClient & Client, uint8_t OpCode,
                                                    TGNString const & Payload )
 {
  const TGNString Frame = WebSocketProtocol::BuildFrame( OpCode, Payload );
  return Client.connected() &&
         Client.write( reinterpret_cast<uint8_t const *>( Frame.data() ), Frame.length() ) == Frame.length();
 }

void TTmrSwWebSocket::TImplementation::AcceptClient(void)
 {
  WiFiClient Client = FServer->accept();
  if ( !Client )
    return;
  Client.setNoDelay( true );

  TGNString Request;
  TGNString Path;
  TGNString ClientKey;
  if ( FClients.size() >= MaxClients || !ReadHandshake( Client, Request ) ||
       !WebSocketProtocol::ParseHandshake( Request, Path, ClientKey ) || Path != "/api/v1/events" )
   {
    Client.print( F( "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n" ) );
    Client.stop();
    return;
   }

  const TGNString Accept = WebSocketProtocol::AcceptKey( ClientKey );
  TGNString Response;
  const auto BuildResponse = [&]( auto & Output )
   {
    Output += F( "HTTP/1.1 101 Switching Protocols\r\n"
                 "Upgrade: websocket\r\n"
                 "Connection: Upgrade\r\n"
                 "Sec-WebSocket-Accept: " );
    Output.append( Accept );
    Output += F( "\r\n\r\n" );
   };
  if( !GnBuildStringExact( Response, BuildResponse ) )
   {
    Client.stop();
    return;
   }
  if ( Client.write( reinterpret_cast<uint8_t const *>( Response.data() ), Response.length() ) != Response.length() )
   {
    Client.stop();
    return;
   }

  FClients.push_back( { Client, {} } );
  const TGNString Status = TmrSwStatusApi.StatusJson();
  if ( !SendFrame( FClients.back().Client, 0x1, BuildEvent( FSequence, Status ) ) )
   {
    FClients.back().Client.stop();
    FClients.pop_back();
   }
 }

bool TTmrSwWebSocket::TImplementation::HandleClientData( TWebSocketClient & Client )
 {
  char Buffer[256];
  while ( Client.Client.available() > 0 )
   {
    const int Count = Client.Client.read( reinterpret_cast<uint8_t *>( Buffer ), sizeof( Buffer ) );
    if ( Count <= 0 )
      break;
    Client.ReceiveBuffer.append( Buffer, static_cast<size_t>( Count ) );
   }
  if ( !Client.Client.connected() )
    return false;

  while ( true )
   {
    WebSocketProtocol::TFrame Frame;
    bool ProtocolError = false;
    if ( !WebSocketProtocol::ExtractFrame( Client.ReceiveBuffer, Frame, ProtocolError ) )
      return !ProtocolError;
    if ( Frame.OpCode == 0x8 )
     {
      SendFrame( Client.Client, 0x8, Frame.Payload );
      return false;
     }
    if ( Frame.OpCode == 0x9 && !SendFrame( Client.Client, 0xA, Frame.Payload ) )
      return false;
   }
 }

void TTmrSwWebSocket::TImplementation::Broadcast( uint8_t OpCode, TGNString const & Payload )
 {
  for ( size_t ClientNr = 0; ClientNr < FClients.size(); )
   {
    if ( SendFrame( FClients[ClientNr].Client, OpCode, Payload ) )
     {
      ++ClientNr;
      continue;
     }
    FClients[ClientNr].Client.stop();
    FClients.erase( FClients.begin() + ClientNr );
   }
 }

void TTmrSwWebSocket::TImplementation::CheckStatus(void)
 {
  uint64_t signature = 0;
  const TGNString status = TmrSwStatusApi.StatusJson( signature );
  if( signature == FLastSignature )
    return;

  FLastSignature = signature;
  ++FSequence;
  Broadcast( 0x1, BuildEvent( FSequence, status ) );
 }

void TTmrSwWebSocket::TImplementation::StatusTimerCb( TGnCoopTimerBase *, uint16_t )
 {
  FStatusDue = true;
 }

void TTmrSwWebSocket::TImplementation::PingTimerCb( TGnCoopTimerBase *, uint16_t )
 {
  FPingDue = true;
 }

void TTmrSwWebSocket::TImplementation::Loop(void)
 {
  if ( !FServer )
    return;

  AcceptClient();
  for ( size_t ClientNr = 0; ClientNr < FClients.size(); )
   {
    if ( HandleClientData( FClients[ClientNr] ) )
     {
      ++ClientNr;
      continue;
     }
    FClients[ClientNr].Client.stop();
    FClients.erase( FClients.begin() + ClientNr );
   }

  if( FStatusDue && !FClients.empty() )
   {
    FStatusDue = false;
    CheckStatus();
   }
  if( FPingDue )
   {
    FPingDue = false;
    Broadcast( 0x9, {} );
   }
 }

TTmrSwWebSocket::TTmrSwWebSocket(void)
: FImplementation( std::make_unique<TImplementation>() )
 {
 }

TTmrSwWebSocket::~TTmrSwWebSocket(void) = default;

bool TTmrSwWebSocket::Setup( unsigned Port, TGNString & Error )
 {
  return FImplementation->Setup( Port, Error );
 }

void TTmrSwWebSocket::Loop(void)
 {
  FImplementation->Loop();
 }
