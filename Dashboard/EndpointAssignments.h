// EndpointAssignments.h, Version: 1.00
#pragma once
#include <GnBasics.h>
#include "DeviceRegistry.h"

json_object * EndpointView( TDeviceRegistry const & Registry );
bool SwapEndpoints( TDeviceRegistry & Registry, TGNString const & First, TGNString const & Second, TGNString & Error );
