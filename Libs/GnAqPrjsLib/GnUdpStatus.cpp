// GnUdpStatus.cpp, Version: 1.34
#include <GnBasics.h>
#include <GnUdpStatus.h>
#include <GnDeviceIdentity.h>
#if GN_FEEDER_UDP
  #define Do_Feeder_RX
  #define Do_Feeder_TX
#else
  #define Do_Device_RX
#endif
#include <Messages.h>
#include <GnUdpCompact.h>
#include <GnUdpControl.h>
#include <GnUdpSources.h>
#include <GnHeatPiPeer.h>
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
  #if defined( ESP32 )
    #include <lwip/sockets.h>
    #include <lwip/inet.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <cerrno>
    #include <cstring>
  #else
    #include <WiFiUdp.h>
  #endif
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
  TGNString StatusText;
  unsigned Version = 0;
  std::vector<TParsedMeasurement> Measurements;
  TGnChargeLearningPolicy Learning;
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



TGNString BuildStatusJson( TGNString const & Id, TGNString const & Status,
                           TGnUdpTemperature const & Temperature, TGnUdpInfoParts * Parts = nullptr )
 {
  JsonDocument Document;
  if( deserializeJson( Document, Status.c_str(), Status.length() ) || !Document.is<JsonObjectConst>() )
    return {};
  Document["PowerMeter"].as<JsonObject>().remove( "Diagnostics" );
  if( Temperature.Available )
   {
    if( Temperature.Valid && std::isfinite( Temperature.Celsius ) ) Document["Temp"] = Temperature.Celsius;
    else Document["Temp"] = nullptr;
   }
  // Reuse the document after serializing Status to avoid retaining two parsed trees.
  const TGNString StatusText = SerializeJsonDocument( Document );
  Document.clear();
  TGnUdpInfoParts Local;
  return GnUdpCompactStatus(Id,StatusText,Parts?*Parts:Local);
 }


#else
TGNString JsonToString( json_object * Object )
 {
  return json_object_to_json_string_ext( Object, JSON_C_TO_STRING_PLAIN );
 }



TGNString BuildStatusJson( TGNString const & Id, TGNString const & Status,
                           TGnUdpTemperature const & Temperature, TGnUdpInfoParts * Parts = nullptr )
 {
  json_object * StatusObject = json_tokener_parse( Status.c_str() );
  if( !json_object_is_type( StatusObject, json_type_object ) )
   {
    if( StatusObject ) json_object_put( StatusObject );
    return {};
   }
  json_object * Meter = nullptr;
  if( json_object_object_get_ex( StatusObject, "PowerMeter", &Meter ) )
    json_object_object_del( Meter, "Diag" );
  if( Temperature.Available )
   {
    json_object_object_add( StatusObject, "Temp",
      Temperature.Valid && std::isfinite( Temperature.Celsius )
        ? json_object_new_double( Temperature.Celsius ) : nullptr );
   }
  TGnUdpInfoParts Local;
  const TGNString Result=GnUdpCompactStatus(Id,JsonToString(StatusObject),Parts?*Parts:Local);
  json_object_put(StatusObject);
  return Result;
 }


#endif

TGNString BuildRequestJson( std::vector<TGNString> const & Ids )
 {
  TUdpRequestMessage Request;Request.ID=Ids;
  return GnWriteUdpMessage(Request);
 }

// Extract an already validated Request/Status; do not parse the UDP text again.
bool ParsePacket( TGnWireValue Root, TParsedPacket & Packet )
 {
  Packet.Version=GnUdpStatusVersion;
  if(MessageType(Root)==TMsgType::Request)
   {
    Packet.Type=TPacketType::Request;
    for(auto Id:Root.Get("ID").Items())Packet.Ids.push_back(Id.Text());
    return true;
   }
  if(MessageType(Root)!=TMsgType::Status)return false;
  Packet.Type=TPacketType::Status;Packet.Id=Root.Get("ID").Text();
  auto Status=Root.Get("Status");Packet.StatusText=Status.Json();
  auto Allowed=Status.Get("ChargeLearnAllowed"),Counter=Status.Get("BootCnt");
  if(Status.Has("ChargeLearnAllowed") && Status.Has("BootCnt") && Allowed.IsBool() && GnTypeValid(Counter,TGnJsonType<uint32_t>{}))
   { Packet.Learning.Available=true;Packet.Learning.Allowed=Allowed.Bool();Packet.Learning.BootCounter=Counter.UInt(); }
  for(auto const & P:Status.Members())
   {
    if(!GnUdpMeasurementFieldValid(P.first))continue;
    const bool Numeric=P.second.IsNumber() && std::isfinite(P.second.Number());
    Packet.Measurements.push_back({P.first=="Temp"?"Temperature":P.first=="BatteryPct"?"BatteryPercent":P.first,Numeric?static_cast<float>(P.second.Number()):0.0F,Numeric});
   }
  return true;
 }

