// GnMemory.h, Version: 1.00
#pragma once

#include <GnPlatform.h>
#include <stddef.h>

bool GnHasHeapSpace( size_t RequiredBytes, size_t LargestBlockBytes = 0 );
