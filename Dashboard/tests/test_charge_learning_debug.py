#!/usr/bin/env python3
# test_charge_learning_debug.py, Version: 1.01
"""Journal real completed events, including RAM-only steps, deduplication and restart."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=r'''#include "ChargeLearningDebug.h"
#include <cassert>
int main(int,char ** argv){
 std::filesystem::path root=argv[1];
 TGnWireDocument d;auto p=d.Root();p.Text("ID","AA:BB:CC:DD:EE:FF@4");
 auto s=p.NewObject("Status");s.NewObject("Identity").UInt("BootCounter",7);
 auto e=s.NewObject("ChargeLearning");e.UInt("Schema",1);e.UInt("Sequence",1);e.Text("Profile","LaptopCharge");
 e.UInt("BeforePercent",40);e.UInt("AfterPercent",50);e.UInt("AppliedSecondsPerPercent",60);
 e.UInt("NewSecondsPerPercent",59);e.UInt("ChargeMilliseconds",600000);e.Text("Persistence","RamOnly");
 ChargeLearningDebug::Receive(root,p.V);ChargeLearningDebug::Receive(root,p.V);
 TGnWireDocument first(ChargeLearningDebug::Read(root));assert(first.Root().Items().size()==1);
 assert(first.Root().Items()[0].Get("Synchronization").Text()=="NotRequired");
 e.UInt("Sequence",2);e.Text("Persistence","Saved");ChargeLearningDebug::Receive(root,p.V);
 TGnWireDocument second(ChargeLearningDebug::Read(root));assert(second.Root().Items().size()==2);
 assert(second.Root().Items()[1].Get("Synchronization").Text()=="NotObserved");
 s.Get("Identity").UInt("BootCounter",8);ChargeLearningDebug::Receive(root,p.V);
 TGnWireDocument third(ChargeLearningDebug::Read(root));assert(third.Root().Items().size()==3);
 e.UInt("Sequence",3);e.UInt("AfterPercent",39);ChargeLearningDebug::Receive(root,p.V);
 assert(TGnWireDocument(ChargeLearningDebug::Read(root)).Root().Items().size()==3);
 // Retention is bounded without generating 1000 fake network events.
 auto * rows=json_object_new_array();
 for(int i=0;i<1000;++i){auto * row=json_object_new_object();json_object_object_add(row,"CycleId",json_object_new_int(i));json_object_array_add(rows,row);}
 std::ofstream(ChargeLearningDebug::File(root))<<json_object_to_json_string(rows);json_object_put(rows);
 e.UInt("AfterPercent",51);e.UInt("Sequence",4);ChargeLearningDebug::Receive(root,p.V);
 TGnWireDocument bounded(ChargeLearningDebug::Read(root));assert(bounded.Root().Items().size()==1000);
 assert(bounded.Root().Items().back().Get("BeforePercent").UInt()==40);
 std::ofstream(ChargeLearningDebug::File(root))<<"corrupt";
 e.UInt("AfterPercent",51);ChargeLearningDebug::Receive(root,p.V);
 assert(ChargeLearningDebug::Read(root)=="corrupt");
}'''
with tempfile.TemporaryDirectory() as t:
 t=Path(t);(t/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root.parent/'Libs/GnBasicLib'),'-I'+str(root.parent/'Libs/GnAqPrjsLib'),str(t/'test.cpp'),'-ljson-c','-o',str(t/'test')],check=True)
 subprocess.run([str(t/'test'),str(t/'journal')],check=True)
print('Charge learning debug journal tests passed')
