// FeederStatusApi.cpp, Version: 2.69
#define Do_THttpDevice
#define Do_TUdpChannel
#define Do_TUdpMeasurement
#define Do_TUdpNextChange
#define Do_TUdpStateLabels
#define Do_TUdpStatusData
#define Do_TUdpStatusMessage
#define Do_TWsEventEndpoint
#define Do_TWsStatusEvent
#include <GnBasics.h>
#include <GnJsonSchema.h>
#include <DeviceUrl.h>
#include <GnDeviceIdentity.h>
#include <GnUdpChannelBits.h>
#include <GnUdpInfoParts.h>
#include "FeederStatusApi.h"

#include "FeederConfig.h"
#include "FeederDrive.h"
#include "FeederTemperature.h"
#include "FeederWifi.h"
#include "FeedTimes.h"
#include "Version.inc.h"
#include "DeviceType.inc.h"
#include <LittleFS.h>
#include <cstdio>
#include <ctime>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
 {
constexpr unsigned ApiVersion = 1;
constexpr unsigned WebSocketPort = 81;
constexpr unsigned PollingFallbackSeconds = 5;

constexpr size_t StatusBufferBytes = 1280;
enum class TStatusOutput { Full, Live, Info, Events };
enum class TStatusPart { Live, Info, Omit };

bool StatusFieldSelected( TStatusOutput Output, TStatusPart Part )
 {
  switch( Output )
   {
    case TStatusOutput::Live: return Part == TStatusPart::Live;
    case TStatusOutput::Info: return Part == TStatusPart::Info;
    default: return true;
   }
 }

template<size_t N> struct TContractText
 {
  char Text[N];
  constexpr TContractText( char const (& Value)[N] )
   { for( size_t I=0; I<N; ++I )Text[I]=Value[I]; }
  constexpr std::string_view View(void) const { return {Text,N-1}; }
 };

TGNString ReadDataVersion(void)
 {
  File FileHandle = LittleFS.open( "/Versions.txt", "r" );
  if( !FileHandle )
    return {};

  while( FileHandle.available() )
   {
    String Line = FileHandle.readStringUntil( '\n' );
    Line.trim();
    if( Line.startsWith( "LittleFS=" ) )
     {
      TGNString Version = Line.substring( 9 ).c_str();
      FileHandle.close();
      return Version;
     }
   }
  FileHandle.close();
  return {};
 }
 }

TFeederStatusApi::TFeederStatusApi(void)
 {
 }

TGNString TFeederStatusApi::DeviceJson(void) const
 {
  return BuildStatusJson( nullptr, nullptr, nullptr, nullptr, false, true );
 }

TGNString TFeederStatusApi::StatusJson( char const ** Failure, TStatusMemory * Memory ) const
 {
  return BuildStatusJson( nullptr, Failure, Memory );
 }

TGNString TFeederStatusApi::HttpStatusJson( bool Measure, int & Code ) const
 {
  Code = 200;
  const auto FreeHeap = ESP.getFreeHeap();
  char const * Failure = "status";
  TFeederStatusApi::TStatusMemory Memory;
  TGNString Status = StatusJson( &Failure, Measure ? &Memory : nullptr );
  if( Measure )
   {
    char Report[320];
    const auto Bytes = []( uint32_t Value ) { return static_cast<unsigned long>( Value ); };
    snprintf( Report, sizeof( Report ),
      "{\"Failure\":\"%s\",\"BufferBytes\":%lu,\"JsonBytes\":%lu,"
      "\"Entry\":[%lu,%lu],"
      "\"BeforeText\":[%lu,%lu],\"Text\":[%lu,%lu]}",
      Status.empty() ? Failure : "", Bytes( Memory.BufferBytes ), Bytes( Memory.JsonBytes ),
      Bytes( Memory.Entry.Free ), Bytes( Memory.Entry.Largest ),
      Bytes( Memory.BeforeText.Free ), Bytes( Memory.BeforeText.Largest ),
      Bytes( Memory.Text.Free ), Bytes( Memory.Text.Largest ) );
    return Report;
   }
  if( !Status.empty() )return Status;
  Code = 503;
  char Message[112];
  snprintf( Message, sizeof( Message ), "{\"Failure\":\"%s\",\"FreeHeap\":%lu}",
            Failure, static_cast<unsigned long>( FreeHeap ) );
  return Message;
 }