[[maybe_unused]] bool ParsePacket( TGNString const & Payload, TParsedPacket & Packet )
 {
  TGnWireDocument Document(Payload);
  return GnValidateUdpMessage(Document.Root()) && ParsePacket(Document.Root(),Packet);
 }

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

// Only operational changes trigger an early broadcast. Measurements are periodic;
// completed temperature samples have their own monotonic revision.
uint64_t StatusEventSignature( TGNString const & Status )
 {
  TGNString Events;
#if defined( GNPlatformESP )
  JsonDocument Document;
  if( deserializeJson( Document, Status.c_str(), Status.length() ) ) return 0;
  Document.remove( "CurrentTime" );
  Document.remove( "Temperature" );
  Document.remove( "HeatPi" );
  Document.remove( "ChargeLearning" );
  JsonObject Meter = Document["PowerMeter"].as<JsonObject>();
  for( char const * Key : { "Voltage", "Current", "ActivePower", "Diagnostics" } ) Meter.remove( Key );
  for( JsonObject Measurement : Document["Meas"].as<JsonArray>() )
   {
    Measurement.remove( "Value" );
    Measurement.remove( "UpdatedAt" );
   }
  Events = SerializeJsonDocument( Document );
#else
  json_object * Document = json_tokener_parse( Status.c_str() );
  if( !Document ) return 0;
  json_object_object_del( Document, "CurrentTime" );
  json_object_object_del( Document, "Temp" );
  json_object_object_del( Document, "HeatDiag" );
  json_object_object_del( Document, "ChargeLearn" );
  json_object * Meter = nullptr, * Measurements = nullptr;
  if( json_object_object_get_ex( Document, "PowerMeter", &Meter ) )
    for( char const * Key : { "Voltage", "Current", "ActivePower", "Diagnostics" } ) json_object_object_del( Meter, Key );
  if( json_object_object_get_ex( Document, "Meas", &Measurements ) && json_object_is_type( Measurements, json_type_array ) )
    for( size_t Index = 0; Index < json_object_array_length( Measurements ); ++Index )
     {
      json_object * Measurement = json_object_array_get_idx( Measurements, Index );
      json_object_object_del( Measurement, "Value" );
      json_object_object_del( Measurement, "UpdatedAt" );
     }
  Events = JsonToString( Document );
  json_object_put( Document );
#endif
  return GnHash64( Events.data(), Events.length() );
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
    TGnChargeLearningPolicy Learning;
   };

  bool Setup( TGNString Id, TStatusProvider StatusProvider,
              TTemperatureProvider TemperatureProvider );
  TGNString const & Error(void) const                         { return FError; }
  void Loop(void);
  void RequestAll(void)                                      { SendRaw( BuildRequestJson( {} ) ); }
  void Request( TGNString const & Id )                       { SendRaw( BuildRequestJson( { Id } ) ); }
  void Request( std::vector<TGNString> const & Ids )         { SendRaw( BuildRequestJson( Ids ) ); }
  void WatchStatus( TGNString const & Id )
   {
    if( Id == FWatchedId ) return;
    FWatchedId = Id; FWatchedStatus.clear(); FWatchedRevision = 0; FWatchedCompact.clear(); FDescriptions = TGnUdpDescriptions{};
   }
  bool GetWatchedStatus( TGNString const & Id, TGNString & Status, uint64_t & Revision ) const
   {
    if( Id != FWatchedId || FWatchedStatus.empty() || GnMillis64()-FWatchedReceivedAt > 60000 ) return false;
    Status = FWatchedStatus; Revision = FWatchedRevision; return true;
   }
  TGNString ResolveSource(TGNString const & Function,TGNString const & Preferred,TGNString const & Revision)
   {
    TGNString Request;auto Result=FSourceClaims.Resolve(Function,Preferred,Revision,GnMillis64(),Request);
    if(!Request.empty()&&FControlReady)FControl.Send(Request,{});
    return Result;
   }
  TGnHeatPiPeers HeatPiPeers;
  bool GetTemperature( TGNString const & Id, float & Celsius ) const;
  bool GetMeasurement( TGNString const & Id, TGNString const & Field, float & Value ) const;
  TGnChargeLearningPolicy ChargeLearningPolicy( TGNString const & Id ) const;
  TGnCoopTimer_t TemperatureAgeMs( TGNString const & Id ) const;
  TGnCoopTimer_t MeasurementAgeMs( TGNString const & Id, TGNString const & Field ) const;

  void RxMsg_Request( TGnWireValue Root, TGNString const & Payload, TGNString const & Address, uint16_t Port );
  void RxMsg_Status( TGnWireValue Root, TGNString const & Payload, TGNString const & Address, uint16_t Port );
  void RxMsg_InfoRequest( TGnWireValue Root, TGNString const & Payload, TGNString const & Address, uint16_t Port );
  void RxMsg_Info( TGnWireValue Root, TGNString const & Payload, TGNString const & Address, uint16_t Port );
  void RxMsg_SourceRequest( TGnWireValue Root, TGNString const & Payload, TGNString const & Address, uint16_t Port );
  void RxMsg_SourceInfo( TGnWireValue Root, TGNString const & Payload, TGNString const & Address, uint16_t Port );

 private:
  bool NetworkConnected(void) const;
  bool OpenSocket(void);
  void ReceivePackets(void);
  bool SendStatus(void);
  bool SendStatus( TGNString const & Status, uint64_t Signature,
                   TGnUdpTemperature const & Temperature );
  bool SendRaw( TGNString const & Payload );
  void HandlePacket( TGNString const & Payload, TGNString const & Address = {}, uint16_t Port = GnUdpStatusPort );
  void ReceiveControl(void);
  void AnnounceSources(void);
  void CacheTemperature( TGNString const & Id, bool Valid, float Celsius, TGNString const & Field );
  bool TemperatureChanged( TGnUdpTemperature const & Current ) const;
  void RememberTemperature( TGnUdpTemperature const & Current );
  void NetworkCheckTimerCb( TGnCoopTimerBase * Tmr, uint16_t IntervalsElapsed );
  void StatusCheckTimerCb( TGnCoopTimerBase * Tmr, uint16_t IntervalsElapsed );
  void PublishTimerCb( TGnCoopTimerBase * Tmr, uint16_t IntervalsElapsed );

 private:
  TGnUdpControl FControl;
  TGnUdpDescriptions FDescriptions;
  TGnUdpSources FSourceClaims;
  TGnUdpInfoParts FInfoParts;
  TGNString FSourceSignature;
  std::map<TGNString,uint64_t> FInfoReplyTimes;
  TGNString FWatchedId, FWatchedStatus;
  uint64_t FWatchedRevision = 0;
  uint64_t FWatchedReceivedAt=0;
  TGNString FWatchedCompact;
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
  bool FControlReady = false;

