// GnUdpStatus.cpp, Version: 1.14
#include <GnBasics.h>
#include <GnUdpStatus.h>
#include <GnDeviceIdentity.h>
#include <GnCoopTimer.h>

#include <algorithm>
#include <cmath>
#include <new>
#include <utility>
#include <vector>

#if defined( GNPlatformESP )
  #include <ArduinoJson.h>
  #if defined( ESP32 )
    #include <WiFi.h>
  #else
    #include <ESP8266WiFi.h>
  #endif
  #include <WiFiUdp.h>
#elif defined( GNPlatformLinux )
  #include <json-c/json.h>
  #include <arpa/inet.h>
  #include <cerrno>
  #include <cstring>
  #include <fcntl.h>
  #include <ifaddrs.h>
  #include <net/if.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <unistd.h>
#else
  #error Unsupported GnUdpStatus platform
#endif

namespace
 {
constexpr size_t MaxUdpPacketSize = 8192;
constexpr TGnCoopTimer_t NetworkCheckIntervalMs = 1000;

enum class TPacketType : uint8_t
 {
  Invalid,
  Request,
  Status
 };

struct TParsedMeasurement
 {
  TGNString Field;
  float Value = 0.0F;
  bool Valid = false;
 };

struct TParsedPacket
 {
  unsigned Version = 0;
  std::vector<TParsedMeasurement> Measurements;
  TPacketType Type = TPacketType::Invalid;
  TGNString Id;
  std::vector<TGNString> Ids;
 };

#if defined( GNPlatformESP )
TGNString SerializeJsonDocument( JsonDocument const & Document )
 {
  TGNString Result;
  serializeJson( Document, Result );
  return Result;
 }

TGNString BuildRequestJson( std::vector<TGNString> const & Ids )
 {
  JsonDocument Document;
  Document["Version"] = GnUdpStatusVersion;
  Document["Type"] = "Request";
  JsonArray IdArray = Document["ID"].to<JsonArray>();
  for( TGNString const & Id : Ids )
    IdArray.add( Id );
  return SerializeJsonDocument( Document );
 }

TGNString BuildStatusJson( TGNString const & Id, TGNString const & Status,
                           TGnUdpTemperature const & Temperature )
 {
  JsonDocument Document;
  if( deserializeJson( Document, Status.c_str(), Status.length() ) || !Document.is<JsonObjectConst>() )
    return {};
  if( Temperature.Available )
   {
    if( Temperature.Valid && std::isfinite( Temperature.Celsius ) ) Document["Temperature"] = Temperature.Celsius;
    else Document["Temperature"] = nullptr;
   }
  // Reuse the document after serializing Status to avoid retaining two parsed trees.
  const TGNString StatusText = SerializeJsonDocument( Document );
  Document.clear();
  Document["Version"] = GnUdpStatusVersion;
  Document["Type"] = "Status";
  Document["ID"] = Id;
  Document["Status"] = serialized( StatusText.c_str(), StatusText.length() );
  return SerializeJsonDocument( Document );
 }

bool ParsePacket( TGNString const & Payload, TParsedPacket & Packet )
 {
  JsonDocument Document;
  if( deserializeJson( Document, Payload.c_str(), Payload.length() ) )
    return false;
  if( !Document["Version"].is<unsigned>() ||
      !GnUdpStatusVersionSupported( Document["Version"].as<unsigned>() ) )
    return false;

  Packet.Version = Document["Version"].as<unsigned>();
  char const * Type = Document["Type"] | "";
  if( strcmp( Type, "Request" ) == 0 )
   {
    JsonVariantConst IdVariant = Document["ID"];
    if( !IdVariant.is<JsonArrayConst>() )
      return false;
    Packet.Type = TPacketType::Request;
    for( JsonVariantConst Item : IdVariant.as<JsonArrayConst>() )
     {
      if( !Item.is<char const *>() )
        return false;
      Packet.Ids.emplace_back( Item.as<char const *>() );
     }
    return true;
   }
  if( strcmp( Type, "Status" ) != 0 )
    return false;

  JsonVariantConst IdVariant = Document["ID"];
  if( !IdVariant.is<char const *>() )
    return false;
  Packet.Type = TPacketType::Status;
  Packet.Id = IdVariant.as<char const *>();

  if( !Document["Status"].is<JsonObjectConst>() ) return false;
  for( JsonPairConst Member : Document["Status"].as<JsonObjectConst>() )
   {
    TGNString Field = Member.key().c_str();
    if( !GnUdpMeasurementFieldValid( Field ) ) continue;
    JsonVariantConst Value = Member.value();
    const bool Numeric = !Value.is<bool>() && ( Value.is<float>() || Value.is<double>() ||
                         Value.is<int>() || Value.is<long>() );
    const float Number = Numeric ? Value.as<float>() : 0.0F;
    Packet.Measurements.push_back( { Field, Number, Numeric && std::isfinite( Number ) } );
   }
  return true;
 }
#else
TGNString JsonToString( json_object * Object )
 {
  return json_object_to_json_string_ext( Object, JSON_C_TO_STRING_PLAIN );
 }

TGNString BuildRequestJson( std::vector<TGNString> const & Ids )
 {
  json_object * Root = json_object_new_object();
  json_object_object_add( Root, "Version", json_object_new_int( GnUdpStatusVersion ) );
  json_object_object_add( Root, "Type", json_object_new_string( "Request" ) );
  json_object * IdArray = json_object_new_array();
  for( TGNString const & Id : Ids )
    json_object_array_add( IdArray, json_object_new_string( Id.c_str() ) );
  json_object_object_add( Root, "ID", IdArray );
  const TGNString Result = JsonToString( Root );
  json_object_put( Root );
  return Result;
 }

TGNString BuildStatusJson( TGNString const & Id, TGNString const & Status,
                           TGnUdpTemperature const & Temperature )
 {
  json_object * StatusObject = json_tokener_parse( Status.c_str() );
  if( !json_object_is_type( StatusObject, json_type_object ) )
   {
    if( StatusObject ) json_object_put( StatusObject );
    return {};
   }
  if( Temperature.Available )
   {
    json_object_object_add( StatusObject, "Temperature",
      Temperature.Valid && std::isfinite( Temperature.Celsius )
        ? json_object_new_double( Temperature.Celsius ) : nullptr );
   }
  json_object * Root = json_object_new_object();
  json_object_object_add( Root, "Version", json_object_new_int( GnUdpStatusVersion ) );
  json_object_object_add( Root, "Type", json_object_new_string( "Status" ) );
  json_object_object_add( Root, "ID", json_object_new_string( Id.c_str() ) );
  json_object_object_add( Root, "Status", StatusObject );
  const TGNString Result = JsonToString( Root );
  json_object_put( Root );
  return Result;
 }

bool ParsePacket( TGNString const & Payload, TParsedPacket & Packet )
 {
  json_object * Root = json_tokener_parse( Payload.c_str() );
  if( Root == nullptr || !json_object_is_type( Root, json_type_object ) )
   {
    if( Root != nullptr )
      json_object_put( Root );
    return false;
   }

  json_object * Version = nullptr;
  json_object * TypeObject = nullptr;
  json_object * IdObject = nullptr;
  if( !json_object_object_get_ex( Root, "Version", &Version ) ||
      !json_object_is_type( Version, json_type_int ) ||
      !GnUdpStatusVersionSupported( json_object_get_int( Version ) ) ||
      !json_object_object_get_ex( Root, "Type", &TypeObject ) ||
      !json_object_is_type( TypeObject, json_type_string ) ||
      !json_object_object_get_ex( Root, "ID", &IdObject ) )
   {
    json_object_put( Root );
    return false;
   }

  Packet.Version = json_object_get_int( Version );
  const char * Type = json_object_get_string( TypeObject );
  if( strcmp( Type, "Request" ) == 0 )
   {
    if( !json_object_is_type( IdObject, json_type_array ) )
     {
      json_object_put( Root );
      return false;
     }
    Packet.Type = TPacketType::Request;
    for( size_t Index = 0; Index < json_object_array_length( IdObject ); ++Index )
     {
      json_object * Item = json_object_array_get_idx( IdObject, Index );
      if( !json_object_is_type( Item, json_type_string ) )
       {
        json_object_put( Root );
        return false;
       }
      Packet.Ids.emplace_back( json_object_get_string( Item ) );
     }
    json_object_put( Root );
    return true;
   }

  if( strcmp( Type, "Status" ) != 0 || !json_object_is_type( IdObject, json_type_string ) )
   {
    json_object_put( Root );
    return false;
   }
  Packet.Type = TPacketType::Status;
  Packet.Id = json_object_get_string( IdObject );

  json_object * Status = nullptr;
  if( !json_object_object_get_ex( Root, "Status", &Status ) || !json_object_is_type( Status, json_type_object ) )
   { json_object_put( Root ); return false; }
  json_object_object_foreach( Status, Key, Value )
   {
    if( !GnUdpMeasurementFieldValid( Key ) ) continue;
    const bool Numeric = json_object_is_type( Value, json_type_double ) || json_object_is_type( Value, json_type_int );
    const float Number = Numeric ? static_cast<float>( json_object_get_double( Value ) ) : 0.0F;
    Packet.Measurements.push_back( { Key, Number, Numeric && std::isfinite( Number ) } );
   }
  json_object_put( Root );
  return true;
 }
#endif

bool ValidTemperatureId( TGNString const & Id )
 {
  if( Id.length() != 19 || Id[17] != '@' || Id[18] != '3' ) return false;
  bool Nonzero = false;
  for( size_t I = 0; I < 17; ++I )
   {
    if( I % 3 == 2 ) { if( Id[I] != ':' ) return false; }
    else
     {
      if( !( ( Id[I] >= '0' && Id[I] <= '9' ) || ( Id[I] >= 'A' && Id[I] <= 'F' ) ) ) return false;
      Nonzero = Nonzero || Id[I] != '0';
     }
   }
  return Nonzero;
 }

bool RequestMatches( TParsedPacket const & Packet, TGNString const & LocalId )
 {
  if( Packet.Type != TPacketType::Request )
    return false;
  return Packet.Ids.empty() ||
         std::find( Packet.Ids.begin(), Packet.Ids.end(), LocalId ) != Packet.Ids.end();
 }
 }

