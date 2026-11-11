// main.cpp, Version: 1.00
#include "Arduino.h"
#include "sdkconfig.h"

static_assert( CONFIG_PARTITION_TABLE_OFFSET == 0x8000, "Wrong Nous partition table offset" );

void setup(void);
void loop(void);

extern "C" void app_main(void)
 {
  setup();
  for(;;)
   {
    loop();
    delay( 1 );
   }
 }
