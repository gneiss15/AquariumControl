// UUID.h, Version: 1.02
#pragma once

#include <GnBasics.h>

#if defined( GNPlatformESP )
  #include <Arduino.h>
#endif

class UUID
 {
 public:
                        UUID(void); //  seeds the instance 

                        //  at least one seed value is mandatory, two is better.
                        //  seed can be random or explicitly fixed.
  void                  Seed( uint32_t s1, uint32_t s2 = 0 );

 
  char *                Generate(void); //  generate a new UUID
  char *                Uuid(void)                                           { return FUuid; }

 private:
  void                  Generate( uint8_t * p );
  uint32_t              Random(void);
  uint32_t              Hash( const char * str );
  
 private:
  char                  FUuid[ 37 ];          //  UUID in string format
  uint32_t              Fw = 1;
  uint32_t              Fz = 2;
 };
