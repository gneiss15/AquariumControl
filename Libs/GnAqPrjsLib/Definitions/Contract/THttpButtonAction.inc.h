// THttpButtonAction.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_THttpButtonAction) || defined(Do_THttpDevice)
JD_BEGIN( THttpButtonAction, "Channel button action endpoint" )
  JD( TGNString, Url, Required, HttpUrl, "Complete URL that invokes the button action", Live )
  JD( ArrayOf(TGNString), Presses, Required, Any, "Names of supported button press actions, for example Short and Long", Live )
JD_END( THttpButtonAction )

#endif
