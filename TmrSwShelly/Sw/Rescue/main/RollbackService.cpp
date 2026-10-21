// RollbackService.cpp, Version: 1.01
#include "RollbackService.h"
#include "Migration.h"
#include "esp_random.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include <cstring>
#include "lwip/sockets.h"
extern const uint8_t NewTable[] asm("_binary_NewTable_bin_start");
static char Token[33];
static bool GeometryValid;
// Production Rescue follows the user-accepted completion of migration.
// Legacy checkpoints remain untouched; no Stock rollback is offered afterwards.
static constexpr bool MigrationCompletionAccepted = true;
static bool RollbackWritten;
void ShellyRollbackConfigure(bool Valid)
 {
  GeometryValid = Valid;
  uint8_t Random[16]; esp_fill_random(Random, sizeof(Random));
  for( unsigned I = 0; I < sizeof(Random); ++I ) snprintf(Token + 2 * I, 3, "%02x", Random[I]);
 }
bool ShellyRollbackGeometryValid(void) { return GeometryValid; }
bool ShellyRollbackBlockUpdates(void)
 {
  return !GeometryValid || MigrationUncertain() || RollbackWritten || (!MigrationCompletionAccepted && MigrationCheckpointPresent());
 }
static void Reply(int Socket, int Code, const char * Type, const char * Text)
 {
  char Header[200];
  const int Size = snprintf(Header, sizeof(Header), "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
    Code, Code == 200 ? "OK" : "Bad Request", Type, unsigned(strlen(Text)));
  // Socket writes may be partial, even on small diagnostic responses.
  const char * Parts[] = { Header, Text };
  const unsigned Lengths[] = { unsigned(Size), unsigned(strlen(Text)) };
  for( unsigned P = 0; P < 2; ++P )
    for( unsigned Sent = 0; Sent < Lengths[P]; )
     {
      int Count = send(Socket, Parts[P] + Sent, Lengths[P] - Sent, 0);
      if( Count <= 0 ) return;
      Sent += unsigned(Count);
     }
 }
static void DrainBody(int Socket, uint32_t Length)
 {
  // Consume bounded rejected bodies so TCP close does not discard the error response.
  if( Length > MIG_MAIN_SIZE + MIG_FS_SIZE + 0x10000 ) return;
  char Buffer[512];
  while( Length )
   {
    const unsigned Want = Length < sizeof(Buffer) ? Length : sizeof(Buffer);
    const int Count = recv(Socket, Buffer, Want, 0);
    if( Count <= 0 ) return;
    Length -= unsigned(Count);
   }
 }
bool ShellyRollbackHandle(int Socket, const char * Path, bool Post, uint32_t Length)
 {
  if( Post && ShellyRollbackBlockUpdates() &&
      (!strcmp(Path, "/Firmware") || !strcmp(Path, "/Filesystem") || !strcmp(Path, "/Main") || !strcmp(Path, "/Commit")) )
   {
    DrainBody(Socket, Length);
    Reply(Socket, 400, "text/plain", "Geometry or uncertain flash state rejected; updates and Main activation locked");
    return true;
   }
  if( !Post && !strcmp(Path, "/rollback") )
   {
    Reply(Socket, 200, "text/html; charset=utf-8",
      "<!doctype html><html><head><meta charset='utf-8'><title>Rescue Status</title></head><body><h1>Rescue: Migration abgeschlossen</h1>"
      "<p>Main und LittleFS koennen nach Signatur- und Geraetetyppruefung aktualisiert werden. Kein Stock-Rollback mehr.</p>"
      "<a href='/'>Zur Update-Seite</a><pre id='status'></pre><script>fetch('/rollback-info').then(r=>r.json()).then(s=>{delete s.ActionToken;"
      "document.getElementById('status').textContent=JSON.stringify(s,null,2);});</script></body></html>");
    return true;
   }
  if( !Post && !strcmp(Path, "/rollback-info") )
   {
    const bool Ready = !MigrationCompletionAccepted && GeometryValid && MigrationRollbackReady(NewTable);
    const bool RestartReady = !MigrationCompletionAccepted && GeometryValid && MigrationRollbackRestartReady(NewTable);
    char Json[900];
    snprintf(Json, sizeof(Json), "{\"Application\":\"ShellyRescue\",\"MigrationComplete\":%s,\"DestructiveUpdatesLocked\":%s,\"RollbackReady\":%s,\"RestartReady\":%s,\"GeometryVerified\":%s,\"DoNotRestart\":%s,\"LastCompletedStep\":\"%s\",\"Error\":\"%s\",\"ActionToken\":\"%s\"}",
      MigrationCompletionAccepted && GeometryValid ? "true" : "false", ShellyRollbackBlockUpdates() ? "true" : "false",
      Ready ? "true" : "false", RestartReady ? "true" : "false", GeometryValid ? "true" : "false",
      !GeometryValid || MigrationUncertain() ? "true" : "false", MigrationCompletionAccepted && GeometryValid ? "MigrationCompletedProductionRescue" : Ready ? "RescueRunningOriginalBaselineVerified" : MigrationStep(), !GeometryValid ? "Rescue geometry or NVS rejected; diagnostic mode" : MigrationUncertain() ? MigrationError() : MigrationCompletionAccepted || Ready || RestartReady ? "" : MigrationError(), Token);
    Reply(Socket, 200, "application/json", Json); return true;
   }
  if( strcmp(Path, "/rollback-write") && strcmp(Path, "/rollback-restart") ) return false;
  if( MigrationCompletionAccepted )
   {
    if( Post ) DrainBody(Socket, Length);
    Reply(Socket, 400, "text/plain", "Migration completed; Stock rollback is disabled");
    return true;
   }
  if( Post && Length != 32 ) DrainBody(Socket, Length);
  char Provided[33] = {};
  unsigned Got = 0;
  if( Post && Length == 32 )
    while( Got < 32 )
     {
      int Count = recv(Socket, Provided + Got, 32 - Got, 0);
      if( Count <= 0 ) break;
      Got += unsigned(Count);
     }
  if( !Post || Got != 32 || strcmp(Provided, Token) || !GeometryValid || MigrationUncertain() )
   { Reply(Socket, 400, "text/plain", "Invalid token, geometry or uncertain flash state"); return true; }
  if( !strcmp(Path, "/rollback-write") )
   {
    const bool Ok = MigrationRollback(NewTable);
    if( Ok ) RollbackWritten = true;
    Reply(Socket, Ok ? 200 : 400, "text/plain", Ok ? "Original table and SH0S verified; no automatic restart" : MigrationError());
   }
  else
   {
    const bool Ok = MigrationRollbackRestartReady(NewTable);
    Reply(Socket, Ok ? 200 : 400, "text/plain", Ok ? "Original-layout restart verified" : MigrationError());
    if( Ok ) { vTaskDelay(pdMS_TO_TICKS(2000)); esp_restart(); }
   }
  return true;
 }
