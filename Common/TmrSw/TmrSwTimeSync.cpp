// TmrSwTimeSync.cpp, Version: 1.20
#define Do_TJsonConfigurationFiles
#define Do_TJsonFunctionEnvelope
#define Do_TJsonSchedule
#include <GnBasics.h>
#include <GnConfigurationJournal.h>
#include "TmrSwTimeSync.h"
#include <GnDeviceIdentity.h>
#include <GnFunctionConfig.h>
#include "TmrSwConfig.h"

#include "SwitchTimes.h"
#include "TmrSwChannels.h"

// Do not expand the singleton shorthand inside schedule field names.
#undef SwitchTimes

TGnTimeSyncConfig const TmrSwTimeSyncConfig =
 {
  "/SwitchTimes.sync.json",
  "/SwitchTimes.json",
  "SwitchTimes",
  "TmrSw",
  4000,
  "/Channels.json",
  "/Intervals.json",
  TmrSwTimeSyncFunctionConfig
 };

namespace
 {
constexpr char TransactionFile[] = "/TimeConfig.transaction.json";
bool WriteConfiguration( char const * Name, String const & Text )
 {
  // Keep the journal temporary basename within legacy LittleFS name_max=32.
  const String Temporary = String( Name ) + ".new";
  File Out = LittleFS.open( Temporary.c_str(), "w" );
  if( !Out ) return false;
  const bool Ok = Out.write( reinterpret_cast<uint8_t const *>( Text.c_str() ), Text.length() ) == Text.length();
  Out.flush(); Out.close();
  return Ok && LittleFS.rename( Temporary.c_str(), Name );
 }
bool MaterializeConfiguration(TJsonConfigurationFiles const & Document)
 {
  return WriteConfiguration("/Channels.json",Document.Chs.Value.c_str()) &&
    WriteConfiguration("/SwitchTimes.json",Document.Schedule.Value.c_str()) &&
    WriteConfiguration("/Intervals.json",Document.Intervals.Value.c_str());
 }
String ReadConfiguration(char const * Name)
 {
  File Input=LittleFS.open(Name,"r");
  return Input?Input.readString():String{};
 }

 }

bool TmrSwRecoverConfiguration(void)
 {
  if(!LittleFS.exists(TransactionFile))return true;
  TJsonConfigurationFiles Document;
  if(!GnReadConfigurationJournal(ReadConfiguration(TransactionFile).c_str(),true,Document))return false;
  String Error;TTmrSwChannelConfig Channel;THeaterProfiles Profiles;
  if(!TTmrSwChannels::Validate(Document.Chs.Value.c_str(),Channel,Profiles,Error) ||
     TSwitchTimes::ValidateBundle(Document.Schedule.Value.c_str(),Document.Intervals.Value.c_str(),nullptr)!="1")return false;
  return MaterializeConfiguration(Document) && LittleFS.remove(TransactionFile);
 }

String TmrSwTimeSyncApply(char const * Content)
 {
  TJsonSchedule Update;
  if(!GnReadJsonValue(Content,Update))return F("InvalidContent");
  if(!Update.Chs.Present && !Update.SwitchTimes.Present && !Update.Intervals.Present)return F("InvalidContent");
  if(Update.Chs.Present && Update.Chs.IsNull)return F("InvalidChannels");
  const String Channels=Update.Chs.Present ? String(GnWriteJson(Update.Chs.Value).c_str()) : ReadConfiguration("/Channels.json");
  const String Schedule=Update.SwitchTimes.Present ? (Update.SwitchTimes.IsNull ? String("[]") : String(GnWriteJson(Update.SwitchTimes.Value).c_str())) : ReadConfiguration("/SwitchTimes.json");
  const String Intervals=Update.Intervals.Present ? (Update.Intervals.IsNull ? String("[]") : String(GnWriteJson(Update.Intervals.Value).c_str())) : ReadConfiguration("/Intervals.json");
  String Error;TTmrSwChannelConfig Channel;THeaterProfiles Profiles;
  if(!TTmrSwChannels::Validate(Channels,Channel,Profiles,Error))return Error;
  const String Checked=TSwitchTimes::ValidateBundle(Schedule,Intervals,nullptr);
  if(Checked!="1")return Checked;
  const bool Changed=TmrSwChannels.Channel().Id!=Channel.Id || TmrSwChannels.Channel().Role!=Channel.Role || TmrSwChannels.Channel().Profile!=Channel.Profile;
  TJsonConfigurationFiles Transaction;
  Transaction.Chs=Channels.c_str();
  Transaction.Schedule=Schedule.c_str();
  Transaction.Intervals=Intervals.c_str();
  const String Journal=GnWriteJson(Transaction).c_str();
  if(Journal.isEmpty() || !WriteConfiguration(TransactionFile,Journal) || !MaterializeConfiguration(Transaction))return F("ConfigurationCommitFailed");
  if(!TmrSwChannels.Setup())return F("ConfigurationLoadFailed");
  if(Changed || GnDeviceIdentity.FunctionChanged())TSwitchTimes::Instance().ResetFunctionState();
  const String Loaded=TSwitchTimes::Instance().LoadBundle(Schedule,Intervals);
  if(Loaded!="1")return Loaded;
  if(!LittleFS.remove(TransactionFile))return F("ConfigurationCompletionFailed");
  return "1";
 }

String TmrSwTimeSyncFunctionConfig( char const * Envelope )
 {
  return GnApplyFunctionConfig( Envelope, TmrSwConfig,
    [](){ return TmrSwConfigInstance.Save(); } );
 }
