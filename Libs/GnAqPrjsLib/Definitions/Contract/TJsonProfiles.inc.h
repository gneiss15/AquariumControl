// TJsonProfiles.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonProfiles)
JD_BEGIN( TJsonProfiles, "Central profile file" )
  JD( ArrayOf(TJsonProfile), Profs, Required, Any, "Profiles published by the central configuration service", Live )
  JD( TGNString, TimeStamp, Optional, Revision, "Revision of the profile collection", Live )
JD_END( TJsonProfiles )

#endif
