// GnUdpStatusWriter.cpp, Version: 1.08
// Compact UDP status is written directly from the shared message definitions.

#define Do_TUdpRequestMessage
#define Do_TUdpInfoRequestMessage
#define Do_TUdpInfoMessage
#define Do_TUdpSourceRequestMessage
#define Do_TUdpSourceInfoMessage
#define Do_TUdpStatusData
#define Do_TUdpStatusMessage
#define Do_TJsonIdentity
#define Do_TUdpChannel
#define Do_TUdpNextChange
#define Do_TUdpStateLabels
#define Do_TUdpPowerMeter
#define Do_TUdpHeatControl
#define Do_TUdpHeatDiagnostics
#define Do_TJsonHeatConfig
#define Do_TUdpChargeLearning
#define Do_TUdpLastError
#define Do_TUdpMeasurement
#define Do_TJsonFeedbackState

#include <GnBasics.h>
#include <GnJsonSchema.h>
#include <GnUdpChannelBits.h>
#include <GnUdpInfoParts.h>
#include "GnUdpStatusWriter.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

namespace
 {
constexpr size_t StatusBufferBytes = 2048;

enum class TStatusPart { Live, Info, Omit };

class TCompactStatusWriter;
void WriteFeedbackState( TCompactStatusWriter & Writer, TJsonFeedbackState const & Object );

class TCompactStatusWriter
 {
 public:
  explicit TCompactStatusWriter( bool HashOnly ) : FHashOnly( HashOnly )
   {
   }

  bool HashOnly(void) const { return FHashOnly; }
  uint64_t Hash(void) const { return FHash; }
  bool Failed(void) const { return FFailed; }

  template<class... T> void Add( char const * Format, T... Values )
   {
    char Scratch[192];
    char * Target = FHashOnly ? Scratch : FBuffer.data()+FPosition;
    const size_t Capacity = FHashOnly ? sizeof(Scratch) : FBuffer.size()-FPosition;
    if( !Capacity ) { FFailed=true; return; }
    const int Count = [&]()
     {
      if constexpr( sizeof...(Values) == 0 )return std::snprintf( Target, Capacity, "%s", Format );
      else return std::snprintf( Target, Capacity, Format, Values... );
     }();
    if( Count < 0 || static_cast<size_t>(Count) >= Capacity ) { FFailed=true; return; }
    AppendRaw( Target, static_cast<size_t>(Count) );
   }

  void BeginObject(void)
   {
    PrepareValue();
    Add( "{" );
    if( FDepth >= FLast.size() ) { FFailed=true; return; }
    FLast[FDepth++] = false;
   }

  void EndObject(void)
   {
    if( !FDepth || FValuePending ) { FFailed=true; return; }
    --FDepth;
    Add( "}" );
   }

  void BeginArray(void)
   {
    PrepareValue();
    Add( "[" );
    if( FDepth >= FLast.size() ) { FFailed=true; return; }
    FLast[FDepth++] = false;
   }

  void EndArray(void)
   {
    if( !FDepth || FValuePending ) { FFailed=true; return; }
    --FDepth;
    Add( "]" );
   }

  void BeforeValue(void)
   {
    if( !FDepth )return;
    if( FLast[FDepth-1] )Add( "," );
    FLast[FDepth-1] = true;
   }

  void Key( char const * Name )
   {
    if( !FDepth || FValuePending ) { FFailed=true; return; }
    BeforeValue();
    WriteString( Name );
    Add( ":" );
    FValuePending = true;
   }

  void String( char const * Value )
   {
    PrepareValue();
    WriteString( Value );
   }

  void String( TGNString const & Value ) { String( Value.c_str() ); }
  void Null(void) { PrepareValue(); Add( "null" ); }

  template<class T> void Scalar( T Value )
   {
    PrepareValue();
    if constexpr( std::is_same_v<T,bool> )Add( Value ? "true" : "false" );
    else if constexpr( std::is_same_v<T,double> || std::is_same_v<T,float> )
     {
      if( !std::isfinite( static_cast<double>(Value) ) ) { FFailed=true; return; }
      Add( "%.15g", static_cast<double>(Value) );
     }
    else if constexpr( std::is_enum_v<T> )Add( "%u", static_cast<unsigned>(Value) );
    else if constexpr( std::is_integral_v<T> && std::is_signed_v<T> )Add( "%lld", static_cast<long long>(Value) );
    else if constexpr( std::is_integral_v<T> )Add( "%llu", static_cast<unsigned long long>(Value) );
    else static_assert( std::is_same_v<T,void>, "Unsupported compact status scalar" );
   }

  template<class T> void Value( std::vector<T> const & Items )
   {
    BeginArray();
    for( auto const & Item : Items )Value( Item );
    EndArray();
   }

  template<class T> void Value( std::map<TGNString,T> const & Items )
   {
    BeginObject();
    for( auto const & [Name,Item] : Items )
     {
      Key( Name.c_str() );
      Value( Item );
     }
    EndObject();
   }

  template<class T> void Value( T const & Item )
   {
    if constexpr( std::is_same_v<T,TGNString> )String( Item );
    else if constexpr( std::is_same_v<T,char const *> )String( Item );
    else if constexpr( std::is_same_v<T,TJsonFeedbackState> )WriteFeedbackState( *this, Item );
    else Scalar( Item );
   }

  TGNString CopyOutput(void) const
   { return FHashOnly ? TGNString{} : TGNString(FBuffer.data(),FPosition); }

  void ResetOutput(void)
   {
    if( FHashOnly )return;
    FPosition=0;
    FDepth=0;
    FValuePending=false;
    FLast.fill(false);
    FFailed=false;
   }

  void BeginHash(void)
   {
    ResetOutput();
    FHash=GnHash64OffsetBasis;
    FHashOnly=true;
   }

  TGNString Take(void)
   {
    if( FHashOnly || FFailed || FDepth || FValuePending )return {};
    return TGNString(FBuffer.data(),FPosition);
   }

 private:
  void PrepareValue(void)
   {
    if( FValuePending )FValuePending = false;
    else BeforeValue();
   }

  void WriteString( char const * Value )
   {
    Add( "\"" );
    if( Value )
     for( auto P=reinterpret_cast<unsigned char const *>(Value); *P; ++P )
      {
       if( *P=='\"' || *P=='\\' )Add( "\\%c", *P );
       else if( *P<0x20 )Add( "\\u%04x", static_cast<unsigned>(*P) );
       else Add( "%c", *P );
      }
    Add( "\"" );
   }

  void AppendRaw( char const * Text, size_t Length )
   {
    if( FFailed )return;
    if( FHashOnly )FHash=GnHash64( Text, Length, FHash );
    else
     {
      if( Length > FBuffer.size()-FPosition ) { FFailed=true; return; }
      std::memcpy( FBuffer.data()+FPosition, Text, Length );
      FPosition+=Length;
     }
   }

  std::array<char,StatusBufferBytes> FBuffer{};
  std::array<bool,16> FLast{};
  size_t FPosition=0;
  size_t FDepth=0;
  uint64_t FHash=GnHash64OffsetBasis;
  bool FHashOnly=false;
  bool FFailed=false;
  bool FValuePending=false;
 };

template<class T, class F> auto JsonValue( std::type_identity<T> Type, F Write )
 { return std::pair{Type,std::move(Write)}; }

template<class TValue>
void EmitField( TCompactStatusWriter & Writer, char const * Name, TValue const & Value )
 {
  if constexpr( requires { Value.first; Value.second(); } )
   {
    Writer.Key( Name );
    Value.second();
   }
  else if constexpr( requires { Value.Present; Value.IsNull; Value.Value; } )
   {
    if( !Value.Present )return;
    Writer.Key( Name );
    if( Value.IsNull )Writer.Null();
    else Writer.Value( Value.Value );
   }
  else
   {
    Writer.Key( Name );
    Writer.Value( Value );
   }
 }

#define ArrayOf(Type) std::vector<Type>
#define MapOf(Type) std::map<TGNString,Type>
#undef JD_CONST
#undef JD_BASE
#undef JD_VALUE
#undef JD_TUPLE_BEGIN
#define JD(Type, Name, Presence, Rule, Description, Part) JD_##Part(Name, Presence)
#define JD_Live(Name, Presence) \
  if( Select( TStatusPart::Live, #Name ) )Emit( #Name, Name );
#define JD_Info(Name, Presence) \
  if( Select( TStatusPart::Info, #Name ) )Emit( #Name, Name );
#define JD_Omit(Name, Presence) \
  if( Select( TStatusPart::Omit, #Name ) )Emit( #Name, Name );
#define JD_CONST(Type, Name, Value) Emit( #Name, Type(Value) );
#define JD_BASE(Name, Presence) static_assert(false, "Unsupported compact status base");
#define JD_VALUE(Name, Type, Rule, Description) static_assert(false, "Unsupported compact status value");
#define JD_TUPLE_BEGIN(Name, Description) static_assert(false, "Unsupported compact status tuple");

void WriteFeedbackState( TCompactStatusWriter & Writer, TJsonFeedbackState const & Object )
 {
  const auto Select=[](TStatusPart,std::string_view){return true;};
  const auto & Id=Object.Id;const auto & SwitchFunc=Object.SwitchFunc;const auto & Src=Object.Src;
  const auto & State=Object.State;const auto & Reason=Object.Reason;const auto & SwitchState=Object.SwitchState;
  const auto & FeedbackState=Object.FeedbackState;const auto & GraceSec=Object.GraceSec;
  auto Emit=[&](char const * Name,auto const & Value){EmitField(Writer,Name,Value);};
  Writer.BeginObject();
#define Do_TJsonFeedbackState
#include <Definitions/Contract/TJsonFeedbackState.inc.h>
#undef Do_TJsonFeedbackState
  Writer.EndObject();
 }

void WriteNextChange( TCompactStatusWriter & Writer, TUdpNextChange const & Object )
 {
  const auto Select=[](TStatusPart,std::string_view){return true;};
  const auto & Time=Object.Time;const auto & State=Object.State;const auto & Src=Object.Src;const auto & Portions=Object.Portions;
  auto Emit=[&](char const * Name,auto const & Value){EmitField(Writer,Name,Value);};
  Writer.BeginObject();
#define Do_TUdpNextChange
#include <Definitions/Contract/TUdpNextChange.inc.h>
#undef Do_TUdpNextChange
  Writer.EndObject();
 }

void WriteStateLabels( TCompactStatusWriter & Writer, TUdpStateLabels const & Object )
 {
  const auto Select=[](TStatusPart,std::string_view){return true;};
  const auto & True=Object.True;const auto & False=Object.False;
  auto Emit=[&](char const * Name,auto const & Value){EmitField(Writer,Name,Value);};
  Writer.BeginObject();
#define Do_TUdpStateLabels
#include <Definitions/Contract/TUdpStateLabels.inc.h>
#undef Do_TUdpStateLabels
  Writer.EndObject();
 }

void WriteIdentityInfo( TCompactStatusWriter & Writer, TJsonIdentity const & Object )
 {
  const auto Select=[](TStatusPart Part,std::string_view)
   {return Part==TStatusPart::Info;};
  const auto & Func=Object.Func;const auto & Rev=Object.Rev;
  const auto & AppliedRev=Object.AppliedRev;const auto & ServiceId=Object.ServiceId;
  const auto & Prepared=Object.Prepared;const auto & LocalSrcs=Object.LocalSrcs;
  const auto & BootCnt=Object.BootCnt;const auto & Srcs=Object.Srcs;
  auto Emit=[&](char const * Name,auto const & Value){EmitField(Writer,Name,Value);};
  Writer.BeginObject();
#define Do_TJsonIdentity
#include <Definitions/Contract/TJsonIdentity.inc.h>
#undef Do_TJsonIdentity
  Writer.EndObject();
 }

void WriteChannel( TCompactStatusWriter & Writer, TUdpChannel const & Object, bool Info )
 {
  TGnUdpChannelBits ChannelBits{};
  auto GetBit=[](auto const & Value)
   {
    if constexpr(requires{Value.Present;Value.IsNull;Value.Value;})
     {
      static_assert(std::is_same_v<std::remove_cv_t<decltype(Value.Value)>,bool>,"JD_BIT channel members must be bool");
      const bool Mask=Value.Present&&!Value.IsNull;return std::pair<bool,bool>{Mask&&Value.Value,Mask};
     }
    else if constexpr(requires{Value.has_value();*Value;})
     {
      static_assert(std::is_same_v<typename std::remove_cvref_t<decltype(Value)>::value_type,bool>,"JD_BIT channel members must be bool");
      const bool Mask=Value.has_value();return std::pair<bool,bool>{Mask&&*Value,Mask};
     }
    else
     {
      static_assert(std::is_same_v<std::remove_cvref_t<decltype(Value)>,bool>,"JD_BIT channel members must be bool");
      return std::pair<bool,bool>{Value,true};
     }
   };
#undef JD_Live
#undef JD_Info
#define JD_Live(Name, Presence)
#define JD_Info(Name, Presence)
#undef JD_Omit
#define JD_Omit(Member, Presence) \
  { const auto Pair=GetBit(Object.Member);ChannelBits.Bit.Member=Pair.first;ChannelBits.Bit.Member ## Msk=Pair.second; }
#define Do_TUdpChannel
#include <Definitions/Contract/TUdpChannel.inc.h>
#undef Do_TUdpChannel
#undef JD_Live
#undef JD_Info
#define JD_Live(Name, Presence) \
  if( Select( TStatusPart::Live, #Name ) )Emit( #Name, Name );
#define JD_Info(Name, Presence) \
  if( Select( TStatusPart::Info, #Name ) )Emit( #Name, Name );
#undef JD_Omit
#define JD_Omit(Name, Presence) \
  if( Select( TStatusPart::Omit, #Name ) )Emit( #Name, Name );
  const uint16_t Bits = ChannelBits.All;
  const auto Select=[&](TStatusPart Part,std::string_view Name)
   {
    if(Info)return Part==TStatusPart::Info;
    if(Name=="Bits" && !Bits)return false;
    return Part==TStatusPart::Live;
   };
  const auto & Id=Object.Id;const auto & State=Object.State;
  const auto & OvrUntil=Object.OvrUntil;
  const auto & Name=Object.Name;const auto & HwCh=Object.HwCh;const auto & HasIn=Object.HasIn;
  const auto & HasOut=Object.HasOut;const auto & InState=Object.InState;
  const auto & AutomaticState=Object.AutomaticState;const auto & OvrActive=Object.OvrActive;
  const auto & OvrState=Object.OvrState;const auto & CalOvrActive=Object.CalOvrActive;
  const auto WriteNext=[&](){if(Object.NextChange.Present&&!Object.NextChange.IsNull)WriteNextChange(Writer,Object.NextChange.Value);else Writer.Null();};
  const auto WriteLabels=[&](){if(Object.StateLabels.Present&&!Object.StateLabels.IsNull)WriteStateLabels(Writer,Object.StateLabels.Value);else Writer.Null();};
  const auto NextChange=JsonValue(std::type_identity<TUdpNextChange>{},WriteNext);
  const auto StateLabels=JsonValue(std::type_identity<TUdpStateLabels>{},WriteLabels);
  const auto & Err=Object.Err;
  auto Emit=[&](char const * Field,auto const & Value)
   { EmitField(Writer,Field,Value); };
  Writer.BeginObject();
#define Do_TUdpChannel
#include <Definitions/Contract/TUdpChannel.inc.h>
#undef Do_TUdpChannel
  Writer.EndObject();
 }

void WriteHeatConfig( TCompactStatusWriter & Writer, TJsonHeatConfig const & Object )
 {
  const auto Select=[](TStatusPart Part,std::string_view){return Part==TStatusPart::Live;};
  const auto & Setpoint=Object.Setpoint;const auto & FloorW=Object.FloorW;const auto & MainMaxW=Object.MainMaxW;
  const auto & CapacityWhPerK=Object.CapacityWhPerK;const auto & LossWPerK=Object.LossWPerK;const auto & Kp=Object.Kp;
  const auto & TiSec=Object.TiSec;const auto & WindowSec=Object.WindowSec;const auto & DelaySec=Object.DelaySec;
  const auto & FloorEndpoint=Object.FloorEndpoint;const auto & MainEndpoint=Object.MainEndpoint;const auto & Active=Object.Active;
  auto Emit=[&](char const * Name,auto const & Value){EmitField(Writer,Name,Value);};
  Writer.BeginObject();
#define Do_TJsonHeatConfig
#include <Definitions/Contract/TJsonHeatConfig.inc.h>
#undef Do_TJsonHeatConfig
  Writer.EndObject();
 }

void WriteHeatDiag( TCompactStatusWriter & Writer, TUdpHeatDiagnostics const & Object )
 {
  const auto Select=[](TStatusPart Part,std::string_view){return Part==TStatusPart::Live;};
  const auto & Active=Object.Active;const auto & Prof=Object.Prof;
  const auto & CfgField=Object.Cfg;const auto & Valid=Object.Valid;const auto & Learn=Object.Learn;
  const auto & FloorRequest=Object.FloorRequest;const auto & MainRequest=Object.MainRequest;
  const auto & FloorAvailable=Object.FloorAvailable;const auto & ActualMainOn=Object.ActualMainOn;
  const auto & ActualFloorOn=Object.ActualFloorOn;const auto & Reason=Object.Reason;const auto & LearnReason=Object.LearnReason;
  const auto & Temp=Object.Temp;const auto & ErrK=Object.ErrK;const auto & ProportionalW=Object.ProportionalW;
  const auto & IntegralW=Object.IntegralW;const auto & FeedForwardW=Object.FeedForwardW;const auto & DemandW=Object.DemandW;
  const auto & FloorDemandW=Object.FloorDemandW;const auto & MainDemandW=Object.MainDemandW;const auto & ActualW=Object.ActualW;
  const auto & CapacityWhPerK=Object.CapacityWhPerK;const auto & LossWPerK=Object.LossWPerK;const auto & Kp=Object.Kp;
  const auto & TiSec=Object.TiSec;const auto & Samples=Object.Samples;const auto & LearnedWindows=Object.LearnedWindows;
  const auto & WindowAgeSec=Object.WindowAgeSec;
  auto Emit=[&](char const * Name,auto const & Value){EmitField(Writer,Name,Value);};
  const auto WriteCfg=[&](){if(CfgField.Present&&!CfgField.IsNull)WriteHeatConfig(Writer,CfgField.Value);else Writer.Null();};
  const auto Cfg=JsonValue(std::type_identity<TJsonHeatConfig>{},WriteCfg);
  Writer.BeginObject();
#define Do_TUdpHeatDiagnostics
#include <Definitions/Contract/TUdpHeatDiagnostics.inc.h>
#undef Do_TUdpHeatDiagnostics
  Writer.EndObject();
 }

void WritePowerMeter( TCompactStatusWriter & Writer, TUdpPowerMeter const & Object, bool Signature )
 {
  const auto Select=[&](TStatusPart Part,std::string_view Name)
   {return Part==TStatusPart::Live && (!Signature || (Name!="Voltage"&&Name!="Current"&&Name!="ActivePower"));};
  const auto & Voltage=Object.Voltage;const auto & Current=Object.Current;const auto & ActivePower=Object.ActivePower;
  const auto & Valid=Object.Valid;
  const auto Diag=JsonValue(std::type_identity<TJsonPowerDiagnostics>{},[&](){Writer.BeginObject();Writer.EndObject();});
  auto Emit=[&](char const * Name,auto const & Value){EmitField(Writer,Name,Value);};
  Writer.BeginObject();
#define Do_TUdpPowerMeter
#include <Definitions/Contract/TUdpPowerMeter.inc.h>
#undef Do_TUdpPowerMeter
  Writer.EndObject();
 }

void WriteHeatControl( TCompactStatusWriter & Writer, TUdpHeatControl const & Object )
 {
  const auto Select=[](TStatusPart,std::string_view){return true;};
  const auto & Active=Object.Active;const auto & Prof=Object.Prof;
  const auto & FloorEndpoint=Object.FloorEndpoint;const auto & FloorRequest=Object.FloorRequest;
  auto Emit=[&](char const * Name,auto const & Value){EmitField(Writer,Name,Value);};
  Writer.BeginObject();
#define Do_TUdpHeatControl
#include <Definitions/Contract/TUdpHeatControl.inc.h>
#undef Do_TUdpHeatControl
  Writer.EndObject();
 }

void WriteChargeLearning( TCompactStatusWriter & Writer, TUdpChargeLearning const & Object )
 {
  const auto Select=[](TStatusPart,std::string_view){return true;};
  const auto & Seq=Object.Seq;const auto & CompletedAt=Object.CompletedAt;
  const auto & Prof=Object.Prof;const auto & BeforePct=Object.BeforePct;const auto & AfterPct=Object.AfterPct;
  const auto & AppliedSecPerPct=Object.AppliedSecPerPct;const auto & ChargeMs=Object.ChargeMs;
  const auto & NewSecPerPct=Object.NewSecPerPct;const auto & Persistence=Object.Persistence;
  auto Emit=[&](char const * Name,auto const & Value){EmitField(Writer,Name,Value);};
  Writer.BeginObject();
#define Do_TUdpChargeLearning
#include <Definitions/Contract/TUdpChargeLearning.inc.h>
#undef Do_TUdpChargeLearning
  Writer.EndObject();
 }

void WriteLastError( TCompactStatusWriter & Writer, TUdpLastError const & Object )
 {
  const auto Select=[](TStatusPart,std::string_view){return true;};
  const auto & Time=Object.Time;const auto & Message=Object.Message;
  auto Emit=[&](char const * Name,auto const & Value){EmitField(Writer,Name,Value);};
  Writer.BeginObject();
#define Do_TUdpLastError
#include <Definitions/Contract/TUdpLastError.inc.h>
#undef Do_TUdpLastError
  Writer.EndObject();
 }

void WriteMeasurement( TCompactStatusWriter & Writer, TUdpMeasurement const & Object,
                       bool Info, bool Signature )
 {
  const auto Select=[&](TStatusPart Part,std::string_view Name)
   {
    if(Info)return Part==TStatusPart::Info;
    return Part==TStatusPart::Live && (!Signature || (Name!="Value"&&Name!="UpdatedAt"));
   };
  const auto & Kind=Object.Kind;const auto & Value=Object.Value;const auto & Valid=Object.Valid;const auto & Name=Object.Name;
  const auto & Unit=Object.Unit;const auto & Decimals=Object.Decimals;const auto & UpdatedAt=Object.UpdatedAt;const auto & Err=Object.Err;
  auto Emit=[&](char const * Field,auto const & Item){EmitField(Writer,Field,Item);};
  Writer.BeginObject();
#define Do_TUdpMeasurement
#include <Definitions/Contract/TUdpMeasurement.inc.h>
#undef Do_TUdpMeasurement
  Writer.EndObject();
 }

void WriteStatusData( TCompactStatusWriter & Writer, TUdpStatusData const & Object,
                      bool Info, bool Signature )
 {
  const auto Select=[&](TStatusPart Part,std::string_view Name)
   {
    if(Info)return Part==TStatusPart::Info;
    if(Part!=TStatusPart::Live)return false;
    if(Signature && (Name=="CurrentTime"||Name=="Temp"||Name=="HeatDiag"||Name=="ChargeLearn"))return false;
    return true;
   };
  const auto & DevId=Object.DevId;
  const auto Identity=JsonValue(std::type_identity<TJsonIdentity>{},[&](){if(Object.Identity.Present&&!Object.Identity.IsNull)WriteIdentityInfo(Writer,Object.Identity.Value);else Writer.Null();});
  const auto & Kind=Object.Kind;
  const auto & OnlineSince=Object.OnlineSince;const auto & CurrentTime=Object.CurrentTime;
  const auto & TimeSynchronized=Object.TimeSynchronized;const auto & Healthy=Object.Healthy;
  const auto & TimeSyncErr=Object.TimeSyncErr;const auto & LastErrField=Object.LastErr;const auto & ErrsField=Object.Errs;
  const auto & ChsField=Object.Chs;const auto & MeasField=Object.Meas;const auto & PowerMeterField=Object.PowerMeter;
  const auto & Temp=Object.Temp;const auto & BatteryPct=Object.BatteryPct;const auto & AcOnline=Object.AcOnline;
  const auto & BatteryState=Object.BatteryState;const auto & ChargeLearnAllowed=Object.ChargeLearnAllowed;
  const auto & HeatControlField=Object.HeatControl;const auto & HeatDiagField=Object.HeatDiag;
  const auto & ChargeLearnField=Object.ChargeLearn;const auto & AppVer=Object.AppVer;const auto & FeedbackMonitors=Object.FeedbackMonitors;
  const auto & BootCnt=Object.BootCnt;
  const auto PowerMeter=JsonValue(std::type_identity<TUdpPowerMeter>{},[&]()
   {if(PowerMeterField.Present&&!PowerMeterField.IsNull)WritePowerMeter(Writer,PowerMeterField.Value,Signature);else Writer.Null();});
  const auto HeatDiag=JsonValue(std::type_identity<TUdpHeatDiagnostics>{},[&]()
   {if(HeatDiagField.Present&&!HeatDiagField.IsNull)WriteHeatDiag(Writer,HeatDiagField.Value);else Writer.Null();});
  const auto HeatControl=JsonValue(std::type_identity<TUdpHeatControl>{},[&]()
   {if(HeatControlField.Present&&!HeatControlField.IsNull)WriteHeatControl(Writer,HeatControlField.Value);else Writer.Null();});
  const auto ChargeLearn=JsonValue(std::type_identity<TUdpChargeLearning>{},[&]()
   {if(ChargeLearnField.Present&&!ChargeLearnField.IsNull)WriteChargeLearning(Writer,ChargeLearnField.Value);else Writer.Null();});
  const auto LastErr=JsonValue(std::type_identity<TUdpLastError>{},[&]()
   {if(LastErrField.Present&&!LastErrField.IsNull)WriteLastError(Writer,LastErrField.Value);else Writer.Null();});
  const auto Errs=JsonValue(std::type_identity<std::vector<TGNString>>{},[&]()
   {
    if(!ErrsField.Present){Writer.Null();return;}if(ErrsField.IsNull){Writer.Null();return;}
    Writer.BeginArray();for(auto const & Item:ErrsField.Value)Writer.String(Item);Writer.EndArray();
   });
  const auto Chs=JsonValue(std::type_identity<std::vector<TUdpChannel>>{},[&]()
   {
    if(!ChsField.Present||ChsField.IsNull){Writer.Null();return;}
    Writer.BeginArray();for(auto const & Item:ChsField.Value)WriteChannel(Writer,Item,false);Writer.EndArray();
   });
  const auto Meas=JsonValue(std::type_identity<std::vector<TUdpMeasurement>>{},[&]()
   {
    if(!MeasField.Present||MeasField.IsNull){Writer.Null();return;}
    Writer.BeginArray();for(auto const & Item:MeasField.Value)WriteMeasurement(Writer,Item,false,Signature);Writer.EndArray();
   });
  auto Emit=[&](char const * Name,auto const & Value){EmitField(Writer,Name,Value);};
  Writer.BeginObject();
#define Do_TUdpStatusData
#include <Definitions/Contract/TUdpStatusData.inc.h>
#undef Do_TUdpStatusData
  Writer.EndObject();
 }

bool StoreInfoPart( TCompactStatusWriter & Writer, TGnUdpInfoParts & Parts,
                    TGNString const & Name, auto Write )
 {
  Writer.ResetOutput();
  Write();
  if(Writer.Failed())return false;
  Parts.insert_or_assign(Name,Writer.CopyOutput());
  Writer.ResetOutput();
  return true;
 }

bool HashCompactStatus( TCompactStatusWriter & Writer, TUdpStatusData const & Status,
                        uint64_t & Signature )
 {
  Writer.BeginHash();
  Writer.Add("P:Root:");WriteStatusData(Writer,Status,true,true);
  Writer.Add("P:Identity:");
  if(Status.Identity.Present&&!Status.Identity.IsNull)WriteIdentityInfo(Writer,Status.Identity.Value);
  if(Status.Chs.Present&&!Status.Chs.IsNull)
   for(auto const & Channel:Status.Chs.Value)
    {
     Writer.Add("P:Channel:");Writer.String(Channel.Id.Value);
     WriteChannel(Writer,Channel,true);
    }
  if(Status.Meas.Present&&!Status.Meas.IsNull)
   for(auto const & Item:Status.Meas.Value)
    {
     Writer.Add("P:Measurement:");Writer.String(Item.Kind.Value);
     WriteMeasurement(Writer,Item,true,true);
    }
  Writer.Add("S:");WriteStatusData(Writer,Status,false,true);
  if(Writer.Failed())return false;
  Signature=Writer.Hash();
  return true;
 }

bool BuildCompactStatus( TUdpStatusData const & StatusData, TGNString const & Id,
                         uint64_t & Signature, TGnUdpInfoParts * Parts, TGNString & Packet )
 {
  const bool HashOnly=Parts==nullptr;
  TCompactStatusWriter Writer(HashOnly);
  if(Parts)Parts->clear();

  if(HashOnly)
   {
    Packet.clear();
    return HashCompactStatus(Writer,StatusData,Signature);
   }

  if(!StoreInfoPart(Writer,*Parts,"Root",[&](){WriteStatusData(Writer,StatusData,true,false);}))return false;
  if(StatusData.Identity.Present&&!StatusData.Identity.IsNull &&
     !StoreInfoPart(Writer,*Parts,"Identity",[&](){WriteIdentityInfo(Writer,StatusData.Identity.Value);}))return false;
  if(StatusData.Chs.Present&&!StatusData.Chs.IsNull)
    for(auto const & Channel:StatusData.Chs.Value)
     if(!StoreInfoPart(Writer,*Parts,"Channel:"+Channel.Id.Value,[&](){WriteChannel(Writer,Channel,true);}))return false;
  if(StatusData.Meas.Present&&!StatusData.Meas.IsNull)
    for(auto const & Item:StatusData.Meas.Value)
     if(!StoreInfoPart(Writer,*Parts,"Measurement:"+Item.Kind.Value,[&](){WriteMeasurement(Writer,Item,true,false);}))return false;

  Writer.BeginObject();
  const auto ID=JsonValue(std::type_identity<TChannelId>{},[&](){Writer.String(Id);});
  const bool Compact=true;const TGNString InfoRev=GnUdpInfoHash(*Parts);
  const auto Status=JsonValue(std::type_identity<TUdpStatusData>{},[&](){WriteStatusData(Writer,StatusData,false,false);});
  const auto Select=[](TStatusPart,std::string_view){return true;};
  auto Emit=[&](char const * Name,auto const & Value){EmitField(Writer,Name,Value);};
#define Do_TUdpStatusMessage
#include <Definitions/Contract/TUdpStatusMessage.inc.h>
#undef Do_TUdpStatusMessage
  Writer.EndObject();
  Packet=Writer.Take();
  if(Packet.empty())return false;
  return HashCompactStatus(Writer,StatusData,Signature);
 }
 }

bool GnBuildCompactStatus( TUdpStatusData const & Status, TGNString const & Id,
                           uint64_t & Signature, TGnUdpInfoParts * Parts,
                           TGNString & Packet )
 {
  return BuildCompactStatus( Status, Id, Signature, Parts, Packet );
 }

#undef MapOf
#undef ArrayOf
#undef JD
#undef JD_Live
#undef JD_Info
#undef JD_Omit
#undef JD_CONST
#undef JD_BASE
#undef JD_VALUE
#undef JD_TUPLE_BEGIN
#undef Do_TUdpRequestMessage
#undef Do_TUdpInfoRequestMessage
#undef Do_TUdpInfoMessage
#undef Do_TUdpSourceRequestMessage
#undef Do_TUdpSourceInfoMessage
#undef Do_TUdpStatusData
#undef Do_TUdpStatusMessage
#undef Do_TJsonIdentity
#undef Do_TUdpChannel
#undef Do_TUdpNextChange
#undef Do_TUdpStateLabels
#undef Do_TUdpPowerMeter
#undef Do_TUdpHeatControl
#undef Do_TUdpHeatDiagnostics
#undef Do_TJsonHeatConfig
#undef Do_TUdpChargeLearning
#undef Do_TUdpLastError
#undef Do_TUdpMeasurement
#undef Do_TJsonFeedbackState

