# test_transaction_filename.py, Version: 1.00
# Execute the actual ESP write helper against a legacy 32-character filesystem stub.
from pathlib import Path
import subprocess, tempfile
source = Path(__file__).resolve().parents[1]/'TmrSwTimeSync.cpp'
text = source.read_text()
start = text.index('bool WriteConfiguration(')
end = text.index('\nbool MaterializeConfiguration(', start)
helper = text[start:end]
program = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
class String: public std::string {
 public: using std::string::string;
 String(std::string const & s):std::string(s){}
};
struct File {
 std::string * Text = nullptr;
 bool ShortWrite = false;
 operator bool() const { return Text != nullptr; }
 size_t write(uint8_t const * Data, size_t Size) {
  if(ShortWrite) return 0;
  Text->assign(reinterpret_cast<char const *>(Data), Size); return Size;
 }
 void flush(){} void close(){}
};
struct TFs {
 std::map<std::string,std::string> Files;
 bool ShortWrite=false, RenameFailure=false;
 bool Allowed(char const * Path) { std::string P=Path; return P.substr(P.find_last_of('/')+1).size()<=32; }
 File open(char const * Path, char const *) {
  if(!Allowed(Path)) return {};
  Files[Path]=""; return {&Files[Path],ShortWrite};
 }
 bool rename(char const * From,char const * To) {
  if(RenameFailure || !Allowed(From) || !Allowed(To)) return false;
  Files[To]=Files.at(From); Files.erase(From); return true;
 }
} LittleFS;
'''+helper+r'''
int main() {
 // Old temporary name cannot be opened on the legacy filesystem.
 assert(!LittleFS.open("/TimeConfig.transaction.json.txnnew","w"));
 for(auto Name:{"/TimeConfig.transaction.json","/Channels.json","/SwitchTimes.json","/Intervals.json"}) {
  LittleFS.Files[Name]="OLD";
  assert(WriteConfiguration(Name,String("NEW")));
  assert(LittleFS.Files.at(Name)=="NEW");
  assert(!LittleFS.Files.count(std::string(Name)+".new"));
 }
 char const * Name="/TimeConfig.transaction.json";
 LittleFS.ShortWrite=true;
 assert(!WriteConfiguration(Name,String("BROKEN")));
 assert(LittleFS.Files.at(Name)=="NEW");
 LittleFS.ShortWrite=false; LittleFS.RenameFailure=true;
 assert(!WriteConfiguration(Name,String("BROKEN")));
 assert(LittleFS.Files.at(Name)=="NEW");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 root=Path(tmp);(root/'test.cpp').write_text(program)
 subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror',str(root/'test.cpp'),'-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test')],check=True)
print('Actual write helper: legacy filename limit, replacement and failed-write/rename preservation passed.')
