// GnBasics.h, Version: 2.44
#pragma once

//****************************************************************
// Includes
//****************************************************************

#include <GnPlatform.h>

#if defined( GNPlatformAVR )
  #include <GnBasicsAVR.h>
 #else
// Closing #endig is at the end of the file!

#include <Config.h>
#include <GnString.h>
#include <GnHash.h>
#include <GnTime.h>
#include <GnMemory.h>
#if defined( GNPlatformESP )
  #include <Arduino.h>
  #if defined( GNPlatformESP32 ) && !defined( DONT_USE_GpioPinMacros )
    #include <driver/gpio.h>
    #include <soc/gpio_reg.h>
    #include <soc/soc_caps.h>
  #endif
 #else
  #include <chrono>
  #include <iostream>
#endif

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>


#if 0
#if ENABLEDEBUG
  #pragma message( "ENABLEDEBUG 1" )
 #else
  #pragma message( "ENABLEDEBUG 0" )
#endif
#endif

//****************************************************************
// Global Types
//****************************************************************

typedef unsigned int uint_t;

//****************************************************************
// Global Macros
//****************************************************************

  //****************************************************************
  // MakeName
  //****************************************************************

#define MakeName2( a, b )             MakeName2_( a, b )
#define MakeName2_( a, b )            a##b
#define MakeName3( a, b, c )          MakeName3_( a, b, c )
#define MakeName3_( a, b, c )         a##b##c
#define MakeName4( a, b, c, d )       MakeName4_( a, b, c, d )
#define MakeName4_( a, b, c, d )      a##b##c##d
#define MakeName5( a, b, c, d, e )    MakeName5_( a, b, c, d, e )
#define MakeName5_( a, b, c, d, e )   a##b##c##d##e

  //****************************************************************
  // MakeStr
  //****************************************************************

#define MakeStr( a )                  MakeStr_( a )
#define MakeStr_( a )                 #a

  //****************************************************************
  // MakeChar
  //****************************************************************

#define MAKE_CHAR_A     'A'
#define MAKE_CHAR_B     'B'
#define MAKE_CHAR_C     'C'
#define MAKE_CHAR_D     'D'
#define MAKE_CHAR_E     'E'
#define MAKE_CHAR_F     'F'
#define MAKE_CHAR_G     'G'
#define MAKE_CHAR_H     'H'
#define MAKE_CHAR_I     'I'
#define MAKE_CHAR_J     'J'
#define MAKE_CHAR_K     'K'
#define MAKE_CHAR_L     'L'
#define MAKE_CHAR_M     'M'
#define MAKE_CHAR_N     'N'
#define MAKE_CHAR_O     'O'
#define MAKE_CHAR_P     'P'
#define MAKE_CHAR_Q     'Q'
#define MAKE_CHAR_R     'R'
#define MAKE_CHAR_S     'S'
#define MAKE_CHAR_T     'T'
#define MAKE_CHAR_U     'U'
#define MAKE_CHAR_V     'V'
#define MAKE_CHAR_W     'W'
#define MAKE_CHAR_X     'X'
#define MAKE_CHAR_Y     'Y'
#define MAKE_CHAR_Z     'Z'

#define MAKE_CHAR_a     'a'
#define MAKE_CHAR_b     'b'
#define MAKE_CHAR_c     'c'
#define MAKE_CHAR_d     'd'
#define MAKE_CHAR_e     'e'
#define MAKE_CHAR_f     'f'
#define MAKE_CHAR_g     'g'
#define MAKE_CHAR_h     'h'
#define MAKE_CHAR_i     'i'
#define MAKE_CHAR_j     'j'
#define MAKE_CHAR_k     'k'
#define MAKE_CHAR_l     'l'
#define MAKE_CHAR_m     'm'
#define MAKE_CHAR_n     'n'
#define MAKE_CHAR_o     'o'
#define MAKE_CHAR_p     'p'
#define MAKE_CHAR_q     'q'
#define MAKE_CHAR_r     'r'
#define MAKE_CHAR_s     's'
#define MAKE_CHAR_t     't'
#define MAKE_CHAR_u     'u'
#define MAKE_CHAR_v     'v'
#define MAKE_CHAR_w     'w'
#define MAKE_CHAR_x     'x'
#define MAKE_CHAR_y     'y'
#define MAKE_CHAR_z     'z'