bool TFeederStatusApi::StatusSignature( uint64_t & Signature ) const
 {
  return !BuildStatusJson( &Signature ).empty();
 }

TGNString TFeederStatusApi::StatusEventJson( uint64_t Sequence ) const
 {
  return BuildStatusJson( nullptr, nullptr, nullptr, nullptr, false, false, &Sequence );
 }

bool TFeederStatusApi::CompactStatus( uint64_t & Signature, TGnUdpInfoParts * Parts, TGNString & Packet ) const
 {
  if( Parts )
   {
    Parts->clear();
    Packet = BuildStatusJson( nullptr, nullptr, nullptr, Parts );
    if( Packet.empty() )Parts->clear();
    return !Packet.empty();
   }
  return !BuildStatusJson( &Signature, nullptr, nullptr, nullptr, true ).empty();
 }

TGNString TFeederStatusApi::BuildStatusJson( uint64_t * signature, char const ** Failure,
                                           TStatusMemory * Memory, TGnUdpInfoParts * Parts, bool Events, bool Discovery, uint64_t const * Sequence ) const
 {
  const auto Capture = [&]( THeapSample TStatusMemory::* Phase )
   {
#if defined( GNPlatformESP8266 ) && defined( UMM_INFO )
    if( Memory )
     {
      auto & Sample = Memory->*Phase;
      ESP.getHeapStats( &Sample.Free, &Sample.Largest );
     }
#endif
   };
  Capture( &TStatusMemory::Entry );
  if( Memory )Memory->BufferBytes = StatusBufferBytes;
  const auto Fail = [&]( char const * Reason ) -> TGNString
   {
    if( Failure )*Failure = Reason;
    if( !FReportedStatusFailure )
     {
      THeapSample Heap;
#if defined( GNPlatformESP8266 ) && defined( UMM_INFO )
      ESP.getHeapStats( &Heap.Free, &Heap.Largest );
#endif
      Debug( "Feeder status: %s, free=%lu, largest=%lu\n", Reason,
             static_cast<unsigned long>(Heap.Free), static_cast<unsigned long>(Heap.Largest) );
      FReportedStatusFailure = true;
     }
    return {};
   };
  const std::time_t Now = std::time( nullptr );
  const bool TimeSynchronized = FeederWifi.TimeIsValid();
  const bool StationConnected = !(FeederConfig.WifiMode & WIFI_STA) || WiFi.status() == WL_CONNECTED;
  const char * const TemperatureError = FeederTemperature.Error();
  const char * const DriveError = FeederDrive.Error();
  const bool Healthy = FFileSystemMounted && StationConnected && TimeSynchronized &&
                       FWebSocketStarted && !*TemperatureError && !*DriveError && FSetupError.empty();
  std::time_t NextFeedTime = 0;
  uint8_t NextFeedPortions = 0;
  const bool HasNextFeed = TimeSynchronized && FeedTimes.GetNextFeed( NextFeedTime, NextFeedPortions );
  if( TimeSynchronized && FOnlineSince == 0 )
    FOnlineSince = Now - static_cast<time_t>(FStartedTimer.Elapsed() / 1000);

  Capture( &TStatusMemory::BeforeText );
  TGNString Result;
  const bool HashOnly = signature != nullptr;
  if( !HashOnly && !GnReserveStringExact( Result, Discovery ? 640 : StatusBufferBytes ) )
    return Fail( "status-output" );
  if( !HashOnly && !Discovery )
   {
    Result.resize( StatusBufferBytes );
    if( Result.size() != StatusBufferBytes )return Fail( "status-output" );
   }

  char * Write = Result.data();
  size_t Remaining = HashOnly || Discovery ? 1 : Result.size();
  uint64_t Hash = GnHash64OffsetBasis;
  char Last = '{';
  const auto Add = [&]( char const * Format, auto... Values )
   {
    if( !Remaining )return;
    char Scratch[96];
    char * const Target = HashOnly || Discovery ? Scratch : Write;
    const size_t Capacity = HashOnly || Discovery ? sizeof(Scratch) : Remaining;
    int Count;
    if constexpr( sizeof...(Values) == 0 )Count = std::snprintf( Target, Capacity, "%s", Format );
    else Count = std::snprintf( Target, Capacity, Format, Values... );
    if( Count < 0 || static_cast<size_t>(Count) >= Capacity )
     { Remaining = 0; return; }
    if( Count )Last = Target[Count-1];
    if( HashOnly )Hash = GnHash64(Target,Count,Hash);
    else if( Discovery )
     {
      const size_t Length = Result.size() + Count;
      Result.append(Target,Count);
      if( Result.size() != Length )Remaining = 0;
     }
    else
     {
      Write += Count;
      Remaining -= Count;
     }
   };
  const auto Encode = [&]( char const * Value )
   {
    Add( "\"" );
    for( ; Remaining && *Value; ++Value )
     {
      const auto Byte = static_cast<unsigned char>(*Value);
      if( Byte == '"' || Byte == '\\' )Add( "\\%c", Byte );
      else if( Byte < 0x20 )Add( "\\u%04x", static_cast<unsigned>(Byte) );
      else Add( "%c", Byte );
     }
    Add( "\"" );
   };
  const auto Key = [&]( std::string_view Name )
   {
    // A key after the opening brace starts an object. Other keys need a comma.
    if( Remaining && Last != '{' )Add( "," );
    Add( "\"%.*s\":", static_cast<int>(Name.size()), Name.data() );
   };
  const auto UInt = [&]( uint64_t Value )
   { Add( "%llu", static_cast<unsigned long long>(Value) ); };
  const auto JsonValue = []<class T>( std::type_identity<T>, auto WriteValue )
   { return std::pair{std::type_identity<T>{},WriteValue}; };
  struct TOmitted {};
  constexpr TOmitted Omitted;
  const auto Emit = [&]<class T, bool Optional, TContractText Rule>( char const * KeyName, auto const & Value )
   {
    using TValue = std::remove_cvref_t<decltype(Value)>;
    if constexpr( std::is_same_v<TValue,TOmitted> )
     {
      static_assert(Optional, "Required feeder status field has no value");
     }
    else
     {
      constexpr auto FieldRule = Rule.View();
      static_assert(FieldRule == "Any" || FieldRule == "HttpUrl" || FieldRule == "WebSocketUrl" ||
                    FieldRule == "All(Count(0,32),Each(ChannelAddress))" ||
                    (FieldRule == "NonEmpty" && std::is_same_v<T,TGNString> && !Optional) ||
                    ((FieldRule == "ChannelAddress" || FieldRule == "ServiceAddress") && std::is_same_v<T,TChannelId>) ||
                    (FieldRule == "RequiredIf(Compact,Hex16)" && std::is_same_v<T,TGNString>) ||
                    (FieldRule == "ItemsHaveIf(Compact,Chs,Id)" && std::is_same_v<T,TUdpStatusData>) ||
                    (FieldRule == "Count(0,64)" && requires { typename T::value_type; } &&
                     !std::is_same_v<T,TGNString>),
                    "Unsupported feeder status field rule");
      Key(KeyName);
      if constexpr( requires { Value.first; Value.second(); } )
       {
        static_assert(std::is_same_v<decltype(Value.first),std::type_identity<T>>,
                      "Feeder status writer type does not match its field");
        Value.second();
       }
      else if constexpr( std::is_same_v<TValue,std::nullptr_t> )
       {
        static_assert(Optional, "Required feeder status field cannot be null");
        Add("null");
       }
      else if constexpr( std::is_same_v<TValue,char const *> )
       {
        static_assert(std::is_same_v<T,TGNString>, "Feeder status text field expected");
        if( Value )Encode(Value); else Add("null");
       }
      else
       {
        const auto WriteScalar = [&]( T Scalar )
         {
          if constexpr( std::is_same_v<T,bool> )Add(Scalar ? "true" : "false");
          else if constexpr( std::is_unsigned_v<T> || std::is_enum_v<T> )
            UInt(static_cast<uint64_t>(Scalar));
          else if constexpr( std::is_same_v<T,double> )
           {
            const auto Digits = static_cast<unsigned long>(std::lround(std::abs(Scalar)*10000));
            Add("%s%lu.%04lu",Scalar < 0 ? "-" : "",Digits/10000,Digits%10000);
           }
          else static_assert(std::is_same_v<T,void>,
                             "Unsupported feeder status value type");
         };
        if constexpr( std::is_same_v<TValue,std::optional<T>> )
         {
          static_assert(Optional, "Required feeder status field cannot be null");
          if( Value )WriteScalar(*Value); else Add("null");
         }
        else
         {
          static_assert(std::is_same_v<T,TValue>, "Feeder status value type does not match its field");
          WriteScalar(Value);
         }
       }
     }
   };

  // The contract supplies the JSON key and directly selects the same-named value.
#define ArrayOf(Type) std::vector<Type>
#define MapOf(Type) std::map<TGNString,Type>
#define JD_OPTIONAL_Optional true
#define JD_OPTIONAL_Required false
#define JD(Type, Name, Presence, Rule, Description, Part) JD_##Part(Type, Name, Presence, Rule, Description)
#define JD_Live(Type, Name, Presence, Rule, Description) \
  if( Select(TStatusPart::Live, #Name) )Emit.template operator()<Type, JD_OPTIONAL_##Presence, TContractText{#Rule}>(#Name, Name);
#define JD_Info(Type, Name, Presence, Rule, Description) \
  if( Select(TStatusPart::Info, #Name) )Emit.template operator()<Type, JD_OPTIONAL_##Presence, TContractText{#Rule}>(#Name, Name);
#define JD_Omit(Type, Name, Presence, Rule, Description) \
  if( Select(TStatusPart::Omit, #Name) )Emit.template operator()<Type, JD_OPTIONAL_##Presence, TContractText{#Rule}>(#Name, Name);
#define JD_CONST(Type, Name, Value) \
  Emit.template operator()<Type, false, TContractText{"Any"}>(#Name, Type(Value));
#define JD_BASE(Name, Presence) static_assert(false, "Unsupported feeder status base");
#define JD_VALUE(Name, Type, Rule, Description) static_assert(false, "Unsupported feeder status value");
#define JD_TUPLE_BEGIN(Name, Description) static_assert(false, "Unsupported feeder status tuple");

  const auto SelectAll = [](TStatusPart, std::string_view) { return true; };
  const auto WriteLabels = [&]()
   {
    const auto & Select = SelectAll;
    char const * const True = "FeederRunning";
    char const * const False = "FeederReady";
    Add("{");
#define Do_TUdpStateLabels
#include <Definitions/Contract/TUdpStateLabels.inc.h>
#undef Do_TUdpStateLabels
    Add("}");
   };
  const auto WriteNextFeed = [&]()
   {
    if( !HasNextFeed ) { Add("null"); return; }
    const auto & Select = SelectAll;
    const TUnixTime Time = NextFeedTime;
    const bool State = true;
    const auto Src = TNextChangeSource::FeedTime;
    const uint32_t Portions = NextFeedPortions;
    Add("{");
#define Do_TUdpNextChange
#include <Definitions/Contract/TUdpNextChange.inc.h>
#undef Do_TUdpNextChange
    Add("}");
   };
  const auto WriteChannel = [&]( TStatusOutput Output )
   {
    const auto Select = [&]( TStatusPart Part, std::string_view Name )
     {
      if( !StatusFieldSelected(Output,Part) )return false;
      return Name != "Bits" || Output == TStatusOutput::Live;
     };
    const auto Id = JsonValue(std::type_identity<TChannelId>{},[&]()
     { Add("\"%s/0\"",GnDeviceIdentity.DeviceId().c_str()); });
    const bool State = FeederDrive.Running();
    char const * const Name = "Fütterung";
    const uint32_t HwCh = 0;
    const bool HasIn = FeederDrive.HasInput();
    const bool HasOut = true;
    const auto InState = HasIn ? std::optional<bool>(FeederDrive.InputState()) : std::nullopt;
    const std::optional<bool> AutomaticState;
    const bool OvrActive = false;
    const std::optional<bool> OvrState;
    const auto StateLabels = JsonValue(std::type_identity<TUdpStateLabels>{},WriteLabels);
    const auto NextChange = JsonValue(std::type_identity<TUdpNextChange>{},WriteNextFeed);
    char const * const Err = *DriveError ? DriveError : nullptr;
    TGnUdpChannelBits ChannelBits{};
    auto GetBit=[&](auto const & Value)
     {
      using TValue=std::remove_cvref_t<decltype(Value)>;
      if constexpr(std::is_same_v<TValue,TOmitted>) { return std::pair<bool,bool>{false,false}; }
      else if constexpr(std::is_same_v<TValue,bool>) { return std::pair<bool,bool>{Value,true}; }
      else if constexpr(requires{Value.has_value();*Value;})
       { const bool Mask=Value.has_value();return std::pair<bool,bool>{Mask&&*Value,Mask}; }
      else { return std::pair<bool,bool>{false,false}; }
     };
    constexpr auto CalOvrActive=Omitted;
#undef JD_Omit
#define JD_Omit(Type, Member, Presence, Rule, Description) \
    static_assert(std::is_same_v<Type,bool>, "Compact channel bits must describe bool fields"); \
    { const auto Pair=GetBit(Member);ChannelBits.Bit.Member=Pair.first;ChannelBits.Bit.Member ## Msk=Pair.second; }
#define Do_TUdpChannel
#include <Definitions/Contract/TUdpChannel.inc.h>
#undef Do_TUdpChannel
#undef JD_Omit
#define JD_Omit(Type, Name, Presence, Rule, Description) \
  if( Select(TStatusPart::Omit, #Name) )Emit.template operator()<Type, JD_OPTIONAL_##Presence, TContractText{#Rule}>(#Name, Name);
    const uint16_t Bits = ChannelBits.All;
    Add(Output == TStatusOutput::Info ? "{" : "[{");
#define Do_TUdpChannel
#include <Definitions/Contract/TUdpChannel.inc.h>
#undef Do_TUdpChannel
    Add(Output == TStatusOutput::Info ? "}" : "}]");
   };
  const auto WriteMeasurement = [&]( TStatusOutput Output )
   {
    const auto Select = [&]( TStatusPart Part, std::string_view Name )
     {
      return StatusFieldSelected(Output,Part) &&
             (Output != TStatusOutput::Events || (Name != "Value" && Name != "UpdatedAt"));
     };
    if( !FeederTemperature.Enabled() ) { Add("[]"); return; }
    const double Temperature = FeederTemperature.Value();
    const bool Valid = FeederTemperature.Valid() && std::isfinite(Temperature);
    const auto Value = Valid ? std::optional<double>(Temperature) : std::nullopt;
    char const * const Kind = "Temperature";
    char const * const Name = "Temperatur";
    char const * const Unit = FeederTemperature.Unit();
    const uint8_t Decimals = 1;
    const auto SampleTime = FeederTemperature.UpdatedAt();
    const auto UpdatedAt = SampleTime > 0 ? std::optional<TUnixTime>(SampleTime) : std::nullopt;
    char const * const Err = *TemperatureError ? TemperatureError : nullptr;
    Add(Output == TStatusOutput::Info ? "{" : "[{");
#define Do_TUdpMeasurement
#include <Definitions/Contract/TUdpMeasurement.inc.h>
#undef Do_TUdpMeasurement
    Add(Output == TStatusOutput::Info ? "}" : "}]");
   };

  const auto DevId = JsonValue(std::type_identity<TChannelId>{},[&]()
   { Encode(GnDeviceIdentity.DeviceId().c_str()); });
  const auto WriteIdentity = [&]( bool InfoOnly )
   {
    const auto Select = [&]( TStatusPart Part, std::string_view )
     { return !InfoOnly || Part == TStatusPart::Info; };
    char const * const Func = GnDeviceIdentity.Function().c_str();
    char const * const Rev = GnDeviceIdentity.Revision().c_str();
    char const * const AppliedRev = GnDeviceIdentity.AppliedRevision().c_str();
    const auto ServiceId = DevId;
    const bool Prepared = GnDeviceIdentity.Prepared();
    const bool LocalSrcs = GnDeviceIdentity.LocalSources();
    const uint64_t BootCnt = GnDeviceIdentity.BootCounter();
    const auto Srcs = JsonValue(std::type_identity<std::map<TGNString,TChannelId>>{},[&]()
     {
      Add("{");
      for( auto const & [Name, Address] : GnDeviceIdentity.ConfirmedSources() )
       {
        if( Last != '{' )Add(",");
        Encode(Name.c_str());
        Add(":");
        Encode(Address.c_str());
       }
      Add("}");
     });
    Add("{");
#define Do_TJsonIdentity
#include <Definitions/Contract/TJsonIdentity.inc.h>
#undef Do_TJsonIdentity
    Add("}");
   };
  const auto Identity = JsonValue(std::type_identity<TJsonIdentity>{},[&]() { WriteIdentity(false); });
  const uint64_t OnlineSince = FOnlineSince;
  const auto CurrentTime = JsonValue(std::type_identity<TUnixTime>{},[&]()
   {
    if( HashOnly )return;
    UInt(static_cast<TUnixTime>(Now));
   });
  char const * const TimeSyncErr = TimeSynchronized ? nullptr : "System time is not synchronized";
  constexpr auto LastErr = nullptr;
  const auto Errs = JsonValue(std::type_identity<std::vector<TGNString>>{},[&]()
   {
    Add("[");
    bool First = true;
    const auto Error = [&]( bool Present, char const * Value )
     {
      if( !Present )return;
      if( !First )Add(",");
      Encode(Value);
      First = false;
     };
    Error(!FFileSystemMounted,"LittleFS is not mounted");
    Error(!StationConnected,"WiFi station is not connected");
    Error(!TimeSynchronized,"System time is not synchronized");
    Error(!FWebSocketStarted,"WebSocket server is not running");
    Error(*TemperatureError,TemperatureError);
    Error(*DriveError,DriveError);
    Error(!FSetupError.empty(),FSetupError.c_str());
    Add("]");
   });
  const auto WriteStatus = [&]( TStatusOutput Output )
   {
    const auto Select = [&]( TStatusPart Part, std::string_view Name )
     {
      if( !StatusFieldSelected(Output,Part) )return false;
      if( Name == "BootCnt" )return Output == TStatusOutput::Live;
      if( Name == "Temp" )return Output == TStatusOutput::Live && FeederTemperature.Enabled();
      return !Events || Name != "CurrentTime";
     };
    const auto Chs = JsonValue(std::type_identity<std::vector<TUdpChannel>>{},[&]() { WriteChannel(Output); });
    const auto Meas = JsonValue(std::type_identity<std::vector<TUdpMeasurement>>{},[&]() { WriteMeasurement(Output); });
    const uint64_t BootCnt = GnDeviceIdentity.BootCounter();
    const double Celsius = FeederTemperature.TemperatureCelsius();
    const auto Temp = FeederTemperature.Valid() && std::isfinite(Celsius)
                    ? std::optional<double>(Celsius) : std::nullopt;
    constexpr auto Kind = Omitted, PowerMeter = Omitted,
    BatteryPct = Omitted, AcOnline = Omitted, BatteryState = Omitted, ChargeLearnAllowed = Omitted,
    HeatControl = Omitted, HeatDiag = Omitted, ChargeLearn = Omitted, AppVer = Omitted,
    FeedbackMonitors = Omitted;
  Add("{");
#define Do_TUdpStatusData
#include <Definitions/Contract/TUdpStatusData.inc.h>
#undef Do_TUdpStatusData
    Add("}");
   };

  if( Discovery )
   {
    const TGNString DataVersion = ReadDataVersion();
    const auto Select = [&]( TStatusPart, std::string_view Field )
     { return Field != "DataVer" || !DataVersion.empty(); };
    const uint32_t ApiVer = ApiVersion;
    char const * const DevType = ESP_UPDATE_DEVICE_TYPE;
    char const * const Name = GnDeviceIdentity.Function().c_str();
    char const * const FirmwareVer = ProjectVersion::FirmwareRevision;
    char const * const DataVer = DataVersion.c_str();
    const auto ConfigUrl = DeviceUrl("http",80,"/Index.html");
    char const * const CfgUrl = ConfigUrl.c_str();
    const auto Caps = JsonValue(std::type_identity<std::vector<TGNString>>{},[&]()
     {
      Add("[");
      bool First = true;
      const auto Capability = [&]( char const * Value )
       {
        if( !First )Add(",");
        First = false;
        Encode(Value);
       };
      for( char const * Value : { "Status", "NextChange", "ErrorStatus", "Measurements",
                                  "WebSocket", "PollingFallback", "ConfigPage" } )Capability(Value);
      for( auto const & Value : FeederDrive.Capabilities() )Capability(Value.c_str());
      Add("]");
     });
    const auto Events = JsonValue(std::type_identity<TWsEventEndpoint>{},[&]()
     {
      const auto EventUrl = DeviceUrl("ws",WebSocketPort,"/api/v1/events");
      char const * const Url = EventUrl.c_str();
      const uint32_t PollingFallbackSec = PollingFallbackSeconds;
      Add("{");
#define Do_TWsEventEndpoint
#include <Definitions/Contract/TWsEventEndpoint.inc.h>
#undef Do_TWsEventEndpoint
      Add("}");
     });
    constexpr auto Platform = Omitted, RescueTargetVer = Omitted, EndpointAssignments = Omitted,
                   ButtonPress = Omitted, StatusClientUrl = Omitted;
    Add("{");
#define Do_THttpDevice
#include <Definitions/Contract/THttpDevice.inc.h>
#undef Do_THttpDevice
    Add("}");
   }
  else if( Parts )
   {
    const auto SavePart = [&]( TGNString KeyName )
     {
      if( !Remaining )return false;
      Parts->emplace(std::move(KeyName),TGNString(Result.data(),Write - Result.data()));
      Last = '{';
      Write = Result.data();
      Remaining = Result.size();
      return true;
     };
    WriteStatus(TStatusOutput::Info);
    if( !SavePart("Root") )return Fail("status-buffer");
    WriteIdentity(true);
    if( !SavePart("Identity") )return Fail("status-buffer");
    WriteChannel(TStatusOutput::Info);
    if( !SavePart("Channel:" + GnDeviceIdentity.DeviceId() + "/0") )return Fail("status-buffer");
    if( FeederTemperature.Enabled() )
     {
      WriteMeasurement(TStatusOutput::Info);
      if( !SavePart("Measurement:Temperature") )return Fail("status-buffer");
     }
    const auto & Select = SelectAll;
    const auto ID = JsonValue(std::type_identity<TChannelId>{},[&]() { Encode(GnDeviceIdentity.DeviceId().c_str()); });
    const bool Compact = true;
    const TGNString Revision = GnUdpInfoHash(*Parts);
    char const * const InfoRev = Revision.c_str();
    const auto Status = JsonValue(std::type_identity<TUdpStatusData>{},[&]() { WriteStatus(TStatusOutput::Live); });
    Add("{");
#undef JD_CONST
#define JD_CONST(Type, Name, Value) Emit.template operator()<Type, false, TContractText{"Any"}>(#Name, Type(Value));
#define Do_TUdpStatusMessage
#include <Definitions/Contract/TUdpStatusMessage.inc.h>
#undef Do_TUdpStatusMessage
    Add("}");
   }
  else if( Sequence )
   {
    const auto & Select = SelectAll;
    const uint64_t Seq = *Sequence;
    const uint64_t Timestamp = static_cast<uint64_t>(Now);
    const auto Status = JsonValue(std::type_identity<TUdpStatusData>{},[&]() { WriteStatus(TStatusOutput::Full); });
    Add("{");
#define Do_TWsStatusEvent
#include <Definitions/Contract/TWsStatusEvent.inc.h>
#undef Do_TWsStatusEvent
    Add("}");
   }
  else WriteStatus(Events ? TStatusOutput::Events : TStatusOutput::Full);

#undef MapOf
#undef ArrayOf
#undef JD_OPTIONAL_Optional
#undef JD_OPTIONAL_Required
#undef JD
#undef JD_Live
#undef JD_Info
#undef JD_Omit
#undef JD_BEGIN
#undef JD_END
#undef JD_CONST
#undef JD_BIT
#undef JD_BASE
#undef JD_VALUE
#undef JD_TUPLE_BEGIN
#undef JD_MESSAGE
  if( Remaining == 0 )return Fail( "status-buffer" );
  if( HashOnly )
   {
    *signature = Hash;
    FReportedStatusFailure = false;
    return "1";
   }
  if( !Discovery )Result.resize( static_cast<size_t>(Write - Result.data()) );
  Capture( &TStatusMemory::Text );
  if( Memory )Memory->JsonBytes = Result.length();
  FReportedStatusFailure = false;
  return Result;
 }
