// WebSocketProtocol.h, Version: 1.03
#pragma once
#include <GnBasics.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <vector>

namespace WebSocketProtocol
 {
constexpr char   AcceptGuid[] PROGMEM = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
constexpr size_t MaxFramePayload = 64 * 1024;

struct TFrame
 {
  uint8_t   OpCode = 0;
  TGNString Payload;
 };

inline uint32_t RotateLeft( uint32_t Value, unsigned Count )
 {
  return ( Value << Count ) | ( Value >> ( 32 - Count ) );
 }

inline TGNString Trim( TGNString const & Text )
 {
  size_t First = 0;
  while ( First < Text.length() && std::isspace( static_cast<unsigned char>( Text[First] ) ) )
    ++First;

  size_t Last = Text.length();
  while ( Last > First && std::isspace( static_cast<unsigned char>( Text[Last - 1] ) ) )
    --Last;
  return Text.substr( First, Last - First );
 }

inline bool EqualsIgnoreCase( TGNString const & Left, char const * Right )
 {
  if ( Right == nullptr || Left.length() != strlen( Right ) )
    return false;

  for ( size_t Index = 0; Index < Left.length(); ++Index )
   {
    const unsigned char LeftChar = static_cast<unsigned char>( Left[Index] );
    const unsigned char RightChar = static_cast<unsigned char>( Right[Index] );
    if ( std::tolower( LeftChar ) != std::tolower( RightChar ) )
      return false;
   }
  return true;
 }

inline bool ContainsTokenIgnoreCase( TGNString const & Text, char const * Token )
 {
  size_t Pos = 0;
  while ( Pos < Text.length() )
   {
    size_t End = Text.find( ',', Pos );
    if ( End == TGNString::npos )
      End = Text.length();
    if ( EqualsIgnoreCase( Trim( Text.substr( Pos, End - Pos ) ), Token ) )
      return true;
    Pos = End + 1;
   }
  return false;
 }

inline void Sha1( char const * Data, size_t Length, uint8_t Digest[20] )
 {
  const uint64_t BitLength = static_cast<uint64_t>( Length ) * 8;
  const size_t PaddedLength = ( ( Length + 9 + 63 ) / 64 ) * 64;
  std::vector<uint8_t> Buffer( PaddedLength, 0 );
  if ( Length != 0 )
    memcpy( Buffer.data(), Data, Length );
  Buffer[Length] = 0x80;
  for ( unsigned Index = 0; Index < 8; ++Index )
    Buffer[PaddedLength - 1 - Index] = static_cast<uint8_t>( BitLength >> ( Index * 8 ) );

  uint32_t H0 = 0x67452301;
  uint32_t H1 = 0xEFCDAB89;
  uint32_t H2 = 0x98BADCFE;
  uint32_t H3 = 0x10325476;
  uint32_t H4 = 0xC3D2E1F0;

  for ( size_t Offset = 0; Offset < PaddedLength; Offset += 64 )
   {
    uint32_t Words[80]{};
    for ( unsigned Index = 0; Index < 16; ++Index )
     {
      const size_t Pos = Offset + Index * 4;
      Words[Index] = static_cast<uint32_t>( Buffer[Pos] ) << 24 |
                     static_cast<uint32_t>( Buffer[Pos + 1] ) << 16 |
                     static_cast<uint32_t>( Buffer[Pos + 2] ) << 8 |
                     static_cast<uint32_t>( Buffer[Pos + 3] );
     }
    for ( unsigned Index = 16; Index < 80; ++Index )
      Words[Index] = RotateLeft( Words[Index - 3] ^ Words[Index - 8] ^ Words[Index - 14] ^ Words[Index - 16], 1 );

    uint32_t A = H0;
    uint32_t B = H1;
    uint32_t C = H2;
    uint32_t D = H3;
    uint32_t E = H4;
    for ( unsigned Index = 0; Index < 80; ++Index )
     {
      uint32_t Function = 0;
      uint32_t Constant = 0;
      if ( Index < 20 )
       {
        Function = ( B & C ) | ( ( ~B ) & D );
        Constant = 0x5A827999;
       }
       else if ( Index < 40 )
       {
        Function = B ^ C ^ D;
        Constant = 0x6ED9EBA1;
       }
       else if ( Index < 60 )
       {
        Function = ( B & C ) | ( B & D ) | ( C & D );
        Constant = 0x8F1BBCDC;
       }
       else
       {
        Function = B ^ C ^ D;
        Constant = 0xCA62C1D6;
       }

      const uint32_t Temp = RotateLeft( A, 5 ) + Function + E + Constant + Words[Index];
      E = D;
      D = C;
      C = RotateLeft( B, 30 );
      B = A;
      A = Temp;
     }

    H0 += A;
    H1 += B;
    H2 += C;
    H3 += D;
    H4 += E;
   }

  const uint32_t Hash[5] = { H0, H1, H2, H3, H4 };
  for ( unsigned Word = 0; Word < 5; ++Word )
   {
    for ( unsigned Byte = 0; Byte < 4; ++Byte )
      Digest[Word * 4 + Byte] = static_cast<uint8_t>( Hash[Word] >> ( 24 - Byte * 8 ) );
   }
 }

inline TGNString Base64( uint8_t const * Data, size_t Length )
 {
  static char const Alphabet[] PROGMEM = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const size_t ResultSize = ( ( Length + 2 ) / 3 ) * 4;
  TGNString Result;
  if( !GnReserveStringExact( Result, ResultSize ) )
    return TGNString();
  for ( size_t Pos = 0; Pos < Length; Pos += 3 )
   {
    const uint32_t Value = static_cast<uint32_t>( Data[Pos] ) << 16 |
                           static_cast<uint32_t>( Pos + 1 < Length ? Data[Pos + 1] : 0 ) << 8 |
                           static_cast<uint32_t>( Pos + 2 < Length ? Data[Pos + 2] : 0 );
    Result.push_back( static_cast<char>( pgm_read_byte( &Alphabet[( Value >> 18 ) & 0x3F] ) ) );
    Result.push_back( static_cast<char>( pgm_read_byte( &Alphabet[( Value >> 12 ) & 0x3F] ) ) );
    Result.push_back( Pos + 1 < Length ? static_cast<char>( pgm_read_byte( &Alphabet[( Value >> 6 ) & 0x3F] ) ) : '=' );
    Result.push_back( Pos + 2 < Length ? static_cast<char>( pgm_read_byte( &Alphabet[Value & 0x3F] ) ) : '=' );
   }
  return Result;
 }

inline TGNString AcceptKey( TGNString const & ClientKey )
 {
  const size_t SourceSize = ClientKey.length() + sizeof( AcceptGuid ) - 1;
  TGNString Source = ClientKey;
  if( !GnReserveStringExact( Source, SourceSize ) )
    return TGNString();
  Source += FPSTR( AcceptGuid );
  uint8_t Digest[20];
  Sha1( Source.data(), Source.length(), Digest );
  return Base64( Digest, sizeof( Digest ) );
 }

inline bool ParseHandshake( TGNString const & Request, TGNString & Path, TGNString & ClientKey )
 {
  const size_t HeaderEnd = Request.find( "\r\n\r\n" );
  const size_t RequestLineEnd = Request.find( "\r\n" );
  if ( HeaderEnd == TGNString::npos || RequestLineEnd == TGNString::npos )
    return false;

  const TGNString RequestLine = Request.substr( 0, RequestLineEnd );
  const size_t MethodEnd = RequestLine.find( ' ' );
  const size_t PathEnd = MethodEnd == TGNString::npos ? TGNString::npos : RequestLine.find( ' ', MethodEnd + 1 );
  if ( MethodEnd == TGNString::npos || PathEnd == TGNString::npos || RequestLine.substr( 0, MethodEnd ) != "GET" )
    return false;
  Path = RequestLine.substr( MethodEnd + 1, PathEnd - MethodEnd - 1 );

  bool Upgrade = false;
  bool ConnectionUpgrade = false;
  bool Version13 = false;
  size_t HeaderPos = RequestLineEnd + 2;
  while ( HeaderPos < HeaderEnd )
   {
    const size_t HeaderLineEnd = Request.find( "\r\n", HeaderPos );
    if ( HeaderLineEnd == TGNString::npos || HeaderLineEnd > HeaderEnd )
      return false;

    const TGNString HeaderLine = Request.substr( HeaderPos, HeaderLineEnd - HeaderPos );
    const size_t Separator = HeaderLine.find( ':' );
    if ( Separator == TGNString::npos )
      return false;

    const TGNString Name = Trim( HeaderLine.substr( 0, Separator ) );
    const TGNString Value = Trim( HeaderLine.substr( Separator + 1 ) );
    if ( EqualsIgnoreCase( Name, "Upgrade" ) )
      Upgrade = EqualsIgnoreCase( Value, "websocket" );
    else if ( EqualsIgnoreCase( Name, "Connection" ) )
      ConnectionUpgrade = ContainsTokenIgnoreCase( Value, "Upgrade" );
    else if ( EqualsIgnoreCase( Name, "Sec-WebSocket-Key" ) )
      ClientKey = Value;
    else if ( EqualsIgnoreCase( Name, "Sec-WebSocket-Version" ) )
      Version13 = Value == "13";
    HeaderPos = HeaderLineEnd + 2;
   }
  return Upgrade && ConnectionUpgrade && Version13 && !ClientKey.empty();
 }

inline TGNString BuildFrame( uint8_t OpCode, TGNString const & Payload )
 {
  TGNString Frame;
  const auto Build = [&]( auto & Output )
   {
    Output += static_cast<char>( 0x80 | ( OpCode & 0x0F ) );
    if( Payload.length() <= 125 )
     {
      Output += static_cast<char>( Payload.length() );
     }
     else if( Payload.length() <= 0xFFFF )
     {
      Output += static_cast<char>( 126 );
      Output += static_cast<char>( Payload.length() >> 8 );
      Output += static_cast<char>( Payload.length() );
     }
     else
     {
      Output += static_cast<char>( 127 );
      const uint64_t Length = Payload.length();
      for( int Shift = 56;  Shift >= 0;  Shift -= 8 )
        Output += static_cast<char>( Length >> Shift );
     }
    Output.append( Payload );
   };

  if( !GnBuildStringExact( Frame, Build ) )
    return TGNString();
  return Frame;
 }

inline bool ExtractFrame( TGNString & Buffer, TFrame & Frame, bool & ProtocolError )
 {
  ProtocolError = false;
  if ( Buffer.length() < 2 )
    return false;

  const uint8_t First = static_cast<uint8_t>( Buffer[0] );
  const uint8_t Second = static_cast<uint8_t>( Buffer[1] );
  if ( !( First & 0x80 ) || !( Second & 0x80 ) )
   {
    ProtocolError = true;
    return false;
   }

  uint64_t PayloadLength = Second & 0x7F;
  size_t HeaderLength = 2;
  if ( PayloadLength == 126 )
   {
    if ( Buffer.length() < 4 )
      return false;
    PayloadLength = static_cast<uint8_t>( Buffer[2] ) << 8 | static_cast<uint8_t>( Buffer[3] );
    HeaderLength = 4;
   }
   else if ( PayloadLength == 127 )
   {
    if ( Buffer.length() < 10 )
      return false;
    PayloadLength = 0;
    for ( size_t Index = 2; Index < 10; ++Index )
      PayloadLength = PayloadLength << 8 | static_cast<uint8_t>( Buffer[Index] );
    HeaderLength = 10;
   }
  if ( PayloadLength > MaxFramePayload || ( ( First & 0x08 ) && PayloadLength > 125 ) )
   {
    ProtocolError = true;
    return false;
   }

  const size_t CompleteLength = HeaderLength + 4 + static_cast<size_t>( PayloadLength );
  if ( Buffer.length() < CompleteLength )
    return false;

  const uint8_t Mask[4] = {
    static_cast<uint8_t>( Buffer[HeaderLength] ),
    static_cast<uint8_t>( Buffer[HeaderLength + 1] ),
    static_cast<uint8_t>( Buffer[HeaderLength + 2] ),
    static_cast<uint8_t>( Buffer[HeaderLength + 3] )
  };
  Frame.OpCode = First & 0x0F;
  Frame.Payload.clear();
  if( !GnReserveStringExact( Frame.Payload, static_cast<size_t>( PayloadLength ) ) )
   {
    ProtocolError = true;
    return false;
   }
  const size_t PayloadPos = HeaderLength + 4;
  for ( size_t Index = 0; Index < PayloadLength; ++Index )
    Frame.Payload.push_back( static_cast<char>( static_cast<uint8_t>( Buffer[PayloadPos + Index] ) ^ Mask[Index & 3] ) );
  Buffer.erase( 0, CompleteLength );
  return true;
 }
 } // namespace WebSocketProtocol