#define MAKE_CHAR_0     '0'
#define MAKE_CHAR_1     '1'
#define MAKE_CHAR_2     '2'
#define MAKE_CHAR_3     '3'
#define MAKE_CHAR_4     '4'
#define MAKE_CHAR_5     '5'
#define MAKE_CHAR_6     '6'
#define MAKE_CHAR_7     '7'
#define MAKE_CHAR_8     '8'
#define MAKE_CHAR_9     '9'

#define MakeChar( a )   MakeChar_( a )
#define MakeChar_( a )  MAKE_CHAR_##a

  //****************************************************************
  // Other Global Macros
  //****************************************************************

#define EmptyMacro()

#define MacroFunc( exp )      do exp while( 0 )

#define Mask( bitNr )         ( 1 << (bitNr) )

#define TableSize( tab )      ( sizeof( tab ) / sizeof( (tab)[ 0 ] ) )

#define IsPowerOf2( a )       ( ( (a) & ( (a) - 1 ) ) == 0 )

#define ClrVar( var )         memset( (var), 0, sizeof( (var) ) )

extern int CmpVar_Sizes_not_equal_(void);   // do not implement! => Linker error when sizeof(x) != sizeof(y)
#define CmpVar( x, y )        ( sizeof( x ) == sizeof( y ) ? memcmp( &(x), &(y), sizeof( x ) ) : CmpVar_Sizes_not_equal_() )

extern void * CopyVar_Sizes_not_equal_(void);   // do not implement! => Linker error when sizeof(x) != sizeof(y)
#define CopyVar( dst, src )   ( sizeof( dst ) == sizeof( src ) ? memcpy( &(dst), &(src), sizeof( dst ) ) : CopyVar_Sizes_not_equal_() )

#define GnDelete( ptr )       MacroFunc({ delete ptr; ptr = NULL; })
#define GnDeleteArray( ptr )  MacroFunc({ delete [] ptr; ptr = NULL; })

//****************************************************************
//  Function decorations
//****************************************************************

#define noreturn_                     __attribute__(( noreturn ))

#define packed_                       __attribute__((packed))
#define align_( size )                __attribute__((aligned(size)))

#define longcall_                     __attribute__((longcall))
#define pure_                         __attribute__((pure))

#define unused_( x )                  x __attribute__((unused))
#define unused_para( def, var )       def var __attribute__((unused))

// C++98: 199711, C++11: 201103, C++14: 201402, C++17: 201703, C++20: 202002
#if defined(__cplusplus) and __cplusplus >= 201703
  #define maybe_unused_                 [[maybe_unused]]
 #else
  #define maybe_unused_
#endif
 
//****************************************************************
// Check C++20 language support
//****************************************************************

#if __cplusplus < 202002L && ( !defined( __cpp_concepts ) || __cpp_concepts < 201907L )
  #pragma message( "__cplusplus = " MakeStr( __cplusplus ) )
  #error C++20 language support is required
#endif

//****************************************************************
// Functions (Time)
//****************************************************************

extern void DelayUs( uint32_t Us );
extern void DelayMs( uint32_t Ms );
extern void DelayS( uint32_t Seconds );
extern uint64_t GnMillis64(void);

#if defined( GNPlatformESP )
  static inline uint32_t GnMicros(void)                                       { return micros(); }
  static inline uint32_t GnMillis(void)                                       { return millis(); }
#else
  static inline uint32_t GnMicros(void)
   {
    return static_cast<uint32_t>( std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::steady_clock::now().time_since_epoch() ).count() );
   }
  static inline uint32_t GnMillis(void)
   {
    return static_cast<uint32_t>( std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now().time_since_epoch() ).count() );
   }
#endif
static inline uint32_t GnSeconds(void)                                        { return static_cast<uint32_t>( time( NULL ) ); }

//****************************************************************
// Functions (GPIO)
//****************************************************************

#if defined( GNPlatformESP )

#ifndef DONT_USE_GpioPinMacros

#if defined( BOARD_HAS_PIN_REMAP ) && !defined( BOARD_USES_HW_GPIO_NUMBERS )
  #error "Pin remapping not supported"
#endif

//?#define GnPinMask( pin )              ( (TGpioPinMask)( 1 ) << ( pin ) )
//?#define GpioModReg            ( (volatile TGpioPinMask *)GPIO_ENABLE_REG )

  
// For GPIO-Irqs see: core_esp8266_wiring_digital.cpp or core_esp8266_wiring_digital.c
/*
  ESP-Pins...

ESP8266		0,2,12-15
		? input only
ESP32		0~39 except 24 & 28~31		32
		>= 34 are input only
ESP32S2		0~46 except 22~25
		46 is input only
ESP32S3		0~48 except 22~25
		No GPIO is input only
ESP32C3		0-21
		No GPIO is input only
*/

