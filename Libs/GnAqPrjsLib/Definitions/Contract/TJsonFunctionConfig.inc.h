// TJsonFunctionConfig.inc.h, Version: 1.01
// Repeated definition expansion.
#include "../../GnJsonMacroDefaults.inc.h"

#if defined(Do_ContractAll) || defined(Do_TJsonFunctionConfig) || defined(Do_TJsonFunctionEnvelope) || defined(Do_THttpTimePayload) || defined(Do_THttpTimeReply)
JD_BEGIN( TJsonFunctionConfig, "Function-bound values transferred between hardware devices" )
  JD( TGNString, Hostname, Required, TextLength(1,63), "Network host name", Live )
  JD( TGNString, ApSsid, Required, TextLength(0,32), "Name of the device access point", Live )
  JD( TGNString, ApPw, Required, EmptyOrLength(8,63), "Password for the device access point", Live )
  JD( TGNString, FtpPw, Required, TextLength(0,1024), "Password for the device FTP service", Live )
JD_END( TJsonFunctionConfig )

#endif
