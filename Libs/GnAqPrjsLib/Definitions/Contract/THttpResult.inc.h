// THttpResult.inc.h, Version: 1.02
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_THttpResult)
JD_BEGIN( THttpResult, "HTTP operation result" )
  JD( bool, Ok, Required, Any, "Operation success", Live )
  JD( TGNString, Err, Optional, Any, "Failure description", Live )
  JD( TGNString, Reason, Optional, Any, "Conflict or outcome reason", Live )
  JD( bool, Changed, Optional, Any, "Whether saved configuration changed", Live )
  JD( bool, Pending, Optional, Any, "Whether assignment distribution is still pending", Live )
JD_END( THttpResult )

#endif
