// TJsonSyncMeta.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonSyncMeta)
JD_BEGIN( TJsonSyncMeta, "Persistent configuration synchronization state" )
  JD( TGNString, TimeStamp, Required, Any, "Revision of the synchronized data", Live )
  JD( bool, Synced, Required, Any, "Whether the peer has acknowledged this revision", Live )
JD_END( TJsonSyncMeta )

#endif
