# PrepareSources.py, Version: 1.08
"""Generate target-only wrappers; do not modify common rescue sources."""
import pathlib
import sys

import sys as _generated_sys
from pathlib import Path as _GeneratedPath
_generated_sys.path.insert(0, str(next(parent / "tools" for parent in _GeneratedPath(__file__).resolve().parents if (parent / "tools/SyncGeneratedFiles.py").is_file())))
from SyncGeneratedFiles import write_text_if_changed, write_bytes_if_changed
Root=pathlib.Path(__file__).resolve().parent
Prjs=Root.parent.parent.parent
Out=pathlib.Path(sys.argv[1]);Out.mkdir(parents=True,exist_ok=True)
Sketch=(Prjs/'Libs/Common/EspRescue/EspRescueUpdate.ino').read_text()
for Before, After in [('if( EspRescueHardware::StatusLedPin >= 0 )', 'if( EspRescueHardware::StatusLedPin >= 0 && ShellyRollbackGeometryValid() )'), ('if( EspRescueHardware::StatusLedPin < 0 )', 'if( EspRescueHardware::StatusLedPin < 0 || !ShellyRollbackGeometryValid() )')]:
    assert Sketch.count(Before) == 1, Before
    Sketch = Sketch.replace(Before, After)
write_text_if_changed(Out/'EspRescueUpdate.cpp', '#include "RollbackService.h"\n' + Sketch)
Source=(Prjs/'Libs/Common/EspRescue/EspRescueUpdater.cpp').read_text()
Needle='void TEspRescueUpdater::HandleMain( int ClientSocket )\n {'
assert Source.count(Needle)==1
Source=Source.replace(Needle,Needle+'''
  if( const char * Error = ShellyMainActivationError() )
   {
    SendText(ClientSocket, 400, Error);
    return;
   }''')
Needle = '  if( !IsPost && (strcmp( Path, "/" ) == 0 ||'
assert Source.count(Needle) == 1
Source = Source.replace(Needle, '  if( ShellyRollbackHandle(ClientSocket, Path, IsPost, ContentLength) ) return true;\n' + Needle)
Needle = 'bool TEspRescueUpdater::HandleUpdate('
Start = Source.index(Needle)
Body = Source.index('\n {', Start)
Source = Source[:Body+3] + '\n  if( ShellyRollbackBlockUpdates() ) { SendText(ClientSocket, 400, "Geometry or uncertain flash state rejected; destructive uploads locked"); return false; }' + Source[Body+3:]
Needle = 'void TEspRescueUpdater::RestoreRescueBoot(void)\n {'
assert Source.count(Needle) == 1
Source = Source.replace(Needle, Needle + '\n  if( !ShellyRollbackGeometryValid() || MigrationUncertain() ) return;')
Needle = 'void TEspRescueUpdater::HandleMain( int ClientSocket )\n {'
Source = Source.replace(Needle, Needle + '\n  if( ShellyRollbackBlockUpdates() ) { SendText(ClientSocket, 400, "Geometry or uncertain flash state rejected; Main activation locked"); return; }')
Source = Source.replace('"<body>', '"<body>')
Needle = 'void TEspRescueUpdater::SendRecoveryPage( int ClientSocket )\n {'
# The normal page includes a discoverable link to the independent rollback page.
Link = '    "<!doctype html>'
assert Source.count(Link) == 1
Source = Source.replace(Link, Link)
Needle = '    "<div class=\'f\'><label for=\'fw\'>'
assert Source.count(Needle) == 1
Source = Source.replace(Needle, '    "<p><a href=/rollback>Migration completion and diagnostics</a></p>"\n' + Needle)
write_text_if_changed(Out/'EspRescueUpdater.cpp', '#include "ActivationGate.h"\n#include "RollbackService.h"\n#include "Migration.h"\n' + Source)
print('Generated shared rescue plus bounded target activation hooks')
