// GnJsonRules.h, Version: 1.01
#pragma once
#include <GnBasics.h>
#include <GnUdpSchemaSupport.h>
#include <array>
#include <ChannelAddress.h>
#include <string_view>

// Rule expressions are decomposed at compile time. Runtime code checks values,
// not the spelling of rule names.
template<size_t N> struct TGnJsonRuleText
 {
  char Text[N]{};
  constexpr TGnJsonRuleText(void) = default;
  constexpr TGnJsonRuleText(char const (&Input)[N])
   { for(size_t I=0;I<N;++I)Text[I]=Input[I]; }
  constexpr std::string_view View(void) const { return {Text,N-1}; }
 };
template<TGnJsonRuleText Rule> consteval auto GnJsonRuleArgs(void)
 {
  struct TSpan { size_t Start=0,Length=0; };
  std::array<TSpan,16> Result{};
  auto Text=Rule.View();
  size_t Start=Text.find('(')+1,Index=0,Depth=0;
  for(size_t I=Start;I<Text.size();++I)
   {
    if(Text[I]=='(')++Depth;
    if(Text[I]==')')
     {
      if(!Depth) { Result[Index]={Start,I-Start};break; }
      --Depth;
     }
    if(Text[I]==',' && !Depth) { Result[Index++]={Start,I-Start};Start=I+1; }
   }
  return Result;
 }
template<TGnJsonRuleText Rule,size_t Index> consteval auto GnJsonRuleArg(void)
 {
  constexpr auto Span=GnJsonRuleArgs<Rule>()[Index];
  TGnJsonRuleText<Span.Length+1> Result;
  for(size_t I=0;I<Span.Length;++I)Result.Text[I]=Rule.Text[Span.Start+I];
  return Result;
 }
template<TGnJsonRuleText Text> constexpr double GnJsonRuleNumber(void)
 {
  double Value=0,Scale=1;bool Fraction=false,Negative=false;
  for(char C:Text.View())
   {
    if(C=='-')Negative=true;
    else if(C=='.')Fraction=true;
    else if(C>='0' && C<='9')
     { Value=Value*10+(C-'0');if(Fraction)Scale*=10; }
   }
  return (Negative?-Value:Value)/Scale;
 }
template<TGnJsonRuleText Rule,class TContext> bool GnJsonRule(TContext const & Input,TGnWireValue Field);

template<TGnJsonRuleText Rule,class TContext,size_t... I>
bool GnJsonRuleAll(TContext const & Input,TGnWireValue Field,std::index_sequence<I...>)
 {return (GnJsonRule<GnJsonRuleArg<Rule,I>()>(Input,Field) && ...);}
template<TGnJsonRuleText Rule,size_t... I>
bool GnJsonRuleChoice(TGnWireValue Field,std::index_sequence<I...>)
 {return ((Field.Text()==GnJsonRuleArg<Rule,I>().Text) || ...);}
template<TGnJsonRuleText Rule,size_t... I>
bool GnJsonRuleServices(TGnWireValue Field,std::index_sequence<I...>)
 {return GnServiceId(Field.Text(),{unsigned(GnJsonRuleNumber<GnJsonRuleArg<Rule,I>()>())...});}
template<TGnJsonRuleText Rule> consteval size_t GnJsonRuleArgCount(void)
 {
  size_t Count=0;
  for(auto Span:GnJsonRuleArgs<Rule>())if(Span.Length)++Count;
  return Count;
 }