// ESP8266 supports GPIO0..15 here; ESP32 uses the target-specific IDF GPIO range.

#if defined( GNPlatformESP8266 )
  //#include <esp32-hal-gpio.h>

  typedef uint16_t TGpioPinMask;

  static inline void SetPinM( TGpioPinMask pinMask )                                 { GPOS = pinMask; }
  static inline void ClrPinM( TGpioPinMask pinMask )                                 { GPOC = pinMask; }
  static inline void TogglePinM( TGpioPinMask pinMask )                              { GPO ^= pinMask; }
  static inline bool GetOutPinM( TGpioPinMask pinMask )                              { return GPO & pinMask; }
  static inline bool GetInPinM( TGpioPinMask pinMask )                               { return GPI & pinMask; }

  static inline void PinMode_Output( uint8_t pinNr )
   {
    GPF( pinNr ) = GPFFS( GPFFS_GPIO( pinNr ) ); // Set mode to GPIO
    GPC( pinNr ) = GPC( pinNr ) & ~( ( 1 << GPCS /* 0: SOURCE(GPIO)*/ ) | ( 1 << GPCD /* 0: DRIVER(PushPull) */ ) ); 
    GPES = ( 1 << pinNr ); // Output
   }
  static inline void PinMode_OutputOpenDrain( uint8_t pinNr )
   {
    GPF( pinNr ) = GPFFS( GPFFS_GPIO( pinNr ) ); // Set mode to GPIO
    GPC( pinNr ) =  ( GPC( pinNr ) & ~( 1 << GPCS /* 0: SOURCE(GPIO) */ )  ) | ( 1 << GPCD /* 1: DRIVER(OpenDrain) */ ) ; 
    GPES = ( 1 << pinNr ); // Output
   }
  static inline void PinMode_Input( uint8_t pinNr )
   {
    GPF( pinNr ) = GPFFS( GPFFS_GPIO( pinNr ) ); // Set mode to GPIO
    GPEC = ( 1 << pinNr ); // Input
    GPC( pinNr ) = ( GPC( pinNr ) & ~( 1 << GPCS /* 0: SOURCE(GPIO) */ ) ) | ( 1 << GPCD /* 1: DRIVER(OpenDrain) */ );
   }
  static inline void PinMode_InputPullUp( uint8_t pinNr )
   {
    GPF( pinNr ) = GPFFS( GPFFS_GPIO( pinNr ) ) | ( 1 << GPFPU ) ; // Set mode to GPIO & enable PullUp
    GPEC = ( 1 << pinNr ); // Input
    GPC( pinNr ) = ( GPC( pinNr ) & ~( 1 << GPCS /* 0: SOURCE(GPIO) */ ) ) | ( 1 << GPCD /* 1: DRIVER(OpenDrain) */ );
   }

 #elif defined( GNPlatformESP32 )

  typedef uint64_t TGpioPinMask;

  #define P( p )  (*(volatile uint32_t *)p)
  #if SOC_GPIO_PIN_COUNT > 32
    #define PC64( x ) x;
   #else
    #define PC64( x )
  #endif

  static inline void SetPinM( TGpioPinMask pinMask )
   {
    P( GPIO_OUT_W1TS_REG ) = uint32_t( pinMask );
    PC64( P( GPIO_OUT1_W1TS_REG ) = uint32_t( pinMask >> 32 ) )
   }
  static inline void ClrPinM( TGpioPinMask pinMask )
   {
    P( GPIO_OUT_W1TC_REG ) = uint32_t( pinMask );
    PC64( P( GPIO_OUT1_W1TC_REG ) = uint32_t( pinMask >> 32 ) )
   }
  static inline void TogglePinM( TGpioPinMask pinMask )
   {
    // Invert selected latch bits; not atomic against another writer of the same pin.
    const uint32_t Out = P( GPIO_OUT_REG );
    P( GPIO_OUT_W1TC_REG ) = Out & uint32_t( pinMask );
    P( GPIO_OUT_W1TS_REG ) = ~Out & uint32_t( pinMask );
    PC64( const uint32_t Out1 = P( GPIO_OUT1_REG ) )
    PC64( P( GPIO_OUT1_W1TC_REG ) = Out1 & uint32_t( pinMask >> 32 ) )
    PC64( P( GPIO_OUT1_W1TS_REG ) = ~Out1 & uint32_t( pinMask >> 32 ) )
   }
  static inline bool GetOutPinM( TGpioPinMask pinMask )
   {
    // Read the output latch independently of the input pad and output enable.
    TGpioPinMask Out = P( GPIO_OUT_REG );
    PC64( Out |= TGpioPinMask( P( GPIO_OUT1_REG ) ) << 32 )
    return ( Out & pinMask ) != 0;
   }
  static inline bool GetInPinM( TGpioPinMask pinMask )
   {
    TGpioPinMask In = P( GPIO_IN_REG );
    PC64( In |= TGpioPinMask( P( GPIO_IN1_REG ) ) << 32 )
    return ( In & pinMask ) != 0;
   }

  #undef PC64
  #undef P

  static inline void GnConfigureGpio( uint8_t pinNr, gpio_mode_t Mode, gpio_pullup_t PullUp )
   {
    // IDF validates the target-specific input/output capabilities before the
    // mask shift. gpio_config selects GPIO routing without changing the latch.
    ESP_ERROR_CHECK( GPIO_IS_VALID_GPIO( pinNr ) ? ESP_OK : ESP_ERR_INVALID_ARG );
    gpio_config_t Config = {};
    Config.pin_bit_mask = TGpioPinMask( 1 ) << pinNr;
    Config.mode = Mode;
    Config.pull_up_en = PullUp;
    ESP_ERROR_CHECK( gpio_config( &Config ) );
   }
  static inline void PinMode_Output( uint8_t pinNr )
   {
    // Call SetPin/ClrPin first to preload the intended initial output level.
    GnConfigureGpio( pinNr, GPIO_MODE_INPUT_OUTPUT, GPIO_PULLUP_DISABLE );
   }
  static inline void PinMode_OutputOpenDrain( uint8_t pinNr )
   {
    GnConfigureGpio( pinNr, GPIO_MODE_INPUT_OUTPUT_OD, GPIO_PULLUP_DISABLE );
   }
  static inline void PinMode_Input( uint8_t pinNr )
   {
    GnConfigureGpio( pinNr, GPIO_MODE_INPUT, GPIO_PULLUP_DISABLE );
   }
  static inline void PinMode_InputPullUp( uint8_t pinNr )
   {
    GnConfigureGpio( pinNr, GPIO_MODE_INPUT, GPIO_PULLUP_ENABLE );
   }

