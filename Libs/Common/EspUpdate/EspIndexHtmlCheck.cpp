// EspIndexHtmlCheck.cpp, Version: 1.00
#include "EspIndexHtmlCheck.h"

#if defined( ESP32 )
bool EspIndexHtmlValid( char const * )
 {
  return true;
 }
#else
#include <LittleFS.h>
#include <bearssl/bearssl.h>

#include <cstring>

namespace
 {
  constexpr char Doctype[] = "<!doctype html>";
  constexpr char HashPrefix[] = "<!-- SHA256: ";
  constexpr char HashSuffix[] = " -->";
  constexpr size_t HashTextLength = 64;
  constexpr size_t HashLength = HashTextLength / 2;
  constexpr size_t DoctypeLength = sizeof( Doctype ) - 1;
  constexpr size_t HashPrefixLength = sizeof( HashPrefix ) - 1;
  constexpr size_t HashSuffixLength = sizeof( HashSuffix ) - 1;
  constexpr size_t HeaderLength = DoctypeLength + HashPrefixLength + HashTextLength + HashSuffixLength;

  int HexValue( char Value )
   {
    if( Value >= '0' && Value <= '9' )
      return Value - '0';
    if( Value >= 'a' && Value <= 'f' )
      return Value - 'a' + 10;
    if( Value >= 'A' && Value <= 'F' )
      return Value - 'A' + 10;
    return -1;
   }
 }

bool EspIndexHtmlValid( char const * FileName )
 {
  File HtmlFile = LittleFS.open( FileName, "r" );
  if( !HtmlFile || HtmlFile.size() <= HeaderLength )
   {
    HtmlFile.close();
    return false;
   }

  char Header[ HeaderLength ];
  if( HtmlFile.read( reinterpret_cast<uint8_t *>( Header ), sizeof( Header ) ) != sizeof( Header ) )
   {
    HtmlFile.close();
    return false;
   }

  if( memcmp( Header, Doctype, DoctypeLength ) != 0 ||
      memcmp( Header + DoctypeLength, HashPrefix, HashPrefixLength ) != 0 ||
      memcmp( Header + DoctypeLength + HashPrefixLength + HashTextLength,
        HashSuffix, HashSuffixLength ) != 0 )
   {
    HtmlFile.close();
    return false;
   }

  uint8_t ExpectedHash[ HashLength ];
  char const * HashText = Header + DoctypeLength + HashPrefixLength;
  for( size_t Idx = 0;  Idx < HashLength;  ++Idx )
   {
    const int High = HexValue( HashText[ Idx * 2 ] );
    const int Low = HexValue( HashText[ Idx * 2 + 1 ] );
    if( High < 0 || Low < 0 )
     {
      HtmlFile.close();
      return false;
     }
    ExpectedHash[ Idx ] = static_cast<uint8_t>( ( High << 4 ) | Low );
   }

  br_sha256_context HashContext;
  br_sha256_init( &HashContext );
  br_sha256_update( &HashContext, Doctype, DoctypeLength );

  uint8_t Buffer[ 512 ];
  while( HtmlFile.available() )
   {
    const size_t ReadSize = HtmlFile.read( Buffer, sizeof( Buffer ) );
    if( ReadSize == 0 )
     {
      HtmlFile.close();
      return false;
     }
    br_sha256_update( &HashContext, Buffer, ReadSize );
   }
  HtmlFile.close();

  uint8_t ActualHash[ HashLength ];
  br_sha256_out( &HashContext, ActualHash );
  return memcmp( ActualHash, ExpectedHash, sizeof( ActualHash ) ) == 0;
 }
#endif
