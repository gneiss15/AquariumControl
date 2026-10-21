// WebSocketServerNative.cpp, Version: 2.38
#include <GnBasics.h>
#include "BuildConfig.h"
#if defined( SYSTEM_DUMMY ) || defined( SYSTEM_RASPI )
  #include "WebSocketProtocol.h"
  #include "WebSocketServer.h"
  #include <arpa/inet.h>
  #include <atomic>
  #include <cerrno>
  #include <chrono>
  #include <netinet/in.h>
  #include <sys/select.h>
  #include <sys/socket.h>
  #include <thread>
  #include <unistd.h>
  #include <vector>

namespace
 {
constexpr size_t   MaxClients = 8;
constexpr size_t   MaxHandshakeSize = 16 * 1024;
constexpr unsigned StatusCheckMilliseconds = 250;
constexpr unsigned PingSeconds = 20;

struct TWebSocketClient
 {
  int       Socket = -1;
  TGNString ReceiveBuffer;
 };
 } // namespace

class TWebSocketServer::TImplementation
 {
 public:
                                                    TImplementation(void) = default;
                                                    ~TImplementation(void);
  bool                                              Setup( unsigned Port, TStatusApi & StatusApi, TGNString & Error );
  void                                              Loop(void);

 private:
  static bool                                       SendAll( int Socket, char const * Data, size_t Size );
  static bool                                       ReadHandshake( int Socket, TGNString & Request );
  static TGNString                                  BuildEvent( unsigned long long Sequence, TGNString const & Status );
  static bool                                       SendFrame( int Socket, uint8_t OpCode, TGNString const & Payload );
  bool                                              AcceptClient(void);
  bool                                              HandleClientData( TWebSocketClient & Client );
  void                                              Broadcast( uint8_t OpCode, TGNString const & Payload );
  void                                              CheckStatus(void);
  void                                              Run(void);

 private:
  TStatusApi *                                      StatusApi = nullptr;
  std::thread                                       Thread;
  std::atomic<bool>                                 Stop = false;
  int                                               ListenSocket = -1;
  std::vector<TWebSocketClient>                     Clients;
  uint64_t                                          LastSignature = 0;
  unsigned long long                                Sequence = 0;
  std::chrono::steady_clock::time_point             NextStatusCheck;
  std::chrono::steady_clock::time_point             NextPing;
 };

TWebSocketServer::TImplementation::~TImplementation(void)
 {
  Stop = true;
  if ( ListenSocket >= 0 )
   {
    shutdown( ListenSocket, SHUT_RDWR );
    close( ListenSocket );
   }
  if ( Thread.joinable() )
   {
    Thread.join();
   }
  ListenSocket = -1;
 }

bool TWebSocketServer::TImplementation::Setup( unsigned Port, TStatusApi & StatusApi, TGNString & Error )
 {
  if ( ListenSocket >= 0 )
   {
    Error = "WebSocket server already started";
    return false;
   }

  ListenSocket = socket( AF_INET, SOCK_STREAM, 0 );
  if ( ListenSocket < 0 )
   {
    Error = TGNString( "Cannot create WebSocket-server socket: " ) + strerror( errno );
    return false;
   }

  int ReuseAddress = 1;
  if ( setsockopt( ListenSocket, SOL_SOCKET, SO_REUSEADDR, &ReuseAddress, sizeof( ReuseAddress ) ) < 0 )
   {
    Error = TGNString( "Cannot configure WebSocket-server socket: " ) + strerror( errno );
    close( ListenSocket );
    ListenSocket = -1;
    return false;
   }

  sockaddr_in Address;
  memset( &Address, 0, sizeof( Address ) );
  Address.sin_family = AF_INET;
  Address.sin_addr.s_addr = htonl( INADDR_ANY );
  Address.sin_port = htons( static_cast<uint16_t>( Port ) );
  if ( bind( ListenSocket, reinterpret_cast<sockaddr *>( &Address ), sizeof( Address ) ) < 0 )
   {
    Error = TGNString( "Cannot bind WebSocket-server socket: " ) + strerror( errno );
    close( ListenSocket );
    ListenSocket = -1;
    return false;
   }

  if ( listen( ListenSocket, MaxClients ) < 0 )
   {
    Error = TGNString( "Cannot listen on WebSocket-server socket: " ) + strerror( errno );
    close( ListenSocket );
    ListenSocket = -1;
    return false;
   }

  this->StatusApi = &StatusApi;
  StatusApi.StatusJson( LastSignature );
  Stop = false;
  Thread = std::thread( [this] { Run(); } );
  Error.clear();
  return true;
 }

