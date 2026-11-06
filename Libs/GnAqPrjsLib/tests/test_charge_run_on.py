#!/usr/bin/env python3
# test_charge_run_on.py, Version: 1.06
"""Compile the real RAM state machine and profile-field checks without hardware."""
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
program = r'''

#include <GnChargeRunOn.h>
#include <GnChargeProfileJson.h>
#include <cassert>
#include <cmath>
int main() {
 uint32_t learned=0;
 TGnChargeRunOn c;
 TGnChargeRunOn::TLearningStep step;
 auto update=[&](uint64_t now,bool valid,float value,unsigned age,bool on,bool automatic,uint32_t boot=10,bool learning=true,bool known=true,bool allowed=true) {
   return c.Update("Laptop",80,60,now,valid,!valid,value,age,on,automatic,allowed,learning,boot,known);
 };
 assert(!update(0,false,0,30000,true,false)); // Startup alone never arms.
 assert(update(1000,true,70,0,true,true));
 assert(update(11000,true,70,10000,true,true)); // Same cached packet.
 assert(update(22000,false,0,21000,true,false));
 assert(update(600999,false,0,599999,true,false));
 assert(!update(601000,false,0,600000,true,false));
 assert(!update(900000,true,80,0,false,false,11));
 assert(!c.TakeLearned(learned)); // Factor unchanged, no write.
 assert(c.TakeLearningStep(step) && step.BeforePercent==70 && step.AfterPercent==80);
 assert(step.AppliedFactor==60 && step.NewFactor==60 && step.ChargeMs==600000 && !step.SaveRequired);
 assert(!c.TakeLearningStep(step));
 for(uint32_t start : {10U, UINT32_MAX}) {
  c.Reset();
  assert(update(0,true,70,0,true,true,start));
  assert(update(21000,false,0,21000,true,false,start));
  assert(update(150000,true,75,0,true,true,start+1));
  assert(c.TakeLearningStep(step) && step.SaveRequired && step.NewFactor==52 && step.ChargeMs==150000);
  assert(c.TakeLearned(learned) && learned==52); // 52.5 floored; wrap MAX -> 0 accepted.
 }
 for(uint32_t delta : {0U,2U,3U,UINT32_MAX}) {
  c.Reset();update(0,true,70,0,true,true,UINT32_MAX);
  update(21000,false,0,21000,true,false,UINT32_MAX);
  update(150000,true,75,0,true,true,UINT32_MAX+delta);
  assert(!c.TakeLearningStep(step));
  assert(!c.TakeLearned(learned)); // Same boot / extra boots / backwards counter.
 }
 c.Reset();update(0,true,70,0,true,true);update(21000,false,0,21000,true,false);
 assert(update(30000,false,0,30000,true,false,10,false)); // Learning OFF leaves run-on running.
 update(150000,true,75,0,true,true,11,false);assert(!c.TakeLearned(learned));
 c.Reset();update(0,true,70,0,true,true);update(21000,false,0,21000,true,false);
 update(150000,true,75,0,true,true,11,true,false);assert(!c.TakeLearned(learned)); // Missing counter.
 c.Reset();update(0,true,70,0,true,true);update(21000,false,0,21000,true,false);
 assert(!update(22000,false,0,22000,true,false,10,true,true,false));
 update(150000,true,75,0,true,true,11);assert(!c.TakeLearned(learned)); // Manual abort.
 c.Reset();update(0,true,70,0,true,true);
 assert(!c.Update("Laptop",80,60,1000,false,false,0,0,true,false,true,true,10,true));
 update(150000,true,75,0,true,true,11);assert(!c.TakeLearned(learned)); // Invalid packet, not missing sender.
 c.Reset();update(0,true,70,0,false,true);
 assert(!update(21000,false,0,21000,false,false)); // No energizing an OFF output.
 c.Reset();update(0,true,70,0,true,true);update(21000,false,0,21000,true,false);
 assert(!c.Update("changed source",80,60,22000,false,true,0,22000,true,false,true));
 assert(!c.TakeLearned(learned));
 c.Reset();update(0,true,70,0,true,true);update(21000,false,0,21000,true,false);
 assert(!c.Update("Laptop",85,60,22000,false,true,0,22000,true,false,true));
 c.Reset();update(0,true,70,0,true,true);update(21000,false,0,21000,true,false);
 assert(!c.Update("Laptop",80,120,22000,false,true,0,22000,true,false,true));
 for(float result : {70.0F,69.0F}) {
  c.Reset();update(0,true,70,0,true,true);update(21000,false,0,21000,true,false);
  update(601000,false,0,601000,true,false);update(900000,true,result,0,false,true,11);
  assert(!c.TakeLearned(learned));
 }
 TGnChargeRunOn accumulated;
 uint32_t boot=0;
 auto learn=[&](uint64_t start,unsigned seconds,float gain) {
  assert(accumulated.Update("acc",100,100,start,true,false,50,0,true,true,true,true,boot));
  assert(accumulated.Update("acc",100,100,start+21000,false,true,0,21000,true,false,true,true,boot));
  assert(accumulated.Update("acc",100,100,start+seconds*1000,true,false,50+gain,0,true,true,true,true,++boot));
 };
 learn(0,116,1);assert(!accumulated.TakeLearned(learned)); // 100 -> 104, RAM only.
 assert(accumulated.Update("acc",100,100,137000,false,true,0,21000,true,false,true,true,boot));
 assert(accumulated.Update("acc",100,100,5211999,false,true,0,5095999,true,false,true,true,boot));
 assert(!accumulated.Update("acc",100,100,5212000,false,true,0,5096000,true,false,true,true,boot));
 accumulated.Update("acc",100,100,5300000,true,false,51,0,false,true,true,true,++boot);
 assert(!accumulated.TakeLearned(learned));
 learn(5400000,116,1);assert(accumulated.TakeLearned(learned) && learned==107);
 for(auto pair : {std::pair<unsigned,uint32_t>{120,105},{80,95},{119,0}}) {
  accumulated.Reset();learn(0,pair.first,1);
  bool saved=accumulated.TakeLearned(learned);assert(saved==(pair.second!=0));if(saved)assert(learned==pair.second);
 }
 auto valid=[](const char *text) {auto r=json_tokener_parse(text);bool ok=GnChargeProfileJsonValid(r);json_object_put(r);return ok;};
 assert(valid(R"({"Future":{"any":true}})"));
 assert(valid(R"({"ChargeRunOn":false,"Future":3})"));
 assert(!valid(R"({"ChargeRunOn":true,"Measurement":"BatteryPercent"})"));
 for(auto text : {R"({"ChargeSecondsPerPercent":0})", R"({"ChargeSecondsPerPercent":true})", R"({"ChargeSecondsPerPercent":15.5})", R"({"ChargeSecondsPerPercent":-1})", R"({"ChargeSecondsPerPercent":4294967296})"}) assert(!valid(text));
 assert(valid(R"({"ChargeSecondsPerPercent":4294967295})"));
 assert(valid(R"({"ChargeRunOn":true,"ChargeSecondsPerPercent":15,"Measurement":"BatteryPercent","FloorOn":40,"FloorOff":80,"MainOn":40,"MainOff":80,"Future":{"value":2}})"));
}
'''

with tempfile.TemporaryDirectory() as tmp:
    directory = pathlib.Path(tmp)
    source = directory / 'test.cpp'
    source.write_text(program)
    binary = directory / 'test'
    subprocess.run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-I'+str(root),'-I'+str(root.parent/'GnBasicLib'), str(source), '-ljson-c', '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('Charge run-on: timing, actual ON duration, learning, early return, invalid data, overrides, reassignment, reset and extensible profiles passed.')
