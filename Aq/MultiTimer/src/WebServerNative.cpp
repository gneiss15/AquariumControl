// WebServerNative.cpp, Version: 2.38
#include <GnBasics.h>
#include "BuildConfig.h"
#if defined( SYSTEM_DUMMY ) || defined( SYSTEM_RASPI )
  #include "IWebServer.h"
  #include <arpa/inet.h>
  #include <atomic>
  #include <cerrno>
  #include <cctype>
  #include <cstdlib>
  #include <netinet/in.h>
  #include <sys/select.h>
  #include <sys/socket.h>
  #include <thread>
  #include <unistd.h>

class TIWebServer::TImplementation
 {
 public:
                      TImplementation(void) = default;
                      ~TImplementation(void);
  bool                Setup( unsigned Port, TWebApplication & App, TGNString & Error );
  void                Loop(void);

 private:
  static bool         EqualsIgnoreCase( TGNString const & Left, char const * Right );
  static TGNString    Trim( TGNString const & Text );
  static char const * StatusText( int Status );
  static bool         SendAll( int Socket, char const * Data, size_t Size );
  bool                ReadRequest( int Socket, TGNString & Method, TGNString & Path, TGNString & Body ) const;
  void                HandleClient( int Socket ) const;
  void                Run(void);

 private:
  TWebApplication *   Application = nullptr;
  std::thread         Thread;
  std::atomic<bool>   Stop = false;
  int                 ListenSocket = -1;
 };

TIWebServer::TImplementation::~TImplementation(void)
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

bool TIWebServer::TImplementation::Setup( unsigned Port, TWebApplication & App, TGNString & Error )
 {
  if ( ListenSocket >= 0 )
   {
    Error = "Web server already started";
    return false;
   }

  ListenSocket = socket( AF_INET, SOCK_STREAM, 0 );
  if ( ListenSocket < 0 )
   {
    Error = TGNString( "Cannot create web-server socket: " ) + strerror( errno );
    return false;
   }

  int ReuseAddress = 1;
  if ( setsockopt( ListenSocket, SOL_SOCKET, SO_REUSEADDR, &ReuseAddress, sizeof( ReuseAddress ) ) < 0 )
   {
    Error = TGNString( "Cannot configure web-server socket: " ) + strerror( errno );
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
    Error = TGNString( "Cannot bind web-server socket: " ) + strerror( errno );
    close( ListenSocket );
    ListenSocket = -1;
    return false;
   }

  if ( listen( ListenSocket, 8 ) < 0 )
   {
    Error = TGNString( "Cannot listen on web-server socket: " ) + strerror( errno );
    close( ListenSocket );
    ListenSocket = -1;
    return false;
   }

  Application = &App;
  Stop = false;
  Thread = std::thread( [this] { Run(); } );
  Error.clear();
  return true;
 }

void TIWebServer::TImplementation::Loop(void)
 {
 }

bool TIWebServer::TImplementation::EqualsIgnoreCase( TGNString const & Left, char const * Right )
 {
  if ( Right == nullptr || Left.length() != strlen( Right ) )
    return false;

  for ( size_t Index = 0; Index < Left.length(); ++Index )
   {
    unsigned char LeftChar = static_cast<unsigned char>( Left[Index] );
    unsigned char RightChar = static_cast<unsigned char>( Right[Index] );
    if ( std::tolower( LeftChar ) != std::tolower( RightChar ) )
      return false;
   }
  return true;
 }

TGNString TIWebServer::TImplementation::Trim( TGNString const & Text )
 {
  size_t First = 0;
  while ( First < Text.length() && std::isspace( static_cast<unsigned char>( Text[First] ) ) )
    ++First;

  size_t Last = Text.length();
  while ( Last > First && std::isspace( static_cast<unsigned char>( Text[Last - 1] ) ) )
    --Last;
  return Text.substr( First, Last - First );
 }

char const * TIWebServer::TImplementation::StatusText( int Status )
 {
  switch ( Status )
   {
  case 200:
    return "OK";
  case 400:
    return "Bad Request";
  case 404:
    return "Not Found";
  case 500:
    return "Internal Server Error";
  default:
    return "Unknown";
   }
 }

