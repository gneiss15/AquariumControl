// TJsonHistoryFile.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonHistoryFile) || defined(Do_THttpHistoryManifest)
JD_BEGIN( TJsonHistoryFile, "One CSV part in the streamed history bundle" )
  JD( TGNString, Id, Required, NonEmpty, "Multipart content identifier", Live )
  JD( TGNString, Prof, Required, NonEmpty, "Aquarium profile identifier", Live )
  JD( TGNString, Kind, Required, Choice(Temperature,Energy,Pi), "CSV kind", Live )
  JD( double, Setpoint, Required, Any, "Historical water setpoint", Live )
JD_END( TJsonHistoryFile )

#endif