template<TGnJsonRuleText Rule,class TContext>
bool GnJsonRule(TContext const & Input,TGnWireValue Field)
 {
  constexpr auto Text=Rule.View();
  constexpr auto Kind=Text.substr(0,Text.find('('));
  if constexpr(Kind=="RequiredIf")return GnJsonRule<GnJsonRuleArg<Rule,1>()>(Input,Field);
  else if constexpr(Kind=="RequiredUnless" || Kind=="RequiredWhen")return GnJsonRule<GnJsonRuleArg<Rule,2>()>(Input,Field);
  else if constexpr(Kind=="Any" || Kind=="Finite")return true;
  else if constexpr(Kind=="ChannelAddress" || Kind=="ServiceAddress" || Kind=="OutputAddress")
   {
    TChannelAddress Address;
    return TChannelAddress::Parse(Field.Text(),Address) &&
      (Kind!="ServiceAddress" || (Address.HasService && !Address.HasChannel));
   }
  else if constexpr(Kind=="HttpUrl" || Kind=="WebSocketUrl")
   {
    auto Value=Field.Text();
    const auto Split=Value.find("://");
    if(Split==TGNString::npos)return false;
    auto Scheme=Value.substr(0,Split);
    if(Kind=="HttpUrl" ? (Scheme!="http" && Scheme!="https") : (Scheme!="ws" && Scheme!="wss"))return false;
    auto Host=Value.substr(Split+3,Value.find('/',Split+3)-(Split+3));
    return !Host.empty() && Value.find_first_of(" \t\r\n")==TGNString::npos;
   }
  else if constexpr(Kind=="TemperatureProfileIfActive")
   {
    if(!Field.Get("Active").Bool())return true;
    if(Input.Get("Meas").Text()!="Temperature")return false;
    for(auto Key:{"Setpoint","FloorW","MainMaxW","CapacityWhPerK","LossWPerK","Kp","TiSec","WindowSec","DelaySec"})
      if(!Field.Has(Key) || Field.Get(Key).Null())return false;
    return Field.Get("CapacityWhPerK").Number()>0 && Field.Get("Kp").Number()>0 &&
      Field.Get("TiSec").Number()>0 && Field.Get("WindowSec").Number()>0 &&
      Field.Get("FloorW").Number()>=0 && Field.Get("MainMaxW").Number()>=0 && Field.Get("LossWPerK").Number()>=0;
   }
  else if constexpr(Kind=="NonEmpty")return !Field.Text().empty();
  else if constexpr(Kind=="Hex16")return GnHex16(Field.Text());
  else if constexpr(Kind=="Revision")return GnRevision(Field.Text());
  else if constexpr(Kind=="WireId" || Kind=="SourceId")return GnWireId(Field.Text(),Kind=="SourceId");
  else if constexpr(Kind=="TrueValue")return Field.Bool();
  else if constexpr(Kind=="EmptyOrUuid")return GnEmptyOrUuid(Field.Text());
  else if constexpr(Kind=="SensorId")return GnSensorId(Field.Text());
  else if constexpr(Kind=="Ipv4")return GnIpv4(Field.Text());
  else if constexpr(Kind=="MeasurementName")return GnMeasurementName(Field.Text());
  else if constexpr(Kind=="TimeOfDay")return GnTimeOfDay(Field.Text());
  else if constexpr(Kind=="Duration")return GnDuration(Field.Text());
  else if constexpr(Kind=="PositiveDuration")return GnDuration(Field.Text()) && Field.Text()!="00:00";
  else if constexpr(Kind=="DurationBefore")
    return GnDuration(Field.Text()) && Field.Text()!="00:00" && Field.Text()<Input.Get(GnJsonRuleArg<Rule,0>().Text).Text();
  else if constexpr(Kind=="FitsFloat")return std::abs(Field.Number())<=std::numeric_limits<float>::max();
  else if constexpr(Kind=="Positive")
   {
    if constexpr(Text.find('(')==std::string_view::npos)return Field.Number()>0;
    else return Field.Number()>0 && Field.Number()<=GnJsonRuleNumber<GnJsonRuleArg<Rule,0>()>();
   }
  else if constexpr(Kind=="Range")
    return Field.Number()>=GnJsonRuleNumber<GnJsonRuleArg<Rule,0>()>() && Field.Number()<=GnJsonRuleNumber<GnJsonRuleArg<Rule,1>()>();
  else if constexpr(Kind=="Count")
    return Field.Size()>=GnJsonRuleNumber<GnJsonRuleArg<Rule,0>()>() && Field.Size()<=GnJsonRuleNumber<GnJsonRuleArg<Rule,1>()>();
  else if constexpr(Kind=="Utf8Bytes")return Field.Text().size()<=GnJsonRuleNumber<GnJsonRuleArg<Rule,0>()>();
  else if constexpr(Kind=="TextLength" || Kind=="EmptyOrLength")
    return (Kind=="EmptyOrLength" && Field.Text().empty()) ||
      (Field.Text().size()>=GnJsonRuleNumber<GnJsonRuleArg<Rule,0>()>() && Field.Text().size()<=GnJsonRuleNumber<GnJsonRuleArg<Rule,1>()>());
  else if constexpr(Kind=="Choice")return GnJsonRuleChoice<Rule>(Field,std::make_index_sequence<GnJsonRuleArgCount<Rule>()>{});
  else if constexpr(Kind=="All")return GnJsonRuleAll<Rule>(Input,Field,std::make_index_sequence<GnJsonRuleArgCount<Rule>()>{});
  else if constexpr(Kind=="Each")
    return GnEachJson(Field,[&](TGnWireValue Item){return GnJsonRule<GnJsonRuleArg<Rule,0>()>(Input,Item);});
  else if constexpr(Kind=="ServiceIds")return GnJsonRuleServices<Rule>(Field,std::make_index_sequence<GnJsonRuleArgCount<Rule>()>{});
  else if constexpr(Kind=="Below")
    return !Input.Has(GnJsonRuleArg<Rule,0>().Text) || Field.Number()<Input.Get(GnJsonRuleArg<Rule,0>().Text).Number();
  else if constexpr(Kind=="WireIdIf")return !Input.Get(GnJsonRuleArg<Rule,0>().Text).Bool() || GnWireId(Field.Text());
  else if constexpr(Kind=="ItemsHaveIf")return !Input.Get(GnJsonRuleArg<Rule,0>().Text).Bool() ||
    GnItemsHave(Field.Get(GnJsonRuleArg<Rule,1>().Text),GnJsonRuleArg<Rule,2>().Text);
  else if constexpr(Kind=="TextIf")return !Input.Get(GnJsonRuleArg<Rule,0>().Text).Bool() || Field.Text()==GnJsonRuleArg<Rule,1>().Text;
  else if constexpr(Kind=="RangeIf")return !Input.Get(GnJsonRuleArg<Rule,0>().Text).Bool() ||
    (Field.Number()>=GnJsonRuleNumber<GnJsonRuleArg<Rule,1>()>() && Field.Number()<=GnJsonRuleNumber<GnJsonRuleArg<Rule,2>()>());
  else if constexpr(Kind=="ParentTextUnless")return Field.Get(GnJsonRuleArg<Rule,0>().Text).Text()==GnJsonRuleArg<Rule,1>().Text ||
    !Input.Has(GnJsonRuleArg<Rule,2>().Text) || Input.Get(GnJsonRuleArg<Rule,2>().Text).Text()==GnJsonRuleArg<Rule,3>().Text;
  else static_assert(Text.size()==0,"Unknown JSON field rule");
 }
template<TGnJsonRuleText Rule,class TContext> bool GnJsonRequired(TContext const & Input)
 {
  constexpr auto Kind=Rule.View().substr(0,Rule.View().find('('));
  if constexpr(Kind=="RequiredIf")return Input.Get(GnJsonRuleArg<Rule,0>().Text).Bool();
  else if constexpr(Kind=="RequiredUnless")return Input.Get(GnJsonRuleArg<Rule,0>().Text).Text()!=GnJsonRuleArg<Rule,1>().Text;
  else if constexpr(Kind=="RequiredWhen")return Input.Get(GnJsonRuleArg<Rule,0>().Text).Text()==GnJsonRuleArg<Rule,1>().Text;
  else return false;
 }
template<class T> bool GnJsonConstant(TGnWireValue Field,T const & Value)
 {
  if constexpr(std::is_same_v<T,TGNString>)return Field.Text()==Value;
  else return Field.UInt()==uint64_t(Value);
 }