#if defined( GNPlatformESP ) && !defined( ESP32 )
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
#if defined( GNPlatformESP ) && !defined( ESP32 )
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
  GnDeviceIdentity.SourceResolver([this](TGNString const & Function,TGNString const & Preferred,TGNString const & Revision)
   {return ResolveSource(Function,Preferred,Revision);});
  FId = std::move(Id);
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
  if( !FReady || Payload.empty() )
    return false;
#if defined( GNPlatformESP )
  if( WiFi.status() != WL_CONNECTED )
    return false;
  IPAddress Broadcast;
  if(!Broadcast.fromString(AquariumTransport::BroadcastAddress.c_str()))return false;
#if defined( ESP32 )
  // Arduino NetworkUDP splits TX and truncates RX at 1460 bytes. Preserve the
  // complete bounded status datagram through the native lwIP socket instead.
  sockaddr_in Destination{};
  Destination.sin_family = AF_INET;
  Destination.sin_port = htons(GnUdpStatusPort);
  Destination.sin_addr.s_addr = inet_addr(Broadcast.toString().c_str());
  return sendto(FSocket,Payload.data(),Payload.size(),0,reinterpret_cast<sockaddr *>(&Destination),sizeof(Destination)) == static_cast<ssize_t>(Payload.size());
#else
  if( !FUdp.beginPacket( Broadcast, GnUdpStatusPort ) )
    return false;
  const size_t Written = FUdp.write( reinterpret_cast<uint8_t const *>( Payload.data() ), Payload.length() );
  return Written == Payload.length() && FUdp.endPacket() == 1;
