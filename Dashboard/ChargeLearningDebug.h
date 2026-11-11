// ChargeLearningDebug.h, Version: 1.02
#pragma once
#include <GnTemporaryDiagnostics.h>
#include <GnWireJson.h>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <cmath>
#include <iostream>

// Temporary debug journal. No switching, configuration writes or inference from percentages.
namespace ChargeLearningDebug
 {
inline std::filesystem::path File(std::filesystem::path const & Root)
 { return Root/"generated"/"ChargeLearning"/"Steps.json"; }
inline TGNString Read(std::filesystem::path const & Root)
 {
  std::ifstream Input(File(Root));
  if(!Input)return std::filesystem::exists(File(Root))?"null":"[]";
  return TGNString(std::istreambuf_iterator<char>(Input),{});
 }
inline bool Number(TGnWireValue Value,double Minimum,double Maximum)
 {
  const auto Type=json_object_get_type(Value.V);
  if(Type!=json_type_int && Type!=json_type_double)return false;
  const double X=json_object_get_double(Value.V);
  return std::isfinite(X) && X>=Minimum && X<=Maximum;
 }
inline void Receive(std::filesystem::path const & Root,json_object * Packet)
 {
#if GN_DEBUG_CHARGE_LEARNING
  TGnWireValue P(Packet);auto S=P.Get("Status"),Step=S.Get("ChargeLearn");
  if(!Step.Object() || Step.Get("Ver").UInt()!=1)return;
  auto Boot=S.Get("BootCnt");
  const auto Persistence=Step.Get("Persistence").Text();
  if(!Number(Boot,0,UINT32_MAX) || !Number(Step.Get("Seq"),1,UINT32_MAX) ||
     !Number(Step.Get("BeforePct"),0,100) || !Number(Step.Get("AfterPct"),0,100) ||
     !Number(Step.Get("AppliedSecPerPct"),1,UINT32_MAX) ||
     !Number(Step.Get("NewSecPerPct"),1,UINT32_MAX) ||
     !Number(Step.Get("ChargeMs"),1,9007199254740991.0) ||
     (Persistence!="RamOnly" && Persistence!="Saved" && Persistence!="SaveFailed"))return;
  if(Step.Get("AfterPct").V && json_object_get_double(Step.Get("AfterPct").V)<=json_object_get_double(Step.Get("BeforePct").V))return;
  if(Step.Json().size()>2048 || Step.Get("Prof").Text().empty())return;
  const auto Key=P.Get("ID").Text()+":"+Boot.Json()+":"+Step.Get("Seq").Json();
  TGnWireDocument Existing(Read(Root));
  if(!Existing.Root().Array()) { std::cerr<<"Charge learning debug: invalid journal, preserved\n";return; }
  for(auto Row:Existing.Root().Items())if(Row.Get("CycleId").Text()==Key)return;
  auto * Output=json_object_new_array();
  const auto Rows=Existing.Root().Items();
  const size_t Start=Rows.size()>=1000?Rows.size()-999:0;
  for(size_t I=Start;I<Rows.size();++I)json_object_array_add(Output,json_object_get(Rows[I].V));
  auto * Record=json_tokener_parse(Step.Json().c_str());
  json_object_object_add(Record,"CycleId",json_object_new_string(Key.c_str()));
  json_object_object_add(Record,"Dev",json_object_new_string(P.Get("ID").Text().c_str()));
  json_object_object_add(Record,"SwitchBootCounter",json_object_get(Boot.V));
  json_object_object_add(Record,"ReceivedAt",json_object_new_int64(time(nullptr)));
  // Saved is a device write result, not proof of completed central distribution.
  json_object_object_add(Record,"Synchronization",json_object_new_string(Persistence=="RamOnly"?"NotRequired":"NotObserved"));
  json_object_array_add(Output,Record);
  const TGNString Text=json_object_to_json_string_ext(Output,JSON_C_TO_STRING_PRETTY);
  json_object_put(Output);
  std::error_code Error;const auto Target=File(Root);
  std::filesystem::create_directories(Target.parent_path(),Error);
  auto Temporary=Target;Temporary+=".new";
  bool Written=false;
  if(!Error)
   {
    std::ofstream Stream(Temporary);Stream<<Text<<'\n';Stream.close();Written=bool(Stream);
    if(Written)std::filesystem::rename(Temporary,Target,Error);
   }
  if(!Written || Error)std::cerr<<"Charge learning debug: journal write failed\n";
#else
  (void)Root;(void)Packet;
#endif
 }
 }
