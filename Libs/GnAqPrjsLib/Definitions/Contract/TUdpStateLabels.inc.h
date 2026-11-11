// TUdpStateLabels.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TUdpStateLabels) || defined(Do_TUdpChannel) || defined(Do_TUdpStatusData) || defined(Do_TUdpStatusMessage) || defined(Do_TWsStatusEvent) || defined(Do_TJsonCachedStatus) || defined(Do_TSseStatusBatch)
JD_BEGIN( TUdpStateLabels, "Translation keys for specialized boolean states. The sender supplies stable keys; the receiving browser translates them using its selected language. The feeder sends FeederRunning and FeederReady." )
  JD( TGNString, True, Optional, Any, "", Live )
  JD( TGNString, False, Optional, Any, "", Live )
JD_END( TUdpStateLabels )

#endif