#endif

// Use a GPIO-sized mask: the generic Mask macro shifts a 32-bit int.
// As with the register mask functions, callers must supply valid GPIO numbers.
static inline void SetPin( uint8_t pinNr )                                           { SetPinM( TGpioPinMask( 1 ) << pinNr ); }
static inline void ClrPin( uint8_t pinNr )                                           { ClrPinM( TGpioPinMask( 1 ) << pinNr ); }
static inline void TogglePin( uint8_t pinNr )                                        { TogglePinM( TGpioPinMask( 1 ) << pinNr ); }
static inline bool GetOutPin( uint8_t pinNr )                                        { return GetOutPinM( TGpioPinMask( 1 ) << pinNr ); }
static inline bool GetInPin( uint8_t pinNr )                                         { return GetInPinM( TGpioPinMask( 1 ) << pinNr ); }

//****************************************************************
// Functions (others)
//****************************************************************

#if defined( GNPlatformESP )
static inline IRAM_ATTR uint32_t GetCycleCount(void)
 {
  #if defined( GNPlatformESP32 )
    return ESP.getCycleCount();
  #else
    uint32_t ccount;
    __asm__ __volatile__( "esync; rsr %0,ccount":"=a" ( ccount ) );
    return ccount;
  #endif
 }
#endif

#else // USE_GpioPinMacros

// ...PinM: Not Usable
#define SetPin( pinNr )                                     digitalWrite( pinNr, 1 )
#define ClrPin( pinNr )                                     digitalWrite( pinNr, 0 )
#define TogglePin( pinNr )                                  digitalWrite( pinNr, !GetOutPin( pinNr ) )
//?#define GetOutPin( pinNr )                                  digitalReadOutputPin( pinNr ) 
#define GetOutPin( pinNr )                                  digitalRead( pinNr ) 
#define GetInPin( pinNr )                                   digitalRead( pinNr ) 

