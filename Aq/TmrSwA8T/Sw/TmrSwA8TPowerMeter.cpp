// TmrSwA8TPowerMeter.cpp, Version: 1.08
#include "TmrSwA8TPowerMeter.h"
#include "TmrSwA8THardware.h"
#include <driver/ledc.h>
#include <driver/gpio.h>
#include <esp_intr_alloc.h>
#include <driver/pulse_cnt.h>
#include <hal/ledc_ll.h>
#include <esp_timer.h>
#include <nvs.h>

#if !CONFIG_PCNT_CTRL_FUNC_IN_IRAM || !CONFIG_PCNT_ISR_IRAM_SAFE || !CONFIG_GPIO_CTRL_FUNC_IN_IRAM || !CONFIG_ARDUINO_ISR_IRAM
#error "Hardware metering requires the IRAM settings from sdkconfig.defaults; regenerate cached sdkconfig"
#endif
#if CONFIG_PM_ENABLE
#error "Hardware metering assumes a fixed 80 MHz APB clock"
#endif

namespace
 {
/*
Hardware frequency measurement (no interrupts on CF/CF1 pulses):

  GPIO25: LEDC gate for CF1, nominally 2 s HIGH / 0.5 s LOW.
  GPIO32: LEDC gate for CF,  nominally 4 s HIGH / 1 s LOW.
  PCNT counts falling sensor edges only while its gate is HIGH. The gate
  output is read back through the same GPIO pad; no external jumper is needed.
  These GPIOs must be electrically suitable: the GPIO matrix does not provide
  a pad-independent LEDC-to-PCNT connection.

  CF1 alternates voltage and current. On each falling gate edge the counter
  is already inhibited by hardware. The gate ISR reads it, switches SEL to
  the other measurement and clears the counter during the closed interval.
  The next HIGH interval therefore starts after roughly 0.5 s of settling.
  Voltage and current each produce one result per nominal 5 s cycle. CF has
  its own uninterrupted 4 s measurement window and is read/cleared during
  its 1 s closed interval. GPIO interrupts occur only at gate closure;
  PCNT limit interrupts extend the 16-bit counter through the IDF accumulator.
  Each high-limit event adds 32767; reading returns accumulator + hardware
  remainder, and clearing resets both. No interrupt is required per pulse.

  Unlike a PIC timer with a freely programmable period register, LEDC uses
  a period of 2^resolution ticks. The timer divider has eight fractional bits,
  while the configured duty uses integer ticks. Consequently the asymmetric
  80% duty and the nominal periods cannot all be represented exactly.
  At APB = 80 MHz, divider register 195313, resolutions 18/19 and duties
  209715/419430 give periods 2.5000064/5.0000128 s and HIGH intervals
  2.0000032126/4.0000064253 s. Both periods are related by exactly 2:1.
  Frequency is count / ACTUAL HIGH duration, not count / nominal 2 or 4 s.
  Clock/quartz accuracy remains the accuracy of the measurement time base.

  Existing NVS calibration integers are preserved. The former reciprocal
  period formulas become U = Hz * 822*VoltageCal/1e7,
  I = Hz * 3300*CurrentCal/1e9 and P = Hz * 1362*PowerCal/1e7.
  Products use 64 bits before conversion; fractional Hz are retained.
  Windows spanning a relay transition or a missed gate callback
  are discarded. ISR work must complete in the closed gate interval.
*/
constexpr uint32_t HjlPowerRatio = 1362;
constexpr uint32_t HjlVoltageRatio = 822;
constexpr uint32_t HjlCurrentRatio = 3300;
constexpr uint32_t DefaultPowerCal = 12530;
constexpr uint32_t DefaultVoltageCal = 1950;
constexpr uint32_t DefaultCurrentCal = 3500;
constexpr char CalNvsNamespace[] = "A8TCal";
constexpr char VoltageCalNvsKey[] = "VoltageCal";
constexpr char CurrentCalNvsKey[] = "CurrentCal";
constexpr char PowerCalNvsKey[] = "PowerCal";
// APB clock: 80 MHz; divider register includes eight fractional bits.
constexpr uint32_t GateDivider = 195313;
constexpr uint32_t GateDuty[] = { 209715, 419430 };
constexpr double GateSeconds[] = {
  double( GateDuty[0] ) * GateDivider / ( 256.0 * 80000000.0 ),
  double( GateDuty[1] ) * GateDivider / ( 256.0 * 80000000.0 ) };
DRAM_ATTR const int GatePins[] = { 25, 32 };
constexpr int PulsePins[] = { 26, 27 };
portMUX_TYPE MeterMux = portMUX_INITIALIZER_UNLOCKED;
pcnt_unit_handle_t Units[2] = {};
volatile bool ChangedLoad[2] = {};
volatile bool Ready[2] = {};
volatile bool Good[2] = {};
volatile int Counts[2] = {};
volatile bool VoltageSample = true;
volatile bool MeasureVoltage = true;
volatile uint32_t Edges[2] = {};
int64_t LastGateUs[2] = {};

void IRAM_ATTR OnGate( void * Context )
 {
  const unsigned Index = reinterpret_cast<uintptr_t>( Context ) - 1;
  const int64_t Now = esp_timer_get_time();
  const int64_t PeriodUs = Index == 0 ? 2500006 : 5000013;
  int Count = 0;
  const bool ReadOk = pcnt_unit_get_count( Units[Index], &Count ) == ESP_OK;
  const bool Closed = gpio_get_level( gpio_num_t( GatePins[Index] ) ) == 0;
  portENTER_CRITICAL_ISR( &MeterMux );
  Good[Index] = ReadOk && Closed && Count >= 0 &&
    ( LastGateUs[Index] == 0 ||
      ( Now - LastGateUs[Index] > PeriodUs - 400000 &&
        Now - LastGateUs[Index] < PeriodUs + 400000 ) ) &&
    ( ( Index == 0 && MeasureVoltage ) || !ChangedLoad[Index] );
  Counts[Index] = Count;
  if( Good[Index] ) Edges[Index] = Edges[Index] + uint32_t( Count );
  if( Index == 0 )
   {
    VoltageSample = MeasureVoltage;
    MeasureVoltage = !MeasureVoltage;
    // Preserve the selector polarity of the existing, calibrated A8T code.
    gpio_set_level( GPIO_NUM_14, MeasureVoltage ? 1 : 0 );
   }
  if( pcnt_unit_clear_count( Units[Index] ) != ESP_OK ) Good[Index] = false;
  ChangedLoad[Index] = false;
  LastGateUs[Index] = Now;
  Ready[Index] = true;
  portEXIT_CRITICAL_ISR( &MeterMux );
 }

esp_err_t SetupGate( unsigned Index )
 {
  const auto Timer = ledc_timer_t( Index );
  ledc_timer_config_t Config = {};
  Config.speed_mode = LEDC_HIGH_SPEED_MODE;
  Config.timer_num = Timer;
  Config.duty_resolution = Index == 0 ? LEDC_TIMER_18_BIT : LEDC_TIMER_19_BIT;
  Config.freq_hz = 1;
  Config.clk_cfg = LEDC_USE_APB_CLK;
  esp_err_t Error = ledc_timer_config( &Config );
  if( Error != ESP_OK ) return Error;
  ledc_timer_pause( LEDC_HIGH_SPEED_MODE, Timer );
  ledc_ll_set_clock_divider( LEDC_LL_GET_HW(), LEDC_HIGH_SPEED_MODE, Timer, GateDivider );
  ledc_timer_rst( LEDC_HIGH_SPEED_MODE, Timer );
  ledc_channel_config_t Channel = {};
  Channel.gpio_num = GatePins[Index];
  Channel.speed_mode = LEDC_HIGH_SPEED_MODE;
  Channel.channel = ledc_channel_t( Index );
  Channel.timer_sel = Timer;
  Channel.duty = GateDuty[Index];
  // Start with the closed interval, allowing selector settling before counting.
  Channel.hpoint = ( 1U << unsigned( Config.duty_resolution ) ) - Channel.duty;
  Error = ledc_channel_config( &Channel );
  if( Error != ESP_OK ) return Error;

  pcnt_unit_config_t UnitConfig = {};
  UnitConfig.low_limit = -32768;
  UnitConfig.high_limit = 32767;
  UnitConfig.flags.accum_count = true;
  // Drain limit events before the lower-priority GPIO gate ISR reads/clears.
  UnitConfig.intr_priority = 3;
  Error = pcnt_new_unit( &UnitConfig, &Units[Index] );
  if( Error != ESP_OK ) return Error;
  pcnt_chan_config_t Input = {};
  Input.edge_gpio_num = PulsePins[Index];
  Input.level_gpio_num = GatePins[Index];
  pcnt_channel_handle_t Handle;
  Error = pcnt_new_channel( Units[Index], &Input, &Handle );
  if( Error != ESP_OK ) return Error;
  Error = pcnt_channel_set_edge_action( Handle, PCNT_CHANNEL_EDGE_ACTION_HOLD,
    PCNT_CHANNEL_EDGE_ACTION_INCREASE );
  if( Error != ESP_OK ) return Error;
  Error = pcnt_channel_set_level_action( Handle, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
    PCNT_CHANNEL_LEVEL_ACTION_HOLD );
  if( Error != ESP_OK ) return Error;
  Error = pcnt_unit_add_watch_point( Units[Index], 32767 );
  if( Error != ESP_OK ) return Error;
  Error = pcnt_unit_enable( Units[Index] );
  if( Error != ESP_OK ) return Error;
  Error = pcnt_unit_clear_count( Units[Index] );
  if( Error != ESP_OK ) return Error;
  Error = pcnt_unit_start( Units[Index] );
  if( Error != ESP_OK ) return Error;
  Error = gpio_set_intr_type( gpio_num_t( GatePins[Index] ), GPIO_INTR_NEGEDGE );
  if( Error != ESP_OK ) return Error;
  return gpio_isr_handler_add( gpio_num_t( GatePins[Index] ), OnGate,
    reinterpret_cast<void *>( uintptr_t( Index + 1 ) ) );
 }
 }

