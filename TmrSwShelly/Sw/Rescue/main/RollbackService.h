// RollbackService.h, Version: 1.00
#pragma once
#include <stdint.h>
void ShellyRollbackConfigure(bool GeometryValid);
bool ShellyRollbackGeometryValid(void);
bool ShellyRollbackBlockUpdates(void);
bool ShellyRollbackHandle(int Socket, const char * Path, bool Post, uint32_t Length);
