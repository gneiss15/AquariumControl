// UUID.cpp, Version: 1.00
#include "UUID.h"

//****************************************************************
// EncodeBase64
//****************************************************************

UUID::UUID(void)
 {
  //  seed - differs per construction on compile time constants
  //         and multiplication with micros().
  //  TODO investigate how to differ better at runtime.
  uint32_t s2 = Hash( __TIME__ ) * GnMicros();
  uint32_t s1 = s2 ^ Hash( __DATE__ );
           s2 = s2 ^ Hash( __FILE__ );
  Seed( s1, s2 );
  Generate();
 }

void UUID::Seed( uint32_t s1, uint32_t s2 )
 {
  if( s1 == 0 )
    s1 = 1;
  if( s2 == 0 )
    s2 = 2;
  Fw = s1;
  Fz = s2;
 }

char * UUID::Generate(void)
 {
  uint8_t u[ 16 ]; //  UUID in byte format

  Generate( &u[  0 ] );
  Generate( &u[  4 ] );
  Generate( &u[  8 ] );
  Generate( &u[ 12 ] );

  //  Conforming to RFC 4122 Specification, version 4, variant 1
  //  - byte 7: four most significant bits ==> 0100  --> always 4
  //  - byte 9: two  most significant bits ==> 10    --> always {8, 9, A, B}.
	u[ 6 ] = 0x40 | ( 0x0F & u[ 6 ] ); // version 4 / random based
	u[ 8 ] = 0x80 | ( 0x3F & u[ 8 ] ); // variant RFC9562

	sprintf
   ( 
    FUuid, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
    u[  0 ], u[  1 ], u[  2 ], u[  3 ], u[  4 ], u[  5 ], u[  6 ], u[  7 ],
    u[  8 ], u[  9 ], u[ 10 ], u[ 11 ], u[ 12 ], u[ 13 ], u[ 14 ], u[ 15 ]
   );

  return FUuid;
 }

void UUID::Generate( uint8_t * p )
 {
  uint32_t rnd = Random();
	p[ 0 ] = rnd >> 24;
	p[ 1 ] = rnd >> 16;
	p[ 2 ] = rnd >> 8;
	p[ 3 ] = rnd;
 }

uint32_t UUID::Random(void)
 {
  Fz = 36969L * ( Fz & 65535L ) + ( Fz >> 16 );
  Fw = 18000L * ( Fw & 65535L ) + ( Fw >> 16 );
  return ( Fz << 16 ) + Fw;  //   32-bit result
 }

uint32_t UUID::Hash( const char * s )
 {
  //  very simple hash function.
  uint32_t hash = strlen( s );
  for( ; *s != 0;  ++s )
    hash = hash * 2000099957 + *( (uint8_t *)s );
  return hash;
 }
