// TSseRecordingEvent.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TSseRecordingEvent)
JD_BEGIN( TSseRecordingEvent, "Configuration SSE snapshot or targeted patch" )
  JD( bool, Snapshot, Optional, Any, "Replace the cache before applying entries", Live )
  JD( MapOf(VariantOf(TJsonProfile,TJsonNull)), Profs, Optional, RequiredIf(Snapshot,Any), "Changed profiles; null removes an entry", Live )
  JD( MapOf(VariantOf(TJsonChannelGroup,TJsonNull)), Chs, Optional, RequiredIf(Snapshot,Any), "Changed device channels; null removes an entry", Live )
  JD( TJsonRegistryView, Registry, Optional, RequiredIf(Snapshot,Any), "Changed registry; null clears it", Live )
JD_END( TSseRecordingEvent )

#endif
