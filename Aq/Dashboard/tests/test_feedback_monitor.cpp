// test_feedback_monitor.cpp, Version: 1.01
#include <GnBasics.h>
#include "../FeedbackMonitor.h"
#include <cassert>
#include <string>

json_object * Parse( char const * Text ) { return json_tokener_parse( Text ); }
void Configure( TFeedbackMonitor & Monitor )
 {
  auto C = Parse(R"({"Version":1,"Monitors":[{"Id":"Power","SwitchFunction":"TestSwitch","Source":"Battery","StatusKind":"Power","FeedbackField":"AcOnline","GraceSeconds":60}]})");
  Monitor.Configure( C ); json_object_put( C );
  auto R = Parse(R"({"Devices":[],"ExternalSources":{"Battery":"02:11:22:33:44:55@3"},"Endpoints":[{"Function":"TestSwitch","DeviceId":"02:22:33:44:55:66@4","Capabilities":1,"Index":0}]})");
  Monitor.Bind( R ); json_object_put( R );
 }
void Send( TFeedbackMonitor & Monitor, uint64_t Now, bool On, char const * Feedback )
 {
  std::string S = std::string(R"({"ID":"02:22:33:44:55:66@4","Status":{"Channels":[{"Id":0,"State":)") + ( On ? "true" : "false" ) + "}]}}";
  auto P = Parse( S.c_str() ); Monitor.Receive( P, Now ); json_object_put( P );
  S = std::string(R"({"ID":"02:11:22:33:44:55@3","Status":{"Kind":"Power","AcOnline":)") + Feedback + "}}";
  P = Parse( S.c_str() ); Monitor.Receive( P, Now ); json_object_put( P );
 }
void Expect( TFeedbackMonitor & Monitor, uint64_t Now, char const * Expected )
 {
  auto S = Monitor.Status( Now ); auto R = json_object_array_get_idx( S, 0 ); json_object * State = nullptr;
  assert( json_object_object_get_ex( R, "State", &State ) ); assert( std::string( json_object_get_string( State ) ) == Expected ); json_object_put( S );
 }
void Reason( TFeedbackMonitor & Monitor, uint64_t Now, char const * Expected )
 {
  auto S = Monitor.Status( Now ); auto R = json_object_array_get_idx( S, 0 ); json_object * Value = nullptr;
  assert( json_object_object_get_ex( R, "Reason", &Value ) ); assert( std::string( json_object_get_string( Value ) ) == Expected ); json_object_put( S );
 }
int main(void)
 {
  TFeedbackMonitor M; Configure( M ); Expect( M, 0, "Unknown" );
  Send( M, 1000, false, "false" ); Expect( M, 1000, "OK" );
  for( uint64_t T = 2000; T < 62000; T += 5000 ) { Send( M, T, true, "false" ); Expect( M, T, "Pending" ); }
  Send( M, 62000, true, "false" ); Expect( M, 62000, "Mismatch" );
  Send( M, 63000, true, "true" ); Expect( M, 63000, "OK" );
  Send( M, 64000, true, "false" ); Expect( M, 64000, "Pending" );
  Send( M, 65000, false, "true" ); Expect( M, 65000, "Pending" );
  Expect( M, 85001, "Unknown" ); // Neither stale boolean may be treated as OFF.
  Send( M, 90000, false, "null" ); Expect( M, 90000, "Unknown" ); Reason( M, 90000, "InvalidFeedback" );
  Send( M, 91000, false, "0" ); Expect( M, 91000, "Unknown" ); Reason( M, 91000, "InvalidFeedback" );
  auto Missing = Parse(R"({"ID":"02:11:22:33:44:55@3","Status":{"Kind":"Power"}})");
  M.Receive( Missing, 91500 ); json_object_put( Missing ); Reason( M, 91500, "MissingFeedback" );
  Send( M, 92000, false, "false" ); Expect( M, 92000, "OK" );
  // Recovery between browser requests must also reset continuous mismatch time.
  Send( M, 93000, true, "false" ); Send( M, 94000, true, "true" ); Send( M, 95000, true, "false" );
  Expect( M, 95000, "Pending" );
  for( uint64_t T = 100000; T < 155000; T += 5000 ) Send( M, T, true, "false" );
  Expect( M, 150000, "Pending" ); Send( M, 155000, true, "false" ); Expect( M, 155000, "Mismatch" );
  Configure( M ); Expect( M, 156000, "Unknown" ); Reason( M, 156000, "MissingSwitch" ); // Mapping changes require fresh independent data.
 }
