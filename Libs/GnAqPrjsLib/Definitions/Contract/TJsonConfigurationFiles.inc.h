// TJsonConfigurationFiles.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonConfigurationFiles)
JD_BEGIN( TJsonConfigurationFiles, "Local recovery journal with complete replacement file contents." )
  JD( TGNString, Chs, Required, Any, "Channel configuration text", Live )
  JD( TGNString, Schedule, Required, Any, "Switching schedule text", Live )
  JD( TGNString, Intervals, Optional, Any, "Separate TimerSwitch interval configuration text", Live )
JD_END( TJsonConfigurationFiles )
#endif