class TGnUdpStatus::TImplementation
 {
 public:
  TImplementation(void);

  struct TTemperatureCache
   {
    TTemperatureCache( TGNString Id, float Celsius, bool Valid, TGNString Field )
    : Id( std::move( Id ) )
    , Field( std::move( Field ) )
    , Celsius( Celsius )
    , Valid( Valid )
     {
     }

    TGNString Id;
    TGNString Field;
    float Celsius = 0.0F;
    TGnElapsedTimerMs AgeTimer;
    bool Valid = false;
   };

  bool Setup( TGNString Id, TStatusProvider StatusProvider,
              TTemperatureProvider TemperatureProvider );
  TGNString const & Error(void) const                         { return FError; }
  void Loop(void);
  void RequestAll(void)                                      { SendRaw( BuildRequestJson( {} ) ); }
  void Request( TGNString const & Id )                       { SendRaw( BuildRequestJson( { Id } ) ); }
  void Request( std::vector<TGNString> const & Ids )         { SendRaw( BuildRequestJson( Ids ) ); }
  bool GetTemperature( TGNString const & Id, float & Celsius ) const;
  bool GetMeasurement( TGNString const & Id, TGNString const & Field, float & Value ) const;
  TGnCoopTimer_t TemperatureAgeMs( TGNString const & Id ) const;

 private:
  bool NetworkConnected(void) const;
  bool OpenSocket(void);
  void ReceivePackets(void);
  bool SendStatus(void);
  bool SendStatus( TGNString const & Status, uint64_t Signature,
                   TGnUdpTemperature const & Temperature );
  bool SendRaw( TGNString const & Payload );
  void HandlePacket( TGNString const & Payload );
  void CacheTemperature( TGNString const & Id, bool Valid, float Celsius, TGNString const & Field );
  bool TemperatureChanged( TGnUdpTemperature const & Current ) const;
  void RememberTemperature( TGnUdpTemperature const & Current );
  void NetworkCheckTimerCb( TGnCoopTimerBase * Tmr, uint16_t IntervalsElapsed );
  void StatusCheckTimerCb( TGnCoopTimerBase * Tmr, uint16_t IntervalsElapsed );
  void PublishTimerCb( TGnCoopTimerBase * Tmr, uint16_t IntervalsElapsed );

 private:
  TGNString FId;
  TStatusProvider FStatusProvider;
  TTemperatureProvider FTemperatureProvider;
  std::vector<TTemperatureCache> FTemperatures;
  TGNString FError;
  uint64_t FLastStatusSignature = 0;
  bool FLastStatusSignatureKnown = false;
  TGnUdpTemperature FLastTemperature;
  bool FLastTemperatureKnown = false;
  TGnCoopTimerMs FNetworkCheckTimer;
  TGnCoopTimerMs FStatusCheckTimer;
  TGnCoopTimerMs FPublishTimer;
  bool FNetworkCheckDue = false;
  bool FStatusCheckDue = false;
  bool FPublishDue = false;
  bool FNetworkConnected = false;
  bool FNetworkCheckKnown = false;
  bool FNetworkWasConnected = false;
  bool FReady = false;

#if defined( GNPlatformESP )
  WiFiUDP FUdp;
#else
  int FSocket = -1;
#endif
 };

