// GnWireJson.h, Version: 1.00
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
  TGnWireValue NewObject(TGNString const & K) { V[K].to<JsonObject>(); return TGnWireValue(V[K].as<JsonVariant>()); }
  TGnWireValue NewArray(TGNString const & K) { V[K].to<JsonArray>(); return TGnWireValue(V[K].as<JsonVariant>()); }
  TGnWireValue AddObject(void) { return TGnWireValue(JsonVariant(V.as<JsonArray>().add<JsonObject>())); }
  void AddText(TGNString const & X) { V.as<JsonArray>().add(X); }
  TGNString Json(void) const { TGNString R;serializeJson(V,R);return R; }
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
  bool Has(TGNString const & K) const { json_object * P=nullptr;return Object() && json_object_object_get_ex(V,K.c_str(),&P); }
  TGnWireValue Get(TGNString const & K) const { json_object * P=nullptr;if(Object())json_object_object_get_ex(V,K.c_str(),&P);return P; }
  TGNString Text(void) const { return json_object_is_type(V,json_type_string)?json_object_get_string(V):TGNString{}; }
  bool Bool(void) const { return json_object_is_type(V,json_type_boolean) && json_object_get_boolean(V); }
  uint64_t UInt(void) const { return V?json_object_get_uint64(V):0; }
  void Remove(TGNString const & K) { if(Object())json_object_object_del(V,K.c_str()); }
  void Set(TGNString const & K,TGnWireValue X) { json_object_object_add(V,K.c_str(),X.Null()?nullptr:json_tokener_parse(X.Json().c_str())); }
  void Text(TGNString const & K,TGNString const & X) { json_object_object_add(V,K.c_str(),json_object_new_string_len(X.data(),X.size())); }
  void Bool(TGNString const & K,bool X) { json_object_object_add(V,K.c_str(),json_object_new_boolean(X)); }
  void UInt(TGNString const & K,uint64_t X) { json_object_object_add(V,K.c_str(),json_object_new_uint64(X)); }
  void SetNull(TGNString const & K) { json_object_object_add(V,K.c_str(),nullptr); }
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
 public:
#if defined(GNPlatformESP)
  JsonDocument D;
  TGnWireDocument(void) { D.to<JsonObject>(); }
  explicit TGnWireDocument(TGNString const & Text) { if(deserializeJson(D,Text.c_str(),Text.size()))D.clear(); }
  TGnWireValue Root(void) { return D.as<JsonVariant>(); }
#else
  json_object * D;
  TGnWireDocument(void) : D(json_object_new_object()) {}
  explicit TGnWireDocument(TGNString const & Text) : D(json_tokener_parse(Text.c_str())) {}
  ~TGnWireDocument(void) { if(D)json_object_put(D); }
  TGnWireValue Root(void) { return D; }
#endif
  TGnWireDocument(TGnWireDocument const &)=delete;
  TGnWireDocument & operator=(TGnWireDocument const &)=delete;
 };
