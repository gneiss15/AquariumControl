// Esp32Infos.h, Version: 1.03
#pragma once

#include <GnPlatform.h>

#if defined( GNPlatformESP32 )

#include <GnBasics.h>

extern String GetPartitionInfos(void);
extern String GetFlashInfos(void);
extern String GetAllLittleFSFiles(void);

#endif // ESP32