TGnUdpStatus::TImplementation::TImplementation(void)
: FNetworkCheckTimer( Closure( this, &TImplementation::NetworkCheckTimerCb ), NetworkCheckIntervalMs, false )
, FStatusCheckTimer( Closure( this, &TImplementation::StatusCheckTimerCb ), GnUdpStatusCheckIntervalMs, false )
, FPublishTimer( Closure( this, &TImplementation::PublishTimerCb ), GnUdpStatusPublishIntervalMs, false )
 {
 }

bool TGnUdpStatus::TImplementation::OpenSocket(void)
 {
#if defined( GNPlatformESP )
  if( !FUdp.begin( GnUdpStatusPort ) )
   {
    FError = F( "Cannot bind UDP status port" );
    return false;
   }
#else
  FSocket = socket( AF_INET, SOCK_DGRAM, 0 );
  if( FSocket < 0 )
   {
    FError = TGNString( "Cannot create UDP status socket: " ) + strerror( errno );
    return false;
   }
  int Enabled = 1;
  if( setsockopt( FSocket, SOL_SOCKET, SO_REUSEADDR, &Enabled, sizeof( Enabled ) ) < 0 ||
      setsockopt( FSocket, SOL_SOCKET, SO_BROADCAST, &Enabled, sizeof( Enabled ) ) < 0 )
   {
    FError = TGNString( "Cannot configure UDP status socket: " ) + strerror( errno );
    close( FSocket );
    FSocket = -1;
    return false;
   }
  sockaddr_in Address{};
  Address.sin_family = AF_INET;
  Address.sin_addr.s_addr = htonl( INADDR_ANY );
  Address.sin_port = htons( GnUdpStatusPort );
  if( bind( FSocket, reinterpret_cast<sockaddr *>( &Address ), sizeof( Address ) ) < 0 )
   {
    FError = TGNString( "Cannot bind UDP status socket: " ) + strerror( errno );
    close( FSocket );
    FSocket = -1;
    return false;
   }
  const int Flags = fcntl( FSocket, F_GETFL, 0 );
  if( Flags >= 0 )
    fcntl( FSocket, F_SETFL, Flags | O_NONBLOCK );
#endif
  FError.clear();
  return true;
 }