TTmrSwA8TPowerMeter::TTmrSwA8TPowerMeter(void)
: FHardwareReady( false )
, FLoadEnabled( false )
, FVoltageValid( false )
, FVoltageIntervals( 0 )
, FCurrentIntervals( 0 )
, FVoltage( 0.0f )
, FCurrent( 0.0f )
, FActivePower( 0.0f )
, FVoltageCal( DefaultVoltageCal )
, FCurrentCal( DefaultCurrentCal )
, FPowerCal( DefaultPowerCal )
 {
 }

bool TTmrSwA8TPowerMeter::Available(void) const
 {
  return TmrSwA8THardware::PowerMeterAvailable;
 }

void TTmrSwA8TPowerMeter::Setup(void)
 {
  if( !Available() || FHardwareReady ) return;
  LoadCalValues();
  // GPIO interrupts are only used for gate closure, never for sensor pulses.
  // Keep the native ISR service resident in IRAM during flash operations.
  const esp_err_t IsrError = gpio_install_isr_service( ESP_INTR_FLAG_IRAM );
  if( IsrError != ESP_OK && IsrError != ESP_ERR_INVALID_STATE )
   {
    Debug( "Power meter GPIO ISR setup failed: %s\n", esp_err_to_name( IsrError ) );
    return;
   }
  SetPin( TmrSwA8THardware::MeterSelPin );
  PinMode_Output( TmrSwA8THardware::MeterSelPin );
  for( unsigned Index = 0; Index < 2; ++Index )
   {
    const esp_err_t Error = SetupGate( Index );
    if( Error != ESP_OK )
     {
      Debug( "Power meter hardware setup failed: %s\n", esp_err_to_name( Error ) );
      for( unsigned Gate = 0; Gate <= Index; ++Gate )
       {
        gpio_isr_handler_remove( gpio_num_t( GatePins[Gate] ) );
        gpio_set_intr_type( gpio_num_t( GatePins[Gate] ), GPIO_INTR_DISABLE );
        ledc_stop( LEDC_HIGH_SPEED_MODE, ledc_channel_t( Gate ), 0 );
        gpio_set_direction( gpio_num_t( GatePins[Gate] ), GPIO_MODE_INPUT );
       }
      return;
     }
   }
  FHardwareReady = true;
  // Also reject a shortened first cycle if LEDC applies its divider at overflow.
  LastGateUs[0] = esp_timer_get_time();
  LastGateUs[1] = LastGateUs[0];
  ledc_timer_resume( LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0 );
  ledc_timer_resume( LEDC_HIGH_SPEED_MODE, LEDC_TIMER_1 );
 }

