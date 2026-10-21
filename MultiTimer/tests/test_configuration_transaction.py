#!/usr/bin/env python3
# test_configuration_transaction.py, Version: 1.00
"""Compile/run the actual native transaction helper against interrupted temporary file sets."""
import pathlib
import subprocess
import tempfile

source=pathlib.Path(__file__).resolve().parents[1]
program=r'''
#include "ConfigurationTransaction.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace ConfigurationTransaction;
void Check(bool Value){if(!Value)throw std::runtime_error("transaction check failed");}
std::string Read(std::filesystem::path const & P){std::ifstream Stream(P);return {std::istreambuf_iterator<char>(Stream),{}};}
int main(int argc,char ** argv){
 (void)argc;std::filesystem::path Root=argv[1];TGNString Error;
 Check(Commit(Root,"{\"Channels\":[]}","{\"SwitchTimes\":[],\"Intervals\":[]}",Error));
 Check(Read(Root/"Channels.json")=="{\"Channels\":[]}");
 Check(!std::filesystem::exists(Root/"Configuration.transaction.json"));
 // Model a crash after the journal was committed and one target was updated.
 Check(Write(Root/"Configuration.transaction.json","{\"Files\":{\"Channels.json\":\"NEW CHANNELS\",\"SwitchTimes.json\":\"NEW TIMES\"}}",Error));
 Check(Write(Root/"Channels.json","NEW CHANNELS",Error));
 Check(Recover(Root,Error));Check(Read(Root/"SwitchTimes.json")=="NEW TIMES");
 Check(Read(Root/"Channels.json")=="NEW CHANNELS");
 // A failed target write retains the journal and can be resumed deterministically.
 std::filesystem::remove(Root/"Channels.json");std::filesystem::create_directory(Root/"Channels.json");
 Check(!Commit(Root,"FINAL CHANNELS","FINAL TIMES",Error));
 Check(std::filesystem::exists(Root/"Configuration.transaction.json"));
 std::filesystem::remove(Root/"Channels.json");Check(Recover(Root,Error));
 Check(Read(Root/"Channels.json")=="FINAL CHANNELS");Check(Read(Root/"SwitchTimes.json")=="FINAL TIMES");
 // Incomplete/corrupt journals never silently activate a mixed configuration.
 Check(Write(Root/"Configuration.transaction.json","{\"Files\":{\"Channels.json\":\"BAD\"}}",Error));
 Check(!Recover(Root,Error));Check(Read(Root/"Channels.json")=="FINAL CHANNELS");
 std::cout<<"Passed durable commit, interrupted multi-file recovery, failed writes/retry, incomplete journal rejection.\n";
}
'''
with tempfile.TemporaryDirectory() as d:
 root=pathlib.Path(d);(root/'data').mkdir();(root/'test.cpp').write_text(program)
 subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-I'+str(source/'include'),'-I'+str(source.parent/'Libs/GnBasicLib'),str(root/'test.cpp'),'-ljson-c','-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test'),str(root/'data')],check=True)
