// TJsonHistorySeries.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonHistorySeries) || defined(Do_THttpHistoryReply)
JD_BEGIN( TJsonHistorySeries, "Legacy JSON series for one aquarium profile" )
  JD( TGNString, Id, Required, NonEmpty, "Aquarium profile identifier", Live )
  JD( ArrayOf(TJsonHistoryTemperatureRow), Temp, Required, Any, "Temperature samples", Live )
  JD( ArrayOf(TJsonHistoryEnergyRow), Energy, Required, Any, "Energy samples", Live )
  JD( ArrayOf(TJsonHistoryHeatRow), Heat, Required, Any, "Pi samples", Live )
JD_END( TJsonHistorySeries )

#endif