bool TGnUdpStatus::TImplementation::Setup( TGNString Id, TStatusProvider StatusProvider,
                                            TTemperatureProvider TemperatureProvider )
 {
  if( FReady )
   {
    FError = "UDP status already configured";
    return false;
   }
  if( Id.empty() || !StatusProvider )
   {
    FError = "Invalid UDP status configuration";
    return false;
   }
  if( !OpenSocket() )
    return false;
  FId = std::move( Id );
  FStatusProvider = std::move( StatusProvider );
  FTemperatureProvider = std::move( TemperatureProvider );
  FNetworkCheckDue = true;
  FStatusCheckDue = false;
  FPublishDue = false;
  FNetworkConnected = false;
  FNetworkCheckKnown = false;
  FReady = true;
  FNetworkCheckTimer.Start();
  FStatusCheckTimer.Start();
  FPublishTimer.Start();
  FNetworkWasConnected = false;
  FError.clear();
  return true;
 }

bool TGnUdpStatus::TImplementation::NetworkConnected(void) const
 {
#if defined( GNPlatformESP )
  return WiFi.status() == WL_CONNECTED;
#else
  if( FSocket < 0 )
    return false;
  ifaddrs * Interfaces = nullptr;
  if( getifaddrs( &Interfaces ) != 0 )
    return false;
  bool Connected = false;
  for( ifaddrs * Interface = Interfaces; Interface != nullptr; Interface = Interface->ifa_next )
   {
    if( Interface->ifa_addr != nullptr && Interface->ifa_addr->sa_family == AF_INET &&
        ( Interface->ifa_flags & IFF_UP ) != 0 && ( Interface->ifa_flags & IFF_LOOPBACK ) == 0 &&
        ( Interface->ifa_flags & IFF_BROADCAST ) != 0 && Interface->ifa_broadaddr != nullptr )
     {
      Connected = true;
      break;
     }
   }
  freeifaddrs( Interfaces );
  return Connected;
#endif
 }

