// GnFunctionConfig.inc.h, Version: 1.00
// Only these Config.json fields follow a function during a device swap.
// All other fields are device-bound and must remain on the target hardware.
#ifndef GN_FUNCTION_CONFIG_FIELD
#error Define GN_FUNCTION_CONFIG_FIELD before including this file.
#endif
GN_FUNCTION_CONFIG_FIELD( Hostname )
GN_FUNCTION_CONFIG_FIELD( ApSsid )
GN_FUNCTION_CONFIG_FIELD( ApPw )
GN_FUNCTION_CONFIG_FIELD( FtpPw )
#undef GN_FUNCTION_CONFIG_FIELD
