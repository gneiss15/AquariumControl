// main.cpp, Version: 1.00
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

constexpr int LOW = 0;
constexpr int HIGH = 1;
#if CONFIG_IDF_TARGET_ESP32C3
#include "TmrSwShellyHardware.h"
namespace Hardware = TmrSwShellyHardware;
#else
#include "TmrSwA8THardware.h"
namespace Hardware = TmrSwA8THardware;
#endif

extern "C" void app_main(void)
 {
  constexpr auto Relay = static_cast<gpio_num_t>( Hardware::RelayPin );
  constexpr auto Led = static_cast<gpio_num_t>( Hardware::StatusLedPin );
  ESP_ERROR_CHECK( gpio_set_level( Relay, !Hardware::RelayActiveLevel ) );
  ESP_ERROR_CHECK( gpio_set_direction( Relay, GPIO_MODE_OUTPUT ) );
  ESP_ERROR_CHECK( gpio_set_level( Led, !Hardware::StatusLedActiveLevel ) );
  ESP_ERROR_CHECK( gpio_set_direction( Led, GPIO_MODE_OUTPUT ) );
  for(;;)
   {
    gpio_set_level( Led, Hardware::StatusLedActiveLevel );
    vTaskDelay( pdMS_TO_TICKS( 250 ) );
    gpio_set_level( Led, !Hardware::StatusLedActiveLevel );
    vTaskDelay( pdMS_TO_TICKS( 750 ) );
   }
 }