bool TGnUdpStatus::TImplementation::SendRaw( TGNString const & Payload )
 {
  if( !FReady || Payload.empty() || Payload.length() > MaxUdpPacketSize )
    return false;
#if defined( GNPlatformESP )
  if( WiFi.status() != WL_CONNECTED )
    return false;
  const IPAddress Ip = WiFi.localIP();
  const IPAddress Mask = WiFi.subnetMask();
  IPAddress Broadcast;
  for( unsigned Byte = 0; Byte < 4; ++Byte )
    Broadcast[Byte] = static_cast<uint8_t>( Ip[Byte] | static_cast<uint8_t>( ~Mask[Byte] ) );
  if( !FUdp.beginPacket( Broadcast, GnUdpStatusPort ) )
    return false;
  const size_t Written = FUdp.write( reinterpret_cast<uint8_t const *>( Payload.data() ), Payload.length() );
  return Written == Payload.length() && FUdp.endPacket() == 1;
#else
  ifaddrs * Interfaces = nullptr;
  if( getifaddrs( &Interfaces ) != 0 )
    return false;
  bool Sent = false;
  for( ifaddrs * Interface = Interfaces; Interface != nullptr; Interface = Interface->ifa_next )
   {
    if( Interface->ifa_addr == nullptr || Interface->ifa_addr->sa_family != AF_INET ||
        ( Interface->ifa_flags & IFF_UP ) == 0 || ( Interface->ifa_flags & IFF_LOOPBACK ) != 0 ||
        ( Interface->ifa_flags & IFF_BROADCAST ) == 0 || Interface->ifa_broadaddr == nullptr )
      continue;
    sockaddr_in Destination = *reinterpret_cast<sockaddr_in *>( Interface->ifa_broadaddr );
    Destination.sin_port = htons( GnUdpStatusPort );
    const ssize_t Count = sendto( FSocket, Payload.data(), Payload.length(), 0,
                                  reinterpret_cast<sockaddr *>( &Destination ), sizeof( Destination ) );
    Sent = Sent || Count == static_cast<ssize_t>( Payload.length() );
   }
  freeifaddrs( Interfaces );
  return Sent;
#endif
 }

void TGnUdpStatus::TImplementation::RememberTemperature( TGnUdpTemperature const & Current )
 {
  FLastTemperature = Current;
  FLastTemperatureKnown = true;
 }

bool TGnUdpStatus::TImplementation::TemperatureChanged( TGnUdpTemperature const & Current ) const
 {
  if( !FLastTemperatureKnown )
    return true;
  if( Current.Available != FLastTemperature.Available )
    return true;
  if( !Current.Available )
    return false;
  if( Current.Valid != FLastTemperature.Valid )
    return true;
  return Current.Valid &&
         std::fabs( Current.Celsius - FLastTemperature.Celsius ) >= GnUdpTemperatureChangeThresholdC;
 }

bool TGnUdpStatus::TImplementation::SendStatus( TGNString const & Status, uint64_t Signature,
                                                 TGnUdpTemperature const & Temperature )
 {
  const TGNString Packet = BuildStatusJson( GnDeviceIdentity.WireId(), Status, Temperature );
  if( !SendRaw( Packet ) )
    return false;

  FPublishDue = false;
  FPublishTimer.Start();
  FLastStatusSignature = Signature;
  FLastStatusSignatureKnown = true;
  RememberTemperature( Temperature );
  return true;
 }

