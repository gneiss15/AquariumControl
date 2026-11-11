# test_file_logging.py, Version: 1.01
"""The optional file sink must not disable the last-error status."""
import pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp)
 (p/'test.cpp').write_text(r'''
#include <GnBasics.h>
#include "Logger.h"
#include <cassert>
int main(int argc,char **argv) {
 assert(argc==2);Logger.Init(argv[1]);Logger.Write(TLogLevel::Error,"test error");
 TGNString message;std::time_t at=0;assert(Logger.GetLastError(message,at));
 assert(message=="test error" && at>0);
}
''')
 for enabled in (0,1):
  binary=p/f'test{enabled}';log=p/f'log{enabled}'
  subprocess.run(['g++','-std=c++20','-DSYSTEM_DUMMY',f'-DGN_DEBUG_FILE_LOG={enabled}',
    '-I'+str(root/'include'),'-I'+str(root.parent/'Libs/GnBasicLib'),'-I'+str(root.parent/'Libs/GnAqPrjsLib'),str(p/'test.cpp'),
    str(root/'src/Logger.cpp'),'-lpthread','-ljson-c','-o',str(binary)],check=True)
  subprocess.run([str(binary),str(log)],check=True)
  assert log.exists()==bool(enabled)
  if enabled:assert '[ERROR] test error' in log.read_text()
print('Optional file sink and retained RAM error reporting passed')