void TWebSocketServer::TImplementation::Loop(void)
 {
 }

bool TWebSocketServer::TImplementation::SendAll( int Socket, char const * Data, size_t Size )
 {
  while ( Size > 0 )
   {
    const ssize_t Sent = send( Socket, Data, Size, MSG_NOSIGNAL );
    if ( Sent < 0 )
     {
      if ( errno == EINTR )
        continue;
      return false;
     }
    if ( Sent == 0 )
      return false;
    Data += Sent;
    Size -= static_cast<size_t>( Sent );
   }
  return true;
 }

bool TWebSocketServer::TImplementation::ReadHandshake( int Socket, TGNString & Request )
 {
  char Buffer[1024];
  Request.clear();
  while ( Request.find( "\r\n\r\n" ) == TGNString::npos )
   {
    const ssize_t Count = recv( Socket, Buffer, sizeof( Buffer ), 0 );
    if ( Count < 0 )
     {
      if ( errno == EINTR )
        continue;
      return false;
     }
    if ( Count == 0 )
      return false;
    Request.append( Buffer, static_cast<size_t>( Count ) );
    if ( Request.length() > MaxHandshakeSize )
      return false;
   }
  return true;
 }

TGNString TWebSocketServer::TImplementation::BuildEvent( unsigned long long Sequence, TGNString const & Status )
 {
  TGNString Event = StringFormat( "{\"Type\":\"Status\",\"Sequence\":%llu,\"Timestamp\":%lld,\"Status\":",
                                  Sequence, static_cast<long long>( std::time( nullptr ) ) );
  Event += Status;
  Event += '}';
  return Event;
 }

bool TWebSocketServer::TImplementation::SendFrame( int Socket, uint8_t OpCode, TGNString const & Payload )
 {
  const TGNString Frame = WebSocketProtocol::BuildFrame( OpCode, Payload );
  return SendAll( Socket, Frame.data(), Frame.length() );
 }

bool TWebSocketServer::TImplementation::AcceptClient(void)
 {
  int ClientSocket = accept( ListenSocket, nullptr, nullptr );
  if ( ClientSocket < 0 )
    return false;

  timeval ClientTimeout;
  ClientTimeout.tv_sec = 1;
  ClientTimeout.tv_usec = 0;
  setsockopt( ClientSocket, SOL_SOCKET, SO_RCVTIMEO, &ClientTimeout, sizeof( ClientTimeout ) );
  setsockopt( ClientSocket, SOL_SOCKET, SO_SNDTIMEO, &ClientTimeout, sizeof( ClientTimeout ) );

  TGNString Request;
  TGNString Path;
  TGNString ClientKey;
  if ( Clients.size() >= MaxClients || !ReadHandshake( ClientSocket, Request ) ||
       !WebSocketProtocol::ParseHandshake( Request, Path, ClientKey ) || Path != "/api/v1/events" )
   {
    static char const Response[] = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
    SendAll( ClientSocket, Response, sizeof( Response ) - 1 );
    shutdown( ClientSocket, SHUT_RDWR );
    close( ClientSocket );
    return false;
   }

  const TGNString Accept = WebSocketProtocol::AcceptKey( ClientKey );
  const TGNString Response = "HTTP/1.1 101 Switching Protocols\r\n"
                             "Upgrade: websocket\r\n"
                             "Connection: Upgrade\r\n"
                             "Sec-WebSocket-Accept: " + Accept + "\r\n\r\n";
  if ( !SendAll( ClientSocket, Response.data(), Response.length() ) )
   {
    shutdown( ClientSocket, SHUT_RDWR );
    close( ClientSocket );
    return false;
   }

  Clients.push_back( { ClientSocket, {} } );
  const TGNString Status = StatusApi->StatusJson();
  if ( !SendFrame( ClientSocket, 0x1, BuildEvent( Sequence, Status ) ) )
   {
    shutdown( ClientSocket, SHUT_RDWR );
    close( ClientSocket );
    Clients.pop_back();
    return false;
   }
  return true;
 }

