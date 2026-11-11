// GnWireJson.h, Version: 1.05
#pragma once
#include <GnBasics.h>
#include <vector>
#include <utility>
#if defined(GNPlatformESP)
#include <ArduinoJson.h>
#else
#include <json-c/json.h>
#endif

// Small non-owning JSON view; documents below own all referenced values.
class TGnWireValue
 {
 public:
#if defined(GNPlatformESP)
  JsonVariant V;
  TGnWireValue(JsonVariant Value = {}) : V(Value) {}
  bool Object(void) const { return V.is<JsonObject>(); }
  bool Array(void) const { return V.is<JsonArray>(); }
  bool Null(void) const { return V.isNull(); }
  bool IsText(void) const { return V.is<const char *>(); }
  bool IsBool(void) const { return V.is<bool>(); }
  bool IsUnsigned(void) const { return !V.is<bool>() && V.is<uint64_t>(); }
  bool IsNumber(void) const { return !V.is<bool>() && V.is<double>(); }
  double Number(void) const { return V.as<double>(); }
  size_t Size(void) const { return V.size(); }
  size_t JsonSize(void) const { return measureJson(V); }
  TGnWireValue At(size_t Index) const { return TGnWireValue(V[Index].as<JsonVariant>()); }
  bool Has(TGNString const & K) const { return !V[K].isUnbound(); }
  TGnWireValue Get(TGNString const & K) const { return TGnWireValue(V[K].as<JsonVariant>()); }
  TGNString Text(void) const { return V.is<const char *>() ? V.as<const char *>() : TGNString{}; }
  bool Bool(void) const { return V.is<bool>() && V.as<bool>(); }
  uint64_t UInt(void) const { return V.as<uint64_t>(); }
  void Remove(TGNString const & K) { V.as<JsonObject>().remove(K); }
  void Set(TGNString const & K,TGnWireValue X) { V[K].set(X.V); }
  void Text(TGNString const & K,TGNString const & X) { V[K]=X; }
  void Bool(TGNString const & K,bool X) { V[K]=X; }
  void UInt(TGNString const & K,uint64_t X) { V[K]=X; }
  void SetNull(TGNString const & K) { V[K]=nullptr; }
  void Number(TGNString const & K,double X) { V[K]=X; }
  void AddValue(TGnWireValue X) { V.as<JsonArray>().add(X.V); }
  TGnWireValue NewObject(TGNString const & K) { V[K].to<JsonObject>(); return TGnWireValue(V[K].as<JsonVariant>()); }
  TGnWireValue NewArray(TGNString const & K) { V[K].to<JsonArray>(); return TGnWireValue(V[K].as<JsonVariant>()); }
  TGnWireValue AddObject(void) { return TGnWireValue(JsonVariant(V.as<JsonArray>().add<JsonObject>())); }
  void AddText(TGNString const & X) { V.as<JsonArray>().add(X); }
  TGNString Json(void) const
   {
    TGNString Result;
    const size_t Length = measureJson(V);
    if( !GnReserveStringExact(Result,Length) )return {};
    TGnStringAppendWriter Writer(Result);
    if( serializeJson(V,Writer) != Length || Result.length() != Length )return {};
    return Result;
   }
  std::vector<std::pair<TGNString,TGnWireValue>> Members(void) const
   { std::vector<std::pair<TGNString,TGnWireValue>> R;for(JsonPair P:V.as<JsonObject>())R.emplace_back(P.key().c_str(),P.value());return R; }
  std::vector<TGnWireValue> Items(void) const
   { std::vector<TGnWireValue> R;for(JsonVariant X:V.as<JsonArray>())R.emplace_back(X);return R; }
#else
  json_object * V = nullptr;
  TGnWireValue(json_object * Value = nullptr) : V(Value) {}
  bool Object(void) const { return json_object_is_type(V,json_type_object); }
  bool Array(void) const { return json_object_is_type(V,json_type_array); }
  bool Null(void) const { return V==nullptr || json_object_is_type(V,json_type_null); }
  bool IsText(void) const { return json_object_is_type(V,json_type_string); }
  bool IsBool(void) const { return json_object_is_type(V,json_type_boolean); }
  bool IsUnsigned(void) const { return json_object_is_type(V,json_type_int) && json_object_get_int64(V)>=0; }
  bool IsNumber(void) const { return json_object_is_type(V,json_type_int) || json_object_is_type(V,json_type_double); }
  double Number(void) const { return json_object_get_double(V); }
  size_t Size(void) const { return Array()?json_object_array_length(V):Object()?json_object_object_length(V):0; }
  size_t JsonSize(void) const
   {
    size_t Length = 4;
    if( V )json_object_to_json_string_length(V,JSON_C_TO_STRING_PLAIN,&Length);
    return Length;
   }
  TGnWireValue At(size_t Index) const { return Array() && Index<Size()?TGnWireValue(json_object_array_get_idx(V,Index)):TGnWireValue{}; }
  bool Has(TGNString const & K) const { json_object * P=nullptr;return Object() && json_object_object_get_ex(V,K.c_str(),&P); }
  TGnWireValue Get(TGNString const & K) const { json_object * P=nullptr;if(Object())json_object_object_get_ex(V,K.c_str(),&P);return P; }
  TGNString Text(void) const { return json_object_is_type(V,json_type_string)?json_object_get_string(V):TGNString{}; }
  bool Bool(void) const { return json_object_is_type(V,json_type_boolean) && json_object_get_boolean(V); }
  uint64_t UInt(void) const { return V?json_object_get_uint64(V):0; }
  void Remove(TGNString const & K) { if(Object())json_object_object_del(V,K.c_str()); }
  void Set(TGNString const & K,TGnWireValue X)
   {
    json_object * Copy = nullptr;
    if( X.V && json_object_deep_copy(X.V,&Copy,nullptr) != 0 )return;
    if( json_object_object_add(V,K.c_str(),Copy) != 0 && Copy )json_object_put(Copy);
   }
  void Text(TGNString const & K,TGNString const & X) { json_object_object_add(V,K.c_str(),json_object_new_string_len(X.data(),X.size())); }
  void Bool(TGNString const & K,bool X) { json_object_object_add(V,K.c_str(),json_object_new_boolean(X)); }
  void UInt(TGNString const & K,uint64_t X) { json_object_object_add(V,K.c_str(),json_object_new_uint64(X)); }
  void SetNull(TGNString const & K) { json_object_object_add(V,K.c_str(),nullptr); }
  void Number(TGNString const & K,double X) { json_object_object_add(V,K.c_str(),json_object_new_double(X)); }
  void AddValue(TGnWireValue X)
   {
    json_object * Copy = nullptr;
    if( X.V && json_object_deep_copy(X.V,&Copy,nullptr) != 0 )return;
    if( json_object_array_add(V,Copy) != 0 && Copy )json_object_put(Copy);
   }
  TGnWireValue NewObject(TGNString const & K) { auto * X=json_object_new_object();json_object_object_add(V,K.c_str(),X);return X; }
  TGnWireValue NewArray(TGNString const & K) { auto * X=json_object_new_array();json_object_object_add(V,K.c_str(),X);return X; }
  TGnWireValue AddObject(void) { auto * X=json_object_new_object();json_object_array_add(V,X);return X; }
  void AddText(TGNString const & X) { json_object_array_add(V,json_object_new_string_len(X.data(),X.size())); }
  TGNString Json(void) const { return V?json_object_to_json_string_ext(V,JSON_C_TO_STRING_PLAIN):"null"; }
  std::vector<std::pair<TGNString,TGnWireValue>> Members(void) const
   { std::vector<std::pair<TGNString,TGnWireValue>> R;if(Object()){json_object_object_foreach(V,K,X){R.emplace_back(K,X);}}return R; }
  std::vector<TGnWireValue> Items(void) const
   { std::vector<TGnWireValue> R;if(Array())for(size_t I=0;I<json_object_array_length(V);++I)R.emplace_back(json_object_array_get_idx(V,I));return R; }
#endif
 };