bool TIWebServer::TImplementation::SendAll( int Socket, char const * Data, size_t Size )
 {
  while ( Size > 0 )
   {
    ssize_t Sent = send( Socket, Data, Size, MSG_NOSIGNAL );
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

bool TIWebServer::TImplementation::ReadRequest( int Socket, TGNString & Method, TGNString & Path, TGNString & Body ) const
 {
  static size_t const MaxHeaderSize = 64 * 1024;
  static size_t const MaxBodySize = 4 * 1024 * 1024;

  TGNString           Request;
  size_t              HeaderEnd = TGNString::npos;
  char                Buffer[4096];
  while ( HeaderEnd == TGNString::npos )
   {
    ssize_t Count = recv( Socket, Buffer, sizeof( Buffer ), 0 );
    if ( Count < 0 )
     {
      if ( errno == EINTR )
        continue;
      return false;
     }
    if ( Count == 0 )
      return false;

    Request.append( Buffer, static_cast<size_t>( Count ) );
    if ( Request.length() > MaxHeaderSize )
      return false;
    HeaderEnd = Request.find( "\r\n\r\n" );
   }

  size_t RequestLineEnd = Request.find( "\r\n" );
  if ( RequestLineEnd == TGNString::npos )
    return false;

  TGNString RequestLine = Request.substr( 0, RequestLineEnd );
  size_t    MethodEnd = RequestLine.find( ' ' );
  size_t    PathEnd = MethodEnd == TGNString::npos ? TGNString::npos : RequestLine.find( ' ', MethodEnd + 1 );
  if ( MethodEnd == TGNString::npos || PathEnd == TGNString::npos )
    return false;

  Method = RequestLine.substr( 0, MethodEnd );
  Path = RequestLine.substr( MethodEnd + 1, PathEnd - MethodEnd - 1 );

  size_t ContentLength = 0;
  size_t HeaderPos = RequestLineEnd + 2;
  while ( HeaderPos < HeaderEnd )
   {
    size_t HeaderLineEnd = Request.find( "\r\n", HeaderPos );
    if ( HeaderLineEnd == TGNString::npos || HeaderLineEnd > HeaderEnd )
      return false;

    TGNString HeaderLine = Request.substr( HeaderPos, HeaderLineEnd - HeaderPos );
    size_t    Separator = HeaderLine.find( ':' );
    if ( Separator == TGNString::npos )
      return false;

    TGNString HeaderName = Trim( HeaderLine.substr( 0, Separator ) );
    TGNString HeaderValue = Trim( HeaderLine.substr( Separator + 1 ) );
    if ( EqualsIgnoreCase( HeaderName, "Content-Length" ) )
     {
      char * End = nullptr;
      errno = 0;
      unsigned long long Value = strtoull( HeaderValue.c_str(), &End, 10 );
      if ( errno != 0 || End == HeaderValue.c_str() || *End != '\0' || Value > MaxBodySize )
        return false;
      ContentLength = static_cast<size_t>( Value );
     }
    HeaderPos = HeaderLineEnd + 2;
   }

  size_t BodyPos = HeaderEnd + 4;
  while ( Request.length() - BodyPos < ContentLength )
   {
    ssize_t Count = recv( Socket, Buffer, sizeof( Buffer ), 0 );
    if ( Count < 0 )
     {
      if ( errno == EINTR )
        continue;
      return false;
     }
    if ( Count == 0 )
      return false;

    Request.append( Buffer, static_cast<size_t>( Count ) );
    if ( Request.length() - BodyPos > MaxBodySize )
      return false;
   }

  Body.assign( Request.data() + BodyPos, ContentLength );
  return true;
 }

void TIWebServer::TImplementation::HandleClient( int Socket ) const
 {
  TGNString    Method;
  TGNString    Path;
  TGNString    Body;
  THttpResponse Response;
  if ( ReadRequest( Socket, Method, Path, Body ) )
   {
    Response = Application->Handle( Method, Path, Body );
   }
   else
   {
    Response = { 400, TContentType::PlainText, "Bad request" };
   }

  TGNString Header = StringFormat( "HTTP/1.1 %d %s\r\n"
                                   "Content-Type: %s\r\n"
                                   "Content-Length: %zu\r\n",
                                   Response.Status, StatusText( Response.Status ), Response.ContentTypeText(), Response.Body.length() );
  if ( Response.AllowCors )
   {
    Header += "Access-Control-Allow-Origin: *\r\n";
    Header += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    Header += "Access-Control-Allow-Headers: Content-Type\r\n";
   }
  Header += "Connection: close\r\n\r\n";
  if ( SendAll( Socket, Header.data(), Header.length() ) )
   {
    SendAll( Socket, Response.Body.data(), Response.Body.length() );
   }
  if( Response.ExitAfterResponse )
    Application->RequestExit();
 }

void TIWebServer::TImplementation::Run(void)
 {
  int ServerSocket = ListenSocket;
  while ( !Stop )
   {
    fd_set ReadSet;
    FD_ZERO( &ReadSet );
    FD_SET( ServerSocket, &ReadSet );

    timeval Timeout;
    Timeout.tv_sec = 0;
    Timeout.tv_usec = 200000;
    int SelectResult = select( ServerSocket + 1, &ReadSet, nullptr, nullptr, &Timeout );
    if ( SelectResult < 0 )
     {
      if ( errno == EINTR )
        continue;
      break;
     }
    if ( SelectResult == 0 || Stop )
      continue;

    int ClientSocket = accept( ServerSocket, nullptr, nullptr );
    if ( ClientSocket < 0 )
     {
      if ( errno == EINTR )
        continue;
      if ( Stop )
        break;
      continue;
     }

    timeval ClientTimeout;
    ClientTimeout.tv_sec = 1;
    ClientTimeout.tv_usec = 0;
    setsockopt( ClientSocket, SOL_SOCKET, SO_RCVTIMEO, &ClientTimeout, sizeof( ClientTimeout ) );
    setsockopt( ClientSocket, SOL_SOCKET, SO_SNDTIMEO, &ClientTimeout, sizeof( ClientTimeout ) );
    HandleClient( ClientSocket );
    shutdown( ClientSocket, SHUT_RDWR );
    close( ClientSocket );
   }
 }

TIWebServer::TIWebServer(void)
: Implementation( std::make_unique<TImplementation>() )
 {
 }

TIWebServer::~TIWebServer(void) = default;

bool TIWebServer::Setup( unsigned Port, TWebApplication & Application, TGNString & Error )
 {
  return Implementation->Setup( Port, Application, Error );
 }

void TIWebServer::Loop(void)
 {
  Implementation->Loop();
 }
#endif