void TTmrSwA8TPowerMeter::LoadEnabled( bool Enabled )
 {
  portENTER_CRITICAL( &MeterMux );
  if( Enabled != FLoadEnabled )
   {
    ChangedLoad[0] = true;
    ChangedLoad[1] = true;
   }
  // Relay state only invalidates transition windows; it must not hide measured load.
  FLoadEnabled = Enabled;
  portEXIT_CRITICAL( &MeterMux );
 }

void TTmrSwA8TPowerMeter::LoadCalValues(void)
 {
  nvs_handle_t Handle;
  if( nvs_open( CalNvsNamespace, NVS_READONLY, &Handle ) != ESP_OK )
    return;

  uint32_t Value;
  if( nvs_get_u32( Handle, VoltageCalNvsKey, &Value ) == ESP_OK && Value > 0 )
    FVoltageCal = Value;
  if( nvs_get_u32( Handle, CurrentCalNvsKey, &Value ) == ESP_OK && Value > 0 )
    FCurrentCal = Value;
  if( nvs_get_u32( Handle, PowerCalNvsKey, &Value ) == ESP_OK && Value > 0 )
    FPowerCal = Value;
  nvs_close( Handle );
 }

bool TTmrSwA8TPowerMeter::SetCalValues( uint32_t VoltageCal, uint32_t CurrentCal, uint32_t PowerCal )
 {
  if( VoltageCal == 0 || CurrentCal == 0 || PowerCal == 0 )
    return false;

  nvs_handle_t Handle;
  if( nvs_open( CalNvsNamespace, NVS_READWRITE, &Handle ) != ESP_OK )
    return false;

  esp_err_t Error = nvs_set_u32( Handle, VoltageCalNvsKey, VoltageCal );
  if( Error == ESP_OK )
    Error = nvs_set_u32( Handle, CurrentCalNvsKey, CurrentCal );
  if( Error == ESP_OK )
    Error = nvs_set_u32( Handle, PowerCalNvsKey, PowerCal );
  if( Error == ESP_OK )
    Error = nvs_commit( Handle );
  nvs_close( Handle );
  if( Error != ESP_OK )
    return false;

  FVoltageCal = VoltageCal;
  FCurrentCal = CurrentCal;
  FPowerCal = PowerCal;
  return true;
 }