bool TWebSocketServer::TImplementation::HandleClientData( TWebSocketClient & Client )
 {
  char Buffer[2048];
  const ssize_t Count = recv( Client.Socket, Buffer, sizeof( Buffer ), 0 );
  if ( Count <= 0 )
    return false;
  Client.ReceiveBuffer.append( Buffer, static_cast<size_t>( Count ) );

  while ( true )
   {
    WebSocketProtocol::TFrame Frame;
    bool ProtocolError = false;
    if ( !WebSocketProtocol::ExtractFrame( Client.ReceiveBuffer, Frame, ProtocolError ) )
      return !ProtocolError;
    if ( Frame.OpCode == 0x8 )
     {
      SendFrame( Client.Socket, 0x8, Frame.Payload );
      return false;
     }
    if ( Frame.OpCode == 0x9 && !SendFrame( Client.Socket, 0xA, Frame.Payload ) )
      return false;
   }
 }

void TWebSocketServer::TImplementation::Broadcast( uint8_t OpCode, TGNString const & Payload )
 {
  for ( size_t ClientNr = 0; ClientNr < Clients.size(); )
   {
    if ( SendFrame( Clients[ClientNr].Socket, OpCode, Payload ) )
     {
      ++ClientNr;
      continue;
     }
    shutdown( Clients[ClientNr].Socket, SHUT_RDWR );
    close( Clients[ClientNr].Socket );
    Clients.erase( Clients.begin() + ClientNr );
   }
 }

void TWebSocketServer::TImplementation::CheckStatus(void)
 {
  uint64_t signature = 0;
  const TGNString status = StatusApi->StatusJson( signature );
  if( signature == LastSignature )
    return;

  LastSignature = signature;
  ++Sequence;
  Broadcast( 0x1, BuildEvent( Sequence, status ) );
 }

void TWebSocketServer::TImplementation::Run(void)
 {
  NextStatusCheck = std::chrono::steady_clock::now();
  NextPing = NextStatusCheck + std::chrono::seconds( PingSeconds );
  while ( !Stop )
   {
    fd_set ReadSet;
    FD_ZERO( &ReadSet );
    FD_SET( ListenSocket, &ReadSet );
    int MaxSocket = ListenSocket;
    for ( TWebSocketClient const & Client : Clients )
     {
      FD_SET( Client.Socket, &ReadSet );
      MaxSocket = std::max( MaxSocket, Client.Socket );
     }

    timeval Timeout;
    Timeout.tv_sec = 0;
    Timeout.tv_usec = 200000;
    const int SelectResult = select( MaxSocket + 1, &ReadSet, nullptr, nullptr, &Timeout );
    if ( SelectResult < 0 )
     {
      if ( errno == EINTR )
        continue;
      break;
     }
    if ( SelectResult > 0 && FD_ISSET( ListenSocket, &ReadSet ) )
      AcceptClient();

    for ( size_t ClientNr = 0; ClientNr < Clients.size(); )
     {
      if ( !FD_ISSET( Clients[ClientNr].Socket, &ReadSet ) || HandleClientData( Clients[ClientNr] ) )
       {
        ++ClientNr;
        continue;
       }
      shutdown( Clients[ClientNr].Socket, SHUT_RDWR );
      close( Clients[ClientNr].Socket );
      Clients.erase( Clients.begin() + ClientNr );
     }

    const auto Now = std::chrono::steady_clock::now();
    if ( !Clients.empty() && Now >= NextStatusCheck )
     {
      CheckStatus();
      NextStatusCheck = Now + std::chrono::milliseconds( StatusCheckMilliseconds );
     }
    if ( Now >= NextPing )
     {
      Broadcast( 0x9, {} );
      NextPing = Now + std::chrono::seconds( PingSeconds );
     }
   }

  for ( TWebSocketClient const & Client : Clients )
   {
    shutdown( Client.Socket, SHUT_RDWR );
    close( Client.Socket );
   }
  Clients.clear();
 }

TWebSocketServer::TWebSocketServer(void)
: Implementation( std::make_unique<TImplementation>() )
 {
 }

TWebSocketServer::~TWebSocketServer(void) = default;

bool TWebSocketServer::Setup( unsigned Port, TStatusApi & StatusApi, TGNString & Error )
 {
  return Implementation->Setup( Port, StatusApi, Error );
 }

void TWebSocketServer::Loop(void)
 {
  Implementation->Loop();
 }
#endif