bool TGnUdpStatus::TImplementation::SendStatus(void)
 {
  if( !FReady )
    return false;
  uint64_t Signature = 0;
  const TGNString Status = FStatusProvider( Signature );
  if( Status.empty() )
    return false;
  const TGnUdpTemperature Temperature = FTemperatureProvider ? FTemperatureProvider() : TGnUdpTemperature{};
  return SendStatus( Status, Signature, Temperature );
 }

void TGnUdpStatus::TImplementation::CacheTemperature( TGNString const & Id, bool Valid, float Celsius, TGNString const & Field )
 {
  auto Entry = std::find_if( FTemperatures.begin(), FTemperatures.end(), [&]( TTemperatureCache const & Item )
   {
    return Item.Id == Id && Item.Field == Field;
   } );
  if( Entry == FTemperatures.end() )
   {
    FTemperatures.emplace_back( Id, Celsius, Valid, Field );
    return;
   }
  Entry->Celsius = Celsius;
  Entry->AgeTimer.Start();
  Entry->Valid = Valid;
 }

void TGnUdpStatus::TImplementation::HandlePacket( TGNString const & Payload )
 {
  TParsedPacket Packet;
  if( !ParsePacket( Payload, Packet ) )
    return;
  if( RequestMatches( Packet, GnDeviceIdentity.WireId() ) )
   {
    SendStatus();
    return;
   }
  if( Packet.Type != TPacketType::Status || Packet.Id.empty() || Packet.Id == GnDeviceIdentity.WireId() ||
      !ValidTemperatureId( Packet.Id ) )
    return;
  // Each status is a complete snapshot: never retain an absent field.
  FTemperatures.erase( std::remove_if( FTemperatures.begin(), FTemperatures.end(),
    [&]( TTemperatureCache const & Entry ) { return Entry.Id == Packet.Id; } ), FTemperatures.end() );
  for( auto const & Measurement : Packet.Measurements )
    CacheTemperature( Packet.Id, Measurement.Valid, Measurement.Value, Measurement.Field );

 }

void TGnUdpStatus::TImplementation::ReceivePackets(void)
 {
  if( !FReady )
    return;
#if defined( GNPlatformESP )
  while( true )
   {
    const int PacketSize = FUdp.parsePacket();
    if( PacketSize <= 0 )
      break;
    if( static_cast<size_t>( PacketSize ) > MaxUdpPacketSize )
     {
      while( FUdp.available() > 0 )
        FUdp.read();
      continue;
     }
    std::unique_ptr<char[]> Buffer( new ( std::nothrow ) char[static_cast<size_t>( PacketSize ) + 1] );
    if( !Buffer )
     {
      while( FUdp.available() > 0 )
        FUdp.read();
      continue;
     }
    const int Count = FUdp.read( reinterpret_cast<unsigned char *>( Buffer.get() ), PacketSize );
    if( Count == PacketSize )
     {
      Buffer[static_cast<size_t>( Count )] = '\0';
      HandlePacket( TGNString( Buffer.get() ) );
     }
   }
#else
  char Buffer[MaxUdpPacketSize + 1];
  while( true )
   {
    const ssize_t Count = recvfrom( FSocket, Buffer, MaxUdpPacketSize, MSG_DONTWAIT, nullptr, nullptr );
    if( Count < 0 )
     {
      if( errno == EAGAIN || errno == EWOULDBLOCK )
        break;
      return;
     }
    if( Count == 0 )
      break;
    Buffer[Count] = '\0';
    HandlePacket( TGNString( Buffer, static_cast<size_t>( Count ) ) );
   }
#endif
 }

bool TGnUdpStatus::TImplementation::GetTemperature( TGNString const & Id, float & Celsius ) const
 {
  return GetMeasurement( Id, "Temperature", Celsius );
 }