class TGnWireDocument
 {
  bool FValid = true;
 public:
  bool Valid(void) const { return FValid; }
#if defined(GNPlatformESP)
  JsonDocument D;
  TGnWireDocument(void) { D.to<JsonObject>(); }
  explicit TGnWireDocument(TGNString const & Text)
   { FValid=!deserializeJson(D,Text.c_str(),Text.size());if(!FValid)D.clear(); }
  TGnWireValue Root(void) { return D.as<JsonVariant>(); }
#else
  json_object * D;
  TGnWireDocument(void) : D(json_object_new_object()) {}
  explicit TGnWireDocument(TGNString const & Text) : D(nullptr)
   {
    auto * Parser=json_tokener_new();
    if(!Parser) { FValid=false;return; }
    json_tokener_set_flags(Parser,JSON_TOKENER_STRICT);
    D=json_tokener_parse_ex(Parser,Text.c_str(),Text.size()+1);
    FValid=json_tokener_get_error(Parser)==json_tokener_success;
    json_tokener_free(Parser);
    if(!FValid&&D) { json_object_put(D);D=nullptr; }
   }
  ~TGnWireDocument(void) { if(D)json_object_put(D); }
  TGnWireValue Root(void) { return D; }
#endif
  TGnWireDocument(TGnWireDocument const &)=delete;
  TGnWireDocument & operator=(TGnWireDocument const &)=delete;
 };
