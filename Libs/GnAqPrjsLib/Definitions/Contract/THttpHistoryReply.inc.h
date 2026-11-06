// THttpHistoryReply.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_THttpHistoryReply)
JD_BEGIN( THttpHistoryReply, "Legacy JSON history API response" )
  JD_CONST( uint32_t, Ver, 1 )
  JD( TGNString, Range, Required, Choice(1h,6h,1day,7days,all), "Selected history range", Live )
  JD( TGNString, From, Required, Revision, "UTC range start", Live )
  JD( TGNString, To, Required, Revision, "UTC range end", Live )
  JD( TGNString, AvailableFrom, Required, Revision, "First available active PI record", Live )
  JD( uint32_t, RetentionDays, Required, Range(1,366), "View retention window", Live )
  JD( ArrayOf(TJsonHistorySeries), Profs, Required, Any, "Aquarium history series", Live )
JD_END( THttpHistoryReply )

#endif