#define PinMode_Output( pinNr )                             pinMode( pinNr, OUTPUT )
#define PinMode_OutputOpenDrain( pinNr )                    pinMode( pinNr, OUTPUT_OPEN_DRAIN )
#define PinMode_Input( pinNr )                              pinMode( pinNr, INPUT )
#define PinMode_InputPullUp( pinNr )                        pinMode( pinNr, INPUT_PULLUP )

#endif // USE_GpioPinMacros

#endif // ESP8266 || ESP32

//****************************************************************
// CTassert & derivates
//****************************************************************

  /*
  #define CTassert( cond, msg ) ...
    cond is a compile-time integral or pointer expression
    msg is a C++ identifier that does not need to be defined
    If expr is zero, a compile-time error is generated
    - msg will appear in the error message

  #define CTerror( msg ) ...
    always generates a compile-time error

  #define CTassertTypeSizeEQ( name, size ) ...
    Check that 'sizeof( name ) == size'
    a compile-time error is generated if condition is not fullfiled
    Depending on compiler the message may contain the size of 'name'

  #define CTassertTypeSizeGE( name, size ) ...
    Similar to CTassertTypeSizeEQ, but condition is 'sizeof( name ) >= size'

  #define CTallUnionSizeCheck( unionName ) ...
    primary used for unions with a member 'All' that should be (at least)
    as long as the union
  */

#define CTerror( msg ) \
  CTassert( false, msg )
