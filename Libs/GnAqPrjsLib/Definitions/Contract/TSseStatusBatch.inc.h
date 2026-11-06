// TSseStatusBatch.inc.h, Version: 1.00
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TSseStatusBatch)
JD_VALUE( TSseStatusBatch, ArrayOf(TJsonCachedStatus), Any, "List of changed or removed cached device states, pushed by the Dashboard to its browser clients over SSE. A new connection receives its initial cache snapshot. Fresh UDP data then arrives on the normal publication schedule." )

#endif