uint32_t TTmrSwA8TPowerMeter::CfEdges(void) const
 {
  portENTER_CRITICAL( &MeterMux );
  const uint32_t Count = Edges[1];
  portEXIT_CRITICAL( &MeterMux );
  return Count;
 }

uint32_t TTmrSwA8TPowerMeter::Cf1Edges(void) const
 {
  portENTER_CRITICAL( &MeterMux );
  const uint32_t Count = Edges[0];
  portEXIT_CRITICAL( &MeterMux );
  return Count;
 }

int TTmrSwA8TPowerMeter::SelectorLevel(void) const
 {
  return Available() ? GetInPin( TmrSwA8THardware::MeterSelPin ) : -1;
 }

/*virtual*/ void TTmrSwA8TPowerMeter::Loop(void)
 {
  if( !FHardwareReady ) return;
  for( unsigned Index = 0; Index < 2; ++Index )
   {
    portENTER_CRITICAL( &MeterMux );
    const bool Pending = Ready[Index];
    const int Count = Counts[Index];
    const bool Valid = Good[Index];
    const bool Voltage = VoltageSample;
    Ready[Index] = false;
    portEXIT_CRITICAL( &MeterMux );
    if( !Pending ) continue;
    const double Frequency = Valid ? double( Count ) / GateSeconds[Index] : 0.0;
    if( Index == 1 )
      FActivePower = float( Frequency * double( uint64_t( HjlPowerRatio ) * FPowerCal ) / 1e7 );
    else if( Voltage )
     {
      FVoltage = float( Frequency * double( uint64_t( HjlVoltageRatio ) * FVoltageCal ) / 1e7 );
      FVoltageValid = Valid && Count > 0;
      if( Valid ) FVoltageIntervals += uint32_t( Count );
     }
    else
     {
      FCurrent = float( Frequency * double( uint64_t( HjlCurrentRatio ) * FCurrentCal ) / 1e9 );
      if( Valid ) FCurrentIntervals += uint32_t( Count );
     }
   }
 }