bool TGnUdpStatus::TImplementation::GetMeasurement( TGNString const & Id, TGNString const & Field, float & Value ) const
 {
  if( !GnUdpMeasurementFieldValid( Field ) ) return false;
  const auto Entry = std::find_if( FTemperatures.begin(), FTemperatures.end(), [&]( TTemperatureCache const & Item )
   { return Item.Id == Id && Item.Field == Field; } );
  if( Entry == FTemperatures.end() || !Entry->Valid || Entry->AgeTimer.Elapsed() > GnUdpTemperatureMaxAgeMs )
    return false;
  Value = Entry->Celsius;
  return true;
 }

TGnCoopTimer_t TGnUdpStatus::TImplementation::TemperatureAgeMs( TGNString const & Id ) const
 {
  const auto Entry = std::find_if( FTemperatures.begin(), FTemperatures.end(), [&]( TTemperatureCache const & Item )
   {
    return Item.Id == Id;
   } );
  return Entry == FTemperatures.end() ? TGnCoopTimer_MAX : Entry->AgeTimer.Elapsed();
 }

void TGnUdpStatus::TImplementation::NetworkCheckTimerCb( TGnCoopTimerBase *, uint16_t )
 {
  FNetworkCheckDue = true;
 }

void TGnUdpStatus::TImplementation::StatusCheckTimerCb( TGnCoopTimerBase *, uint16_t )
 {
  FStatusCheckDue = true;
 }

void TGnUdpStatus::TImplementation::PublishTimerCb( TGnCoopTimerBase *, uint16_t )
 {
  FPublishDue = true;
 }

void TGnUdpStatus::TImplementation::Loop(void)
 {
  if( !FReady )
    return;

  if( !FNetworkCheckKnown || FNetworkCheckDue )
   {
    FNetworkCheckDue = false;
    FNetworkConnected = NetworkConnected();
    FNetworkCheckKnown = true;
   }
  if( FNetworkConnected && !FNetworkWasConnected )
   {
    if( SendStatus() )
      FNetworkWasConnected = true;
     else
      FNetworkConnected = false;
   }
   else if( !FNetworkConnected )
    FNetworkWasConnected = false;
  if( !FNetworkConnected )
    return;

  ReceivePackets();

  if( FStatusCheckDue )
   {
    FStatusCheckDue = false;
    uint64_t Signature = 0;
    const TGNString Status = FStatusProvider( Signature );
    const TGnUdpTemperature Temperature = FTemperatureProvider ? FTemperatureProvider() : TGnUdpTemperature{};
    if( !FLastStatusSignatureKnown || Signature != FLastStatusSignature || TemperatureChanged( Temperature ) )
     {
      SendStatus( Status, Signature, Temperature );
      return;
     }
   }
  if( FPublishDue )
   {
    FPublishDue = false;
    SendStatus();
   }
 }

TGnUdpStatus::TGnUdpStatus(void)
: FImplementation( std::make_unique<TImplementation>() )
 {
 }

TGnUdpStatus::~TGnUdpStatus(void) = default;

bool TGnUdpStatus::Setup( TGNString Id, TStatusProvider StatusProvider,
                          TTemperatureProvider TemperatureProvider )
 {
  return FImplementation->Setup( std::move( Id ), std::move( StatusProvider ),
                                 std::move( TemperatureProvider ) );
 }

TGNString const & TGnUdpStatus::Error(void) const
 {
  return FImplementation->Error();
 }

void TGnUdpStatus::RequestAll(void)
 {
  FImplementation->RequestAll();
 }

void TGnUdpStatus::Request( TGNString const & Id )
 {
  FImplementation->Request( Id );
 }

void TGnUdpStatus::Request( std::vector<TGNString> const & Ids )
 {
  FImplementation->Request( Ids );
 }

bool TGnUdpStatus::GetTemperature( TGNString const & Id, float & Celsius ) const
 {
  return FImplementation->GetTemperature( Id, Celsius );
 }

TGnCoopTimer_t TGnUdpStatus::TemperatureAgeMs( TGNString const & Id ) const
 {
  return FImplementation->TemperatureAgeMs( Id );
 }

void TGnUdpStatus::Loop(void)
 {
  FImplementation->Loop();
 }

bool TGnUdpStatus::GetMeasurement( TGNString const & Id, TGNString const & Field, float & Value ) const
 {
  return FImplementation->GetMeasurement( Id, Field, Value );
 }
