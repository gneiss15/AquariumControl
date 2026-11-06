# test_definition_expansion.py, Version: 1.01
"""Compile repeated real definition includes directly, without the Python generator."""
from pathlib import Path
import subprocess
import tempfile

Library = Path(__file__).resolve().parents[1]
Program = r'''
#include <GnBasics.h>
#include <MessageTypes.h>
#include <type_traits>
#include <vector>
#define ArrayOf(Type) std::vector<Type>
#define JD_BEGIN(Name, Description) struct Name {
#define JD(Type, Member, Presence, Rule, Description, Part) Type Member;
#define JD_CONST(Type, Member, Value) Type Member=Value;
#define JD_END(Name) };
#define DoMsg_Request
#include "MessageDefinition.inc.h"
#undef ArrayOf
static_assert(std::is_same_v<decltype(TUdpRequestMessage::ID),std::vector<TChannelId>>);
constexpr unsigned InfoFields=0
#define JD(...) +1
#define DoMsg_Info
#include "MessageDefinition.inc.h"
;
static_assert(InfoFields==4);
#ifdef JD
#error Field hook leaked
#endif
#ifdef Do_TUdpInfoMessage
#error Selection leaked
#endif
enum class Selected {
#define JD(...)
#define JD_MESSAGE(Type, Tag, Value) Tag=static_cast<unsigned>(Value),
#define Do_Dashboard_TX
#include "MessageDefinition.inc.h"
};
static_assert(static_cast<unsigned>(Selected::Request)==static_cast<unsigned>(TMsgType::Request));
static_assert(static_cast<unsigned>(Selected::SourceInfo)==static_cast<unsigned>(TMsgType::SourceInfo));
int main(){TUdpRequestMessage Request;return Request.Type==TMsgType::Request && Request.Ver==3 ? 0:1;}
'''
with tempfile.TemporaryDirectory(prefix="aq-include-") as Directory:
    Root = Path(Directory)
    Source = Root / "test.cpp"
    Source.write_text(Program)
    Command = ["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-I"+str(Library), "-I"+str(Library.parent/"GnBasicLib")]
    subprocess.run(Command+[str(Source), "-ljson-c", "-o", str(Root/"test")], check=True)
    subprocess.run([str(Root/"test")], check=True)
    Source.write_text(chr(10).join(['#define DoMsg_Request', '#include "MessageDefinition.inc.h"', '']))
    Result = subprocess.run(Command+["-fsyntax-only", str(Source)], capture_output=True, text=True)
    assert Result.returncode != 0 and "Define JD before" in Result.stderr
print("Direct C++ expansion, optional hooks, TX subset, repeated includes and required JD passed")