#endif
#else
  sockaddr_in Destination{};
  Destination.sin_family=AF_INET;
  Destination.sin_port=htons(GnUdpStatusPort);
  if(inet_pton(AF_INET,AquariumTransport::BroadcastAddress.c_str(),&Destination.sin_addr)!=1)return false;
  return sendto(FSocket,Payload.data(),Payload.length(),0,
                reinterpret_cast<sockaddr *>(&Destination),sizeof(Destination))==static_cast<ssize_t>(Payload.length());
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
  return Current.Valid && Current.SampleRevision != FLastTemperature.SampleRevision;
 }

bool TGnUdpStatus::TImplementation::SendStatus( TGNString const & Status, uint64_t Signature,
                                                 TGnUdpTemperature const & Temperature )
 {
  Signature = StatusEventSignature( Status );
  const TGNString Packet = BuildStatusJson( GnDeviceIdentity.WireId(), Status, Temperature, &FInfoParts );
  if( Packet.empty() ) { FError = "Cannot encode UDP status"; return false; }
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

void TGnUdpStatus::TImplementation::HandlePacket( TGNString const & Payload, TGNString const & Address, uint16_t Port )
 {
  TGnWireDocument Document(Payload);auto Root=Document.Root();
  GnDispatchUdpMessage(Root,*this,Payload,Address,Port);
 }

void TGnUdpStatus::TImplementation::RxMsg_InfoRequest( TGnWireValue Root, TGNString const & Payload, TGNString const & Address, uint16_t Port )
 {
  (void)Root;(void)Payload;(void)Address;(void)Port;
  if(Address.empty()||Root.Get("ID").Text()!=FId||Root.Get("InfoRev").Text()!=GnUdpInfoHash(FInfoParts))return;
  const TGNString Key=Address+":"+std::to_string(Port);const auto Now=GnMillis64();
  auto Last=FInfoReplyTimes.find(Key);if(Last!=FInfoReplyTimes.end()&&Now-Last->second<1000)return;
  if(FInfoReplyTimes.size()>=16)FInfoReplyTimes.erase(FInfoReplyTimes.begin());
  FInfoReplyTimes[Key]=Now;
  std::vector<TGNString> Fields;for(auto Item:Root.Get("Fields").Items())Fields.push_back(Item.Text());
  for(auto const & Reply:GnUdpInfoReplies(FId,GnUdpInfoHash(FInfoParts),FInfoParts,Fields))FControl.Send(Reply,Address,Port);
 }

void TGnUdpStatus::TImplementation::RxMsg_Info( TGnWireValue Root, TGNString const & Payload, TGNString const & Address, uint16_t Port )
 {
  (void)Root;(void)Payload;(void)Address;(void)Port;
  if(FDescriptions.AcceptInfo(Root)&&!FWatchedCompact.empty())
   {
    TGNString Request;auto Expanded=FDescriptions.Expand(FWatchedCompact,GnMillis64(),Request,true);
    TGnWireDocument Frame(Expanded);auto Status=Frame.Root().Get("Status").Json();
    if(!Expanded.empty()&&Status!=FWatchedStatus){FWatchedStatus=Status;++FWatchedRevision;}
   }
 }

void TGnUdpStatus::TImplementation::RxMsg_SourceInfo( TGnWireValue Root, TGNString const & Payload, TGNString const & Address, uint16_t Port )
 {
  (void)Root;(void)Payload;(void)Address;(void)Port;
  FSourceClaims.Accept(Root);
 }

void TGnUdpStatus::TImplementation::RxMsg_SourceRequest( TGnWireValue Root, TGNString const & Payload, TGNString const & Address, uint16_t Port )
 {
  (void)Root;(void)Payload;(void)Address;(void)Port;
  const auto Revision=GnDeviceIdentity.ConfirmedSourceRevision(),Function=Root.Get("Func").Text();
  auto const & Sources=GnDeviceIdentity.ConfirmedSources();auto Source=Sources.find(Function);
  if(!Address.empty()&&!Revision.empty()&&Source!=Sources.end())
    FControl.Send(GnUdpSourceReply(FId,Function,Source->second,Revision),Address,Port);
 }

void TGnUdpStatus::TImplementation::RxMsg_Request( TGnWireValue Root, TGNString const & Payload, TGNString const & Address, uint16_t Port )
 {
  (void)Root;(void)Payload;(void)Address;(void)Port;
  TParsedPacket Packet;ParsePacket(Root,Packet);
  if(RequestMatches(Packet,GnDeviceIdentity.WireId()))SendStatus();
 }

void TGnUdpStatus::TImplementation::RxMsg_Status( TGnWireValue Root, TGNString const & Payload, TGNString const & Address, uint16_t Port )
 {
  (void)Root;(void)Payload;(void)Address;(void)Port;
  HeatPiPeers.AcceptStatus(Root,GnMillis64(),time(nullptr));
  TGNString Expanded;
  if(Root.Get("ID").Text()==FWatchedId && !Root.Get("Compact").Bool())FWatchedCompact.clear();
  TParsedPacket Packet;
  if(Root.Get("ID").Text()==FWatchedId && Root.Get("Compact").Bool())
   {
    FWatchedCompact=Payload;
    TGNString Request;Expanded=FDescriptions.Expand(Root,GnMillis64(),Request,true);
    if(!Request.empty())FControl.Send(Request,Address,GnUdpDescriptionPort(FWatchedId));
    if(Expanded.empty())return;
    TGnWireDocument Frame(Expanded);if(!ParsePacket(Frame.Root(),Packet))return;
   }
  else if(!ParsePacket(Root,Packet))return;
  if( Packet.Type == TPacketType::Status && !FWatchedId.empty() && Packet.Id == FWatchedId && !Packet.StatusText.empty() )
   { FWatchedStatus = Packet.StatusText; ++FWatchedRevision; FWatchedReceivedAt=GnMillis64(); }
  if( Packet.Type != TPacketType::Status || Packet.Id.empty() || Packet.Id == GnDeviceIdentity.WireId() ||
      !ValidTemperatureId( Packet.Id ) )
    return;
  // Each status is a complete snapshot: never retain an absent field.
  FTemperatures.erase( std::remove_if( FTemperatures.begin(), FTemperatures.end(),
    [&]( TTemperatureCache const & Entry ) { return Entry.Id == Packet.Id; } ), FTemperatures.end() );
  for( auto const & Measurement : Packet.Measurements )
    CacheTemperature( Packet.Id, Measurement.Valid, Measurement.Value, Measurement.Field );
  for( auto & Entry : FTemperatures )
    if( Entry.Id == Packet.Id ) Entry.Learning = Packet.Learning;

 }



void TGnUdpStatus::TImplementation::ReceivePackets(void)
 {
  if( !FReady )
    return;
#if defined( GNPlatformESP ) && !defined( ESP32 )
  while( true )
   {
    const int PacketSize = FUdp.parsePacket();
    if( PacketSize <= 0 )
      break;
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
      HandlePacket( TGNString( Buffer.get() ),FUdp.remoteIP().toString().c_str(),FUdp.remotePort() );
     }
   }
#else
  while( true )
   {
    sockaddr_in From{};
    TGNString Packet;
    if(!GnReadUdpDatagram(FSocket,Packet,From))return;
    if(Packet.empty())continue;
    char Ip[INET_ADDRSTRLEN];inet_ntop(AF_INET,&From.sin_addr,Ip,sizeof(Ip));
    HandlePacket( Packet,Ip,ntohs(From.sin_port) );
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

// Keep the last packet's policy through sender loss; it also governs the pending cycle.
TGnChargeLearningPolicy TGnUdpStatus::TImplementation::ChargeLearningPolicy( TGNString const & Id ) const
 {
  const auto Entry = std::find_if( FTemperatures.begin(), FTemperatures.end(), [&]( TTemperatureCache const & Item )
   { return Item.Id == Id; } );
  return Entry == FTemperatures.end() ? TGnChargeLearningPolicy{} : Entry->Learning;
 }

TGnCoopTimer_t TGnUdpStatus::TImplementation::MeasurementAgeMs( TGNString const & Id, TGNString const & Field ) const
 {
  const auto Entry = std::find_if( FTemperatures.begin(), FTemperatures.end(), [&]( TTemperatureCache const & Item )
   { return Item.Id == Id && Item.Field == Field; } );
  return Entry == FTemperatures.end() ? TGnCoopTimer_MAX : Entry->AgeTimer.Elapsed();
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
  if( !FReady )return;
  // ESP identity is initialized by time sync after UDP setup, before loop().
  if(!FControlReady)
   {
    const auto Id=GnDeviceIdentity.WireId();const auto Port=GnUdpDescriptionPort(Id);
    if(!Port)return;
    if(!FControl.Setup(Port)){FError="Cannot bind UDP description reply socket";return;}
    FId=Id;FControlReady=true;
   }
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

  ReceiveControl();
  ReceivePackets();
  AnnounceSources();

  if( FStatusCheckDue )
   {
    FStatusCheckDue = false;
    uint64_t Signature = 0;
    const TGNString Status = FStatusProvider( Signature );
    if( Status.empty() ) return;
    Signature = StatusEventSignature( Status );
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
    // A valid temperature source publishes at read completion, not on a second
    // independent timer which could send the previous sample just beforehand.
    const TGnUdpTemperature Temperature = FTemperatureProvider ? FTemperatureProvider() : TGnUdpTemperature{};
    if( !Temperature.Available || !Temperature.Valid ) SendStatus();
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

TGnCoopTimer_t TGnUdpStatus::MeasurementAgeMs( TGNString const & Id, TGNString const & Field ) const
 {
  return FImplementation->MeasurementAgeMs( Id, Field );
 }

TGnChargeLearningPolicy TGnUdpStatus::ChargeLearningPolicy( TGNString const & Id ) const
 {
  return FImplementation->ChargeLearningPolicy( Id );
 }

bool TGnUdpStatus::GetHeatPiPeer( TGNString const & Id, TGNString & Status )
 { return FImplementation->HeatPiPeers.Get(Id,GnMillis64(),Status); }
void TGnUdpStatus::WatchStatus( TGNString const & Id )
 { FImplementation->WatchStatus( Id ); }
bool TGnUdpStatus::GetWatchedStatus( TGNString const & Id, TGNString & Status, uint64_t & Revision ) const
 { return FImplementation->GetWatchedStatus( Id, Status, Revision ); }

void TGnUdpStatus::TImplementation::ReceiveControl(void)
 {
  TGNString Payload,Address;uint16_t Port;
  for(unsigned I=0;I<64&&FControl.Read(Payload,Address,Port);++I)
    if(!Payload.empty())HandlePacket(Payload,Address,Port);
 }
void TGnUdpStatus::TImplementation::AnnounceSources(void)
 {
  if(!FNetworkConnected)return;
  const auto Revision=GnDeviceIdentity.ConfirmedSourceRevision();if(Revision.empty())return;
  TGNString Signature=Revision;
  for(auto const & Source:GnDeviceIdentity.ConfirmedSources())Signature+=Source.first+Source.second;
  if(Signature==FSourceSignature)return;
  bool Sent=true;
  for(auto const & Source:GnDeviceIdentity.ConfirmedSources())
    Sent=FControl.Send(GnUdpSourceReply(FId,Source.first,Source.second,Revision),{})&&Sent;
  if(Sent)FSourceSignature=Signature;
 }