#define CTassertTypeSizeEQ( name, size ) \
  CTassert( sizeof( name ) == size, Sizeof_##name##_not_equal_expected_size )
#define CTassertTypeSizeLE( name, maxsize ) \
  CTassert( sizeof( name ) <= maxsize, Sizeof_##name##_greater_than_max_size )
#define CTassertTypeSizeGE( name, minsize ) \
  CTassert( sizeof( name ) >= minsize, Sizeof_##name##_less_than_min_size )
#define CTallUnionSizeCheck( unionName ) \
  CTassert( sizeof( unionName ) == sizeof( ((unionName*)0)->All ), unionName##_size_invalid )

#define CTassert( cond, msg ) \
  static_assert( cond, msg )

//****************************************************************
// GnAssert
//****************************************************************

// Generate call to failed_ with: __FILE__, __LINE__, & msg
#define GnFail( msg ) \
  failed_( __FILE__, MakeStr( __LINE__ ), msg )

// Call GnFail if an assertion fails in a all builds.
#define GnCheck( cond, msg )     \
  MacroFunc \
   ({ \
    if( !(cond) ) GnFail( msg ); \
   })

// Call GnFail if an assertion fails in a debug build.
#if FINALVERSION
  #define GnAssert( cond, msg )
 #else
  #define GnAssert( cond, msg ) GnCheck( cond, msg )
#endif

// Assert in debug builds, leave the code in other builds.
#if FINALVERSION
  #define GnVerify( exp, msg  ) ( exp )
 #else
  #define GnVerify( exp, msg )  GnCheck( exp, msg )
#endif

extern noreturn_ void failed_
 (
  char const * file,
  char const * line,
  char const * msg
 );

//****************************************************************
// Functions (others)
//****************************************************************

template <typename t>
  void Restrict( t & var, t min, t max )
   {
    if( var < min )
      var = min;
     else if( var > max )
      var = max;
   }
  
// Convert uint8_t to 2 digit String
inline TGNString TwoDigitStr( uint8_t const v )
 {
  char Str[ 4 ];
  snprintf( Str, sizeof( Str ), "%02u", static_cast<unsigned int>( v ) );
  return TGNString( Str );
 }

inline TGNString TimeString( uint8_t const h, uint8_t const m )
 {
  return TwoDigitStr( h ) + ":" + TwoDigitStr( m );
 }

inline TGNString TimeString( uint8_t const h, uint8_t const m, uint8_t const s )
 {
  return TimeString( h, m ) + ":" + TwoDigitStr( s );
 }

// Similar to String.format() (Java or Python)
extern TGNString StringFormat( const char * fmt, ... );

#if defined( GNPlatformESP )
struct TFSInfo
 {
  size_t totalBytes;
  size_t usedBytes;
  size_t blockSize;
  size_t pageSize;
  size_t maxOpenFiles;
  size_t maxPathLength;
 };

extern bool GetLittleFSInfo( TFSInfo & Info );
extern bool WriteLittleFSTemporary( char const * FileName, TGNString const & Data, TGNString & Error );
extern bool CommitLittleFSTemporary( char const * FileName, TGNString & Error );
extern bool RestoreLittleFSBackup( char const * FileName, TGNString & Error );
extern void DiscardLittleFSTemporary( char const * FileName );
#endif

// Read File from LittleFs into String
extern TGNString ReadFile( char const * fileName );
#if defined( GNPlatformESP )
  extern TGNString ReadFile( __FlashStringHelper const * fileName );
#endif

//****************************************************************
// Policy Classes
//****************************************************************

class no_defaultcreate
 {
 protected:
  inline              no_defaultcreate(void) = delete;
 };

class no_copy
 {
 private:
                      no_copy( no_copy const & src ) = delete;
 public:
  inline              no_copy(void) throw() = default;
 };

class no_assign
 {
 private:
  template<class T>
    no_assign &       operator=( T const & rhs ) = delete;
 public:
  inline              no_assign(void) throw() = default;
 };

template <class T>
  class TSingleton : no_copy, no_assign
   {
   protected:
    inline            TSingleton(void) throw() = default;
                      ~TSingleton(void) throw() = default;
   public:
    static inline T & Instance(void);
   };
template<class T>
 /*static*/ T &       TSingleton<T>::Instance(void)
  {
   static T SInstance;
   return SInstance;
  }

//****************************************************************
// TLoopInstances
//
// List of Instances, that needs to be called inside loop
//****************************************************************

// This (Singlton-)class maintains a list of all instances of
// TLoopInstance.
// To enshure a regulary call of all methods "Loop" of all instances
// insert a call to
// LoopInstances.Loop();
// inside loop() of the sketch

class TLoopInstances;
class TLoopInstance;

class TLoopInstances : public TSingleton<TLoopInstances>
 {
  friend class TSingleton<TLoopInstances>;
  friend class TLoopInstance;
 protected:
  TLoopInstance *     FList;
 private:
                      TLoopInstances(void)              : FList( NULL ) {}
                      ~TLoopInstances(void)             { FList = NULL; }
 protected:
  void                Add( TLoopInstance * obj );
  void                Remove( TLoopInstance * obj );
 public:
  void                Loop(void);
 };
#define LoopInstances TLoopInstances::Instance()
 
//****************************************************************
// TLoopInstance
//****************************************************************

// The method "Loop" of each class derived from TLoopInstance will be
// called by a call to
// LoopInstances.Loop();

class TLoopInstance
 {
  friend class TLoopInstances;
 private:
  virtual void        Loop(void)                        = 0;
  TLoopInstance *     FNext;
 public:
  TLoopInstance *     Next(void)                        { return FNext; }
  
 public:
                      TLoopInstance(void)               : FNext( NULL ) { LoopInstances.Add( this ); }
                      ~TLoopInstance(void)              { LoopInstances.Remove( this ); }
 };
 
//****************************************************************
// TBits
//****************************************************************

class TBitsBase
 {
 protected: // Prevent this class from being used anywhere else.
  class TBitsRef
   {
   public:
                          TBitsRef( uint64_t & r, uint64_t m )                 : FByteRef( r ), FBitMask( m ) {}
    const TBitsRef &      operator=( bool b )                                  { if( b ) FByteRef |= FBitMask; else FByteRef &= ~FBitMask; return *this; }
    const TBitsRef &      operator=( const TBitsRef & r )                      { return this->operator=( bool( r ) ); }
    operator              bool(void) const                                     { return ( FByteRef & FBitMask ) ? true : false; }
   private:
    uint64_t &            FByteRef;
    uint64_t              FBitMask;
   };
 };

template <size_t N> class TBits : public TBitsBase
 {
 private:
  static constexpr size_t FCapacity = N;
  static constexpr size_t FNumInts = ( ( N + 63 ) / 64 );
 public:
                          TBits(void)                                          : FSize( 0 ) { memset( FData, 0, sizeof( FData ) ); }
  size_t                  Capacity(void) const                                 { return FCapacity; }
  size_t                  Size(void) const                                     { return FSize; }
  bool                    operator[]( size_t n ) const                         { return ( FData[ ByteN( n ) ] & BitMask( n ) ) ? true : false; }
  TBitsRef                operator[]( size_t n )                               { return TBitsRef( FData[ ByteN( n ) ], BitMask( n ) ); }
  void                    Add( const bool b )                                  { GnAssert( FSize < FCapacity, "TBits::Add: FSize >= FCapacity" ); operator[]( FSize++ ) = b; }
  // Debug
  const uint64_t &        Data(void) const                                     { return FData; }
 private:
  size_t                  ByteN( size_t n ) const                              { return n / 64; }
  size_t                  BitN( size_t n ) const                               { return n % 64; }
  uint64_t                BitMask( size_t n ) const                            { return 1ULL << BitN( n ); }
 private:
  size_t                  FSize;
  uint64_t                FData[ FNumInts ];
 };

// Template specialisation for N = 1..64
template <size_t N> requires ( 1 <= N && N <= 64 ) class TBits<N> : public TBitsBase
 {
 private:
  static constexpr size_t FCapacity = N;
  static constexpr uint64_t FMaxMask = 0xFFFFFFFFFFFFFFFFULL;
 public:
                          TBits(void)                                          : FSize( 0 ), FData( 0 ) {}
  size_t                  Capacity(void) const                                 { return N; }
  size_t                  Size(void) const                                     { return FSize; }
  bool                    operator[]( size_t n ) const                         { return ( FData & BitMask( n ) ) ? true : false; }
  TBitsRef                operator[]( size_t n )                               { return TBitsRef( FData, BitMask( n ) ); }
  void                    Add( const bool b )                                  { GnAssert( FSize < FCapacity, "TBits::Add: FSize >= FCapacity" ); operator[]( FSize++ ) = b; }
  void                    Resize( size_t n )                                   { GnAssert( n <= FCapacity, "TBits::Resize: n > FCapacity" ); FSize = n; FData &= ValidMask( n ); }
  uint64_t                NLsbBits( size_t n ) const                           { return ValidMask( n ) & FData; }
  // Debug
  const uint64_t &        Data(void) const                                     { return FData; }
 private:
  size_t                  BitN( size_t n ) const                               { return n % 64; }
  uint64_t                BitMask( size_t n ) const                            { return 1ULL << BitN( n ); }
  uint64_t                ValidMask( size_t n ) const                          { return 0xFFFFFFFFFFFFFFFFULL >> ( 64 - n ); }
 private:
  size_t                  FSize;
  uint64_t                FData;
 };

//****************************************************************
// Log
//****************************************************************
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter" 

class TLog : public TSingleton<TLog>
 {
  friend class TSingleton<TLog>;
 private:
                       TLog(void);
 public:
  void                 LogPrintf( char const * fmt, ... )  __attribute__ ((format (printf, 2, 3)));
 };
#define Log( fmt, ... )    TLog::Instance().LogPrintf( fmt, ## __VA_ARGS__ )

#pragma GCC diagnostic pop

//****************************************************************
// Debug
//****************************************************************

#if !defined( GNPlatformESP )
  template <typename... TArgs>
    inline void GnPrintln( TArgs const & ... args )
     {
      ( ( std::cout << args ), ... );
      std::cout << std::endl;
     }
#endif

#if ENABLEDEBUG
  #if defined( GNPlatformESP )
    #define Dprintln(...) Serial.println( __VA_ARGS__ )
    #define Dprintf(...)  Serial.printf( __VA_ARGS__ )
   #else
    #define Dprintln(...) GnPrintln( __VA_ARGS__ )
    #define Dprintf(...)  printf( __VA_ARGS__ )
  #endif
 #else
  #define Dprintln(...) 
  #define Dprintf(...)  
#endif

// Error/Normal
#if defined( GNPlatformESP )
  #define Eprintln(...) Serial.println( __VA_ARGS__ )
  #define Eprintf(...)  Serial.printf( __VA_ARGS__ )

  #define Println(...) Serial.println( __VA_ARGS__ )
  #define Printf(...)  Serial.printf( __VA_ARGS__ )
#else
  #define Eprintln(...) GnPrintln( __VA_ARGS__ )
  #define Eprintf(...)  printf( __VA_ARGS__ )

  #define Println(...) GnPrintln( __VA_ARGS__ )
  #define Printf(...)  printf( __VA_ARGS__ )
#endif

#if ENABLEDEBUG
  #define Debug( fmt, ... ) Log( fmt, ## __VA_ARGS__ )
 #else
  // Non-Debug: Vanish All Debug-Code
  #define Debug( ... ) 
#endif

#endif // GNPlatformAVR
