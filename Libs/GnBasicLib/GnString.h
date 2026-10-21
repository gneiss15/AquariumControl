// GnString.h, Version: 1.11
#ifndef GnStringH  // sentry
#define GnStringH
#pragma once

//****************************************************************
// Includes
//****************************************************************

#include <GnPlatform.h>

#if defined( GNPlatformESP )
  #include <Arduino.h>
#endif

#include <algorithm>
#include <assert.h>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string.h>
#include <type_traits>
#include <utility>

//****************************************************************
// TGNString
//****************************************************************

#if defined( GNPlatformESP )

#if defined( __GNUC__ )
  #define GNStringDeprecated( Message ) __attribute__((deprecated( Message )))
#else
  #define GNStringDeprecated( Message ) [[deprecated( Message )]]
#endif

class TGNString : public String
 {
 public:
  typedef String                                TNativeString;
  typedef std::char_traits<char>                traits_type;
  typedef std::allocator<char>                  allocator_type;
  typedef char                                  value_type;
  typedef size_t                                size_type;
  typedef ptrdiff_t                             difference_type;
  typedef char &                                reference;
  typedef char const &                          const_reference;
  typedef char *                                pointer;
  typedef char const *                          const_pointer;
  typedef char *                                iterator;
  typedef char const *                          const_iterator;
  typedef std::reverse_iterator<iterator>       reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  static size_type const              npos                                                                  = static_cast<size_type>( -1 );

 public:
                                      TGNString(void)                                                       = default;
                                      TGNString( TGNString const & Str )                                    = default;
                                      TGNString( TGNString && Str ) noexcept                                = default;
  explicit                            TGNString( TNativeString const & Str )                                : String( Str ) {}
  explicit                            TGNString( TNativeString && Str ) noexcept                            : String( std::move( Str ) ) {}
                                      TGNString( std::string const & Str )                                  { AssignBytes( Str.c_str(), Str.length() ); }
                                      TGNString( char const * Str )                                         : String( Str ? Str : "" ) {}
                                      TGNString( char const * Str, size_type Count )                        { AssignBytes( Str, Count ); }
                                      TGNString( size_type Count, char Value )                              { assign( Count, Value ); }
  template <class TInputIterator>
                                      TGNString( TInputIterator First, TInputIterator Last,
                                                 typename std::enable_if<!std::is_integral<TInputIterator>::value>::type * = NULL )
                                                                                                            { assign( First, Last ); }
                                      TGNString( std::initializer_list<char> Values )                       { assign( Values ); }
  GNStringDeprecated( "Byte-buffer construction is Arduino-specific; use a character range for platform compatibility." )
                                      TGNString( uint8_t const * Str, size_type Count )                     { AssignBytes( reinterpret_cast<char const *>( Str ), Count ); }
  GNStringDeprecated( "TGNString(char) is not part of std::string; use TGNString( 1, Value ) for platform compatibility." )
                                      TGNString( char Value )                                               : String( Value ) {}
  GNStringDeprecated( "Numeric TGNString construction is not part of std::string; use std::to_string() for platform compatibility." )
                                      TGNString( unsigned char Value, unsigned char Base = 10 )             : String( Value, Base ) {}
  GNStringDeprecated( "Numeric TGNString construction is not part of std::string; use std::to_string() for platform compatibility." )
                                      TGNString( int Value, unsigned char Base = 10 )                       : String( Value, Base ) {}
  GNStringDeprecated( "Numeric TGNString construction is not part of std::string; use std::to_string() for platform compatibility." )
                                      TGNString( unsigned int Value, unsigned char Base = 10 )              : String( Value, Base ) {}
  GNStringDeprecated( "Numeric TGNString construction is not part of std::string; use std::to_string() for platform compatibility." )
                                      TGNString( long Value, unsigned char Base = 10 )                      : String( Value, Base ) {}
  GNStringDeprecated( "Numeric TGNString construction is not part of std::string; use std::to_string() for platform compatibility." )
                                      TGNString( unsigned long Value, unsigned char Base = 10 )             : String( Value, Base ) {}
  GNStringDeprecated( "Numeric TGNString construction is not part of std::string; use std::to_string() for platform compatibility." )
                                      TGNString( long long Value, unsigned char Base = 10 )                 : String( Value, Base ) {}
  GNStringDeprecated( "Numeric TGNString construction is not part of std::string; use std::to_string() for platform compatibility." )
                                      TGNString( unsigned long long Value, unsigned char Base = 10 )        : String( Value, Base ) {}
  GNStringDeprecated( "Numeric TGNString construction is not part of std::string; use std::to_string() for platform compatibility." )
                                      TGNString( float Value, unsigned int DecimalPlaces = 2 )              : String( Value, DecimalPlaces ) {}
  GNStringDeprecated( "Numeric TGNString construction is not part of std::string; use std::to_string() for platform compatibility." )
                                      TGNString( double Value, unsigned int DecimalPlaces = 2 )             : String( Value, DecimalPlaces ) {}
                                      TGNString( __FlashStringHelper const * Str )                          : String( Str ) {}

  TGNString &                         operator=( TGNString const & Str )                                    = default;
  TGNString &                         operator=( TGNString && Str ) noexcept                                = default;
  TGNString &                         operator=( TNativeString const & Str )                                { String::operator=( Str ); return *this; }
  TGNString &                         operator=( TNativeString && Str ) noexcept                            { String::operator=( std::move( Str ) ); return *this; }
  TGNString &                         operator=( std::string const & Str )                                  { AssignBytes( Str.c_str(), Str.length() ); return *this; }
  TGNString &                         operator=( char const * Str )                                         { String::operator=( Str ? Str : "" ); return *this; }
  TGNString &                         operator=( char Value )                                               { String::operator=( TNativeString( Value ) ); return *this; }
  TGNString &                         operator=( std::initializer_list<char> Values )                       { return assign( Values ); }
  GNStringDeprecated( "Numeric assignment is not part of std::string; assign std::to_string() for platform compatibility." )
  TGNString &                         operator=( unsigned char Value )                                      { String::operator=( TNativeString( Value ) ); return *this; }
  GNStringDeprecated( "Numeric assignment is not part of std::string; assign std::to_string() for platform compatibility." )
  TGNString &                         operator=( int Value )                                                { String::operator=( TNativeString( Value ) ); return *this; }
  GNStringDeprecated( "Numeric assignment is not part of std::string; assign std::to_string() for platform compatibility." )
  TGNString &                         operator=( unsigned int Value )                                       { String::operator=( TNativeString( Value ) ); return *this; }
  GNStringDeprecated( "Numeric assignment is not part of std::string; assign std::to_string() for platform compatibility." )
  TGNString &                         operator=( long Value )                                               { String::operator=( TNativeString( Value ) ); return *this; }
  GNStringDeprecated( "Numeric assignment is not part of std::string; assign std::to_string() for platform compatibility." )
  TGNString &                         operator=( unsigned long Value )                                      { String::operator=( TNativeString( Value ) ); return *this; }
  GNStringDeprecated( "Numeric assignment is not part of std::string; assign std::to_string() for platform compatibility." )
  TGNString &                         operator=( long long Value )                                          { String::operator=( TNativeString( Value ) ); return *this; }
  GNStringDeprecated( "Numeric assignment is not part of std::string; assign std::to_string() for platform compatibility." )
  TGNString &                         operator=( unsigned long long Value )                                 { String::operator=( TNativeString( Value ) ); return *this; }
  GNStringDeprecated( "Numeric assignment is not part of std::string; assign std::to_string() for platform compatibility." )
  TGNString &                         operator=( float Value )                                              { String::operator=( TNativeString( Value ) ); return *this; }
  GNStringDeprecated( "Numeric assignment is not part of std::string; assign std::to_string() for platform compatibility." )
  TGNString &                         operator=( double Value )                                             { String::operator=( TNativeString( Value ) ); return *this; }
  TGNString &                         operator=( __FlashStringHelper const * Str )                          { String::operator=( TNativeString( Str ) ); return *this; }

  TGNString &                         assign( TGNString const & Str )                                       { String::operator=( Str.Base() ); return *this; }
  TGNString &                         assign( TGNString const & Str, size_type Pos, size_type Count = npos ) { return assign( Str.substr( Pos, Count ) ); }
  TGNString &                         assign( std::string const & Str )                                     { AssignBytes( Str.c_str(), Str.length() ); return *this; }
  TGNString &                         assign( char const * Str )                                            { String::operator=( Str ? Str : "" ); return *this; }
  TGNString &                         assign( char const * Str, size_type Count )                           { AssignBytes( Str, Count ); return *this; }
  TGNString &                         assign( size_type Count, char Value );
  template <class TInputIterator>
  typename std::enable_if<!std::is_integral<TInputIterator>::value, TGNString &>::type
                                      assign( TInputIterator First, TInputIterator Last );
  TGNString &                         assign( std::initializer_list<char> Values )                          { return assign( Values.begin(), Values.end() ); }

  reference                           at( size_type Pos )                                                   { assert( Pos < size() ); return String::operator[]( Pos ); }
  const_reference                     at( size_type Pos ) const                                             { assert( Pos < size() ); return c_str()[ Pos ]; }
  reference                           operator[]( size_type Pos )                                           { return String::operator[]( Pos ); }
  const_reference                     operator[]( size_type Pos ) const                                     { return c_str()[ Pos ]; }
  reference                           front(void)                                                           { assert( !empty() ); return String::operator[]( 0 ); }
  const_reference                     front(void) const                                                     { assert( !empty() ); return c_str()[ 0 ]; }
  reference                           back(void)                                                            { assert( !empty() ); return String::operator[]( size() - 1 ); }
  const_reference                     back(void) const                                                      { assert( !empty() ); return c_str()[ size() - 1 ]; }
  char const *                        c_str(void) const                                                     { return String::c_str(); }
  char *                              data(void)                                                            { return String::begin(); }
  char const *                        data(void) const                                                      { return String::c_str(); }

  iterator                            begin(void)                                                           { return String::begin(); }
  const_iterator                      begin(void) const                                                     { return String::begin(); }
  const_iterator                      cbegin(void) const                                                    { return String::begin(); }
  iterator                            end(void)                                                             { return String::end(); }
  const_iterator                      end(void) const                                                       { return String::end(); }
  const_iterator                      cend(void) const                                                      { return String::end(); }
  reverse_iterator                    rbegin(void)                                                          { return reverse_iterator( end() ); }
  const_reverse_iterator              rbegin(void) const                                                    { return const_reverse_iterator( end() ); }
  const_reverse_iterator              crbegin(void) const                                                   { return const_reverse_iterator( end() ); }
  reverse_iterator                    rend(void)                                                            { return reverse_iterator( begin() ); }
  const_reverse_iterator              rend(void) const                                                      { return const_reverse_iterator( begin() ); }
  const_reverse_iterator              crend(void) const                                                     { return const_reverse_iterator( begin() ); }

  bool                                empty(void) const                                                     { return String::isEmpty(); }
  size_type                           size(void) const                                                      { return String::length(); }
  size_type                           length(void) const                                                    { return String::length(); }
  size_type                           max_size(void) const                                                  { return npos - 1; }
  void                                reserve( size_type Size )                                             { String::reserve( Size ); }
  size_type                           capacity(void) const                                                  { return String::capacity(); }
  void                                shrink_to_fit(void);
  void                                clear(void)                                                           { String::clear(); }
  allocator_type                      get_allocator(void) const                                             { return allocator_type(); }

  TGNString &                         insert( size_type Pos, TGNString const & Str )                        { return insert( Pos, Str.c_str(), Str.length() ); }
  TGNString &                         insert( size_type Pos, TGNString const & Str,
                                               size_type StrPos, size_type Count = npos )                   { return insert( Pos, Str.substr( StrPos, Count ) ); }
  TGNString &                         insert( size_type Pos, char const * Str )                             { return insert( Pos, Str, Str ? strlen( Str ) : 0 ); }
  TGNString &                         insert( size_type Pos, char const * Str, size_type Count );
  TGNString &                         insert( size_type Pos, size_type Count, char Value )
   {
    TGNString Str( Count, Value );
    return insert( Pos, Str );
   }
  iterator                            insert( const_iterator Pos, char Value );
  iterator                            insert( const_iterator Pos, size_type Count, char Value );
  template <class TInputIterator>
  typename std::enable_if<!std::is_integral<TInputIterator>::value, iterator>::type
                                      insert( const_iterator Pos, TInputIterator First, TInputIterator Last );
  iterator                            insert( const_iterator Pos, std::initializer_list<char> Values )      { return insert( Pos, Values.begin(), Values.end() ); }

  TGNString &                         erase( size_type Pos = 0, size_type Count = npos );
  iterator                            erase( const_iterator Pos )                                            { return erase( Pos, Pos + 1 ); }
  iterator                            erase( const_iterator First, const_iterator Last );

  void                                push_back( char Value )                                               { String::concat( Value ); }
  void                                pop_back(void)                                                        { if( !empty() ) String::remove( size() - 1, 1 ); }

  TGNString &                         append( TGNString const & Str )                                       { String::concat( Str.Base() ); return *this; }
  TGNString &                         append( TGNString const & Str, size_type Pos,
                                               size_type Count = npos )                                    { return append( Str.substr( Pos, Count ) ); }
  TGNString &                         append( std::string const & Str )                                     { String::concat( Str.c_str(), Str.length() ); return *this; }
  TGNString &                         append( char const * Str )                                            { if( Str ) String::concat( Str ); return *this; }
  TGNString &                         append( char const * Str, size_type Count )                           { if( Str && Count ) String::concat( Str, Count ); return *this; }
  TGNString &                         append( size_type Count, char Value );
  template <class TInputIterator>
  typename std::enable_if<!std::is_integral<TInputIterator>::value, TGNString &>::type
                                      append( TInputIterator First, TInputIterator Last );
  TGNString &                         append( std::initializer_list<char> Values )                          { return append( Values.begin(), Values.end() ); }

  TGNString &                         operator+=( TGNString const & Str )                                   { return append( Str ); }
  GNStringDeprecated( "Appending Arduino String is not part of std::string; use TGNString or a character string for platform compatibility." )
  TGNString &                         operator+=( TNativeString const & Str )                               { String::concat( Str ); return *this; }
  TGNString &                         operator+=( std::string const & Str )                                 { return append( Str ); }
  TGNString &                         operator+=( char const * Str )                                        { return append( Str ); }
  TGNString &                         operator+=( char Value )                                              { push_back( Value ); return *this; }
  TGNString &                         operator+=( std::initializer_list<char> Values )                      { return append( Values ); }
  GNStringDeprecated( "Numeric operator+= is not part of std::string; append std::to_string() for platform compatibility." )
  TGNString &                         operator+=( unsigned char Value )                                     { String::concat( Value ); return *this; }
  GNStringDeprecated( "Numeric operator+= is not part of std::string; append std::to_string() for platform compatibility." )
  TGNString &                         operator+=( int Value )                                               { String::concat( Value ); return *this; }
  GNStringDeprecated( "Numeric operator+= is not part of std::string; append std::to_string() for platform compatibility." )
  TGNString &                         operator+=( unsigned int Value )                                      { String::concat( Value ); return *this; }
  GNStringDeprecated( "Numeric operator+= is not part of std::string; append std::to_string() for platform compatibility." )
  TGNString &                         operator+=( long Value )                                              { String::concat( Value ); return *this; }
  GNStringDeprecated( "Numeric operator+= is not part of std::string; append std::to_string() for platform compatibility." )
  TGNString &                         operator+=( unsigned long Value )                                     { String::concat( Value ); return *this; }
  GNStringDeprecated( "Numeric operator+= is not part of std::string; append std::to_string() for platform compatibility." )
  TGNString &                         operator+=( long long Value )                                         { String::concat( Value ); return *this; }
  GNStringDeprecated( "Numeric operator+= is not part of std::string; append std::to_string() for platform compatibility." )
  TGNString &                         operator+=( unsigned long long Value )                                { String::concat( Value ); return *this; }
  GNStringDeprecated( "Numeric operator+= is not part of std::string; append std::to_string() for platform compatibility." )
  TGNString &                         operator+=( float Value )                                             { String::concat( Value ); return *this; }
  GNStringDeprecated( "Numeric operator+= is not part of std::string; append std::to_string() for platform compatibility." )
  TGNString &                         operator+=( double Value )                                            { String::concat( Value ); return *this; }
  TGNString &                         operator+=( __FlashStringHelper const * Str )                         { String::concat( Str ); return *this; }

  int                                 compare( TGNString const & Str ) const                                { return Compare( c_str(), length(), Str.c_str(), Str.length() ); }
  int                                 compare( size_type Pos, size_type Count, TGNString const & Str ) const { return substr( Pos, Count ).compare( Str ); }
  int                                 compare( size_type Pos, size_type Count, TGNString const & Str,
                                                size_type StrPos, size_type StrCount = npos ) const          { return substr( Pos, Count ).compare( Str.substr( StrPos, StrCount ) ); }
  int                                 compare( char const * Str ) const                                     { return Compare( c_str(), length(), Str ? Str : "", Str ? strlen( Str ) : 0 ); }
  int                                 compare( size_type Pos, size_type Count, char const * Str ) const      { return substr( Pos, Count ).compare( Str ); }
  int                                 compare( size_type Pos, size_type Count,
                                                char const * Str, size_type StrCount ) const                 { return substr( Pos, Count ).compare( TGNString( Str, StrCount ) ); }

  TGNString &                         replace( size_type Pos, size_type Count, TGNString const & Str )      { return replace( Pos, Count, Str.c_str(), Str.length() ); }
  TGNString &                         replace( size_type Pos, size_type Count, TGNString const & Str,
                                                size_type StrPos, size_type StrCount = npos )                { return replace( Pos, Count, Str.substr( StrPos, StrCount ) ); }
  TGNString &                         replace( size_type Pos, size_type Count, char const * Str )           { return replace( Pos, Count, Str, Str ? strlen( Str ) : 0 ); }
  TGNString &                         replace( size_type Pos, size_type Count,
                                                char const * Str, size_type StrCount );
  TGNString &                         replace( size_type Pos, size_type Count,
                                                size_type StrCount, char Value )                            { return replace( Pos, Count, TGNString( StrCount, Value ) ); }
  TGNString &                         replace( const_iterator First, const_iterator Last,
                                                TGNString const & Str )                                     { return replace( First - cbegin(), Last - First, Str ); }
  TGNString &                         replace( const_iterator First, const_iterator Last, char const * Str ) { return replace( First - cbegin(), Last - First, Str ); }
  TGNString &                         replace( const_iterator First, const_iterator Last,
                                                size_type Count, char Value )                               { return replace( First - cbegin(), Last - First, Count, Value ); }
  template <class TInputIterator>
  typename std::enable_if<!std::is_integral<TInputIterator>::value, TGNString &>::type
                                      replace( const_iterator First, const_iterator Last,
                                               TInputIterator StrFirst, TInputIterator StrLast );
  TGNString &                         replace( const_iterator First, const_iterator Last,
                                                std::initializer_list<char> Values )                        { return replace( First, Last, Values.begin(), Values.end() ); }

  TGNString                           substr( size_type Pos = 0, size_type Count = npos ) const;

  size_type                           copy( char * Dst, size_type Count, size_type Pos = 0 ) const;

  void                                resize( size_type Count )                                             { resize( Count, '\0' ); }
  void                                resize( size_type Count, char Value );

  void                                swap( TGNString & Str );

  size_type                           find( TGNString const & Str, size_type Pos = 0 ) const                { return Find( Str.c_str(), Str.length(), Pos ); }
  size_type                           find( char const * Str, size_type Pos, size_type Count ) const        { return Find( Str ? Str : "", Str ? Count : 0, Pos ); }
  size_type                           find( char const * Str, size_type Pos = 0 ) const                     { return Find( Str ? Str : "", Str ? strlen( Str ) : 0, Pos ); }
  size_type                           find( char Value, size_type Pos = 0 ) const;

  size_type                           rfind( TGNString const & Str, size_type Pos = npos ) const            { return RFind( Str.c_str(), Str.length(), Pos ); }
  size_type                           rfind( char const * Str, size_type Pos, size_type Count ) const       { return RFind( Str ? Str : "", Str ? Count : 0, Pos ); }
  size_type                           rfind( char const * Str, size_type Pos = npos ) const                 { return RFind( Str ? Str : "", Str ? strlen( Str ) : 0, Pos ); }
  size_type                           rfind( char Value, size_type Pos = npos ) const;

  size_type                           find_first_of( TGNString const & Str, size_type Pos = 0 ) const       { return FindFirstOf( Str.c_str(), Str.length(), Pos, true ); }
  size_type                           find_first_of( char const * Str, size_type Pos, size_type Count ) const { return FindFirstOf( Str ? Str : "", Str ? Count : 0, Pos, true ); }
  size_type                           find_first_of( char const * Str, size_type Pos = 0 ) const            { return FindFirstOf( Str ? Str : "", Str ? strlen( Str ) : 0, Pos, true ); }
  size_type                           find_first_of( char Value, size_type Pos = 0 ) const                  { return find( Value, Pos ); }
  size_type                           find_last_of( TGNString const & Str, size_type Pos = npos ) const     { return FindLastOf( Str.c_str(), Str.length(), Pos, true ); }
  size_type                           find_last_of( char const * Str, size_type Pos, size_type Count ) const { return FindLastOf( Str ? Str : "", Str ? Count : 0, Pos, true ); }
  size_type                           find_last_of( char const * Str, size_type Pos = npos ) const          { return FindLastOf( Str ? Str : "", Str ? strlen( Str ) : 0, Pos, true ); }
  size_type                           find_last_of( char Value, size_type Pos = npos ) const                { return rfind( Value, Pos ); }
  size_type                           find_first_not_of( TGNString const & Str, size_type Pos = 0 ) const   { return FindFirstOf( Str.c_str(), Str.length(), Pos, false ); }
  size_type                           find_first_not_of( char const * Str, size_type Pos, size_type Count ) const { return FindFirstOf( Str ? Str : "", Str ? Count : 0, Pos, false ); }
  size_type                           find_first_not_of( char const * Str, size_type Pos = 0 ) const        { return FindFirstOf( Str ? Str : "", Str ? strlen( Str ) : 0, Pos, false ); }
  size_type                           find_first_not_of( char Value, size_type Pos = 0 ) const;
  size_type                           find_last_not_of( TGNString const & Str, size_type Pos = npos ) const { return FindLastOf( Str.c_str(), Str.length(), Pos, false ); }
  size_type                           find_last_not_of( char const * Str, size_type Pos, size_type Count ) const { return FindLastOf( Str ? Str : "", Str ? Count : 0, Pos, false ); }
  size_type                           find_last_not_of( char const * Str, size_type Pos = npos ) const      { return FindLastOf( Str ? Str : "", Str ? strlen( Str ) : 0, Pos, false ); }
  size_type                           find_last_not_of( char Value, size_type Pos = npos ) const;

  bool                                starts_with( TGNString const & Str ) const                            { return length() >= Str.length() && compare( 0, Str.length(), Str ) == 0; }
  bool                                starts_with( char const * Str ) const                                 { return starts_with( TGNString( Str ) ); }
  bool                                starts_with( char Value ) const                                       { return !empty() && front() == Value; }
  bool                                ends_with( TGNString const & Str ) const
   {
    return length() >= Str.length() && compare( length() - Str.length(), Str.length(), Str ) == 0;
   }
  bool                                ends_with( char const * Str ) const                                   { return ends_with( TGNString( Str ) ); }
  bool                                ends_with( char Value ) const                                         { return !empty() && back() == Value; }
  bool                                contains( TGNString const & Str ) const                               { return find( Str ) != npos; }
  bool                                contains( char const * Str ) const                                    { return find( Str ) != npos; }
  bool                                contains( char Value ) const                                          { return find( Value ) != npos; }

  TNativeString                       ToArduinoString(void) const                                           { return Base(); }
  std::string                         ToStdString(void) const                                               { return std::string( c_str(), length() ); }

  static TGNString                    FromArduinoString( TNativeString const & Str )                        { return TGNString( Str ); }
  static TGNString                    FromStdString( std::string const & Str )                              { return TGNString( Str ); }

                                      operator std::string(void) const                                      { return ToStdString(); }


  GNStringDeprecated( "TGNString::isEmpty() is not part of std::string; use empty() for platform compatibility." )
  bool                                isEmpty(void) const                                                   { return String::isEmpty(); }

  GNStringDeprecated( "TGNString::concat() is not part of std::string; use append() or operator+= for platform compatibility." )
  bool                                concat( TNativeString const & Str )                                   { return String::concat( Str ); }
  GNStringDeprecated( "TGNString::concat() is not part of std::string; use append() or operator+= for platform compatibility." )
  bool                                concat( char const * Str )                                            { return String::concat( Str ); }
  GNStringDeprecated( "TGNString::concat() is not part of std::string; use append() for platform compatibility." )
  bool                                concat( char const * Str, unsigned int Count )                        { return String::concat( Str, Count ); }
  GNStringDeprecated( "TGNString::concat() is not part of std::string; use append() for platform compatibility." )
  bool                                concat( uint8_t const * Str, unsigned int Count )                     { return String::concat( reinterpret_cast<char const *>( Str ), Count ); }
  GNStringDeprecated( "TGNString::concat() is not part of std::string; use push_back() for platform compatibility." )
  bool                                concat( char Value )                                                  { return String::concat( Value ); }
  GNStringDeprecated( "Numeric concat() is not part of std::string; append std::to_string() for platform compatibility." )
  bool                                concat( unsigned char Value )                                         { return String::concat( Value ); }
  GNStringDeprecated( "Numeric concat() is not part of std::string; append std::to_string() for platform compatibility." )
  bool                                concat( int Value )                                                   { return String::concat( Value ); }
  GNStringDeprecated( "Numeric concat() is not part of std::string; append std::to_string() for platform compatibility." )
  bool                                concat( unsigned int Value )                                          { return String::concat( Value ); }
  GNStringDeprecated( "Numeric concat() is not part of std::string; append std::to_string() for platform compatibility." )
  bool                                concat( long Value )                                                  { return String::concat( Value ); }
  GNStringDeprecated( "Numeric concat() is not part of std::string; append std::to_string() for platform compatibility." )
  bool                                concat( unsigned long Value )                                         { return String::concat( Value ); }
  GNStringDeprecated( "Numeric concat() is not part of std::string; append std::to_string() for platform compatibility." )
  bool                                concat( long long Value )                                             { return String::concat( Value ); }
  GNStringDeprecated( "Numeric concat() is not part of std::string; append std::to_string() for platform compatibility." )
  bool                                concat( unsigned long long Value )                                    { return String::concat( Value ); }
  GNStringDeprecated( "Numeric concat() is not part of std::string; append std::to_string() for platform compatibility." )
  bool                                concat( float Value )                                                 { return String::concat( Value ); }
  GNStringDeprecated( "Numeric concat() is not part of std::string; append std::to_string() for platform compatibility." )
  bool                                concat( double Value )                                                { return String::concat( Value ); }
  GNStringDeprecated( "Flash-string concat() is Arduino-specific; use append() with a normal character string for platform compatibility." )
  bool                                concat( __FlashStringHelper const * Str )                             { return String::concat( Str ); }

  GNStringDeprecated( "TGNString::compareTo() is not part of std::string; use compare() for platform compatibility." )
  int                                 compareTo( TNativeString const & Str ) const                          { return String::compareTo( Str ); }
  GNStringDeprecated( "TGNString::equals() is not part of std::string; use operator== or compare() for platform compatibility." )
  bool                                equals( TNativeString const & Str ) const                             { return String::equals( Str ); }
  GNStringDeprecated( "TGNString::equals() is not part of std::string; use operator== or compare() for platform compatibility." )
  bool                                equals( char const * Str ) const                                      { return String::equals( Str ); }
  GNStringDeprecated( "TGNString::equals() is not part of std::string; use operator== or compare() for platform compatibility." )
  bool                                equals( __FlashStringHelper const * Str ) const                       { return String::equals( TNativeString( Str ) ); }
  GNStringDeprecated( "TGNString::equalsIgnoreCase() is not part of std::string; use a platform-independent comparison helper." )
  bool                                equalsIgnoreCase( TNativeString const & Str ) const                   { return String::equalsIgnoreCase( Str ); }
  GNStringDeprecated( "TGNString::equalsIgnoreCase() is not part of std::string; use a platform-independent comparison helper." )
  bool                                equalsIgnoreCase( __FlashStringHelper const * Str ) const             { return String::equalsIgnoreCase( TNativeString( Str ) ); }
  GNStringDeprecated( "TGNString::equalsConstantTime() is not part of std::string; use a platform-independent constant-time comparison helper." )
  unsigned char                       equalsConstantTime( TNativeString const & Str ) const                 { return String::equalsConstantTime( Str ); }

  GNStringDeprecated( "TGNString::startsWith() is not part of std::string; use starts_with() for platform compatibility." )
  bool                                startsWith( TNativeString const & Str ) const                         { return String::startsWith( Str ); }
  GNStringDeprecated( "TGNString::startsWith() is not part of std::string; use starts_with() for platform compatibility." )
  bool                                startsWith( char const * Str ) const                                  { return String::startsWith( Str ); }
  GNStringDeprecated( "TGNString::startsWith() is not part of std::string; use starts_with() for platform compatibility." )
  bool                                startsWith( __FlashStringHelper const * Str ) const                   { return String::startsWith( TNativeString( Str ) ); }
  GNStringDeprecated( "TGNString::startsWith() is not part of std::string; use compare() with an offset for platform compatibility." )
  bool                                startsWith( TNativeString const & Str, unsigned int Offset ) const    { return String::startsWith( Str, Offset ); }
  GNStringDeprecated( "TGNString::startsWith() is not part of std::string; use compare() with an offset for platform compatibility." )
  bool                                startsWith( __FlashStringHelper const * Str, unsigned int Offset ) const
                                                                                                            { return String::startsWith( TNativeString( Str ), Offset ); }
  GNStringDeprecated( "TGNString::endsWith() is not part of std::string; use ends_with() for platform compatibility." )
  bool                                endsWith( TNativeString const & Str ) const                           { return String::endsWith( Str ); }
  GNStringDeprecated( "TGNString::endsWith() is not part of std::string; use ends_with() for platform compatibility." )
  bool                                endsWith( char const * Str ) const                                    { return String::endsWith( Str ); }
  GNStringDeprecated( "TGNString::endsWith() is not part of std::string; use ends_with() for platform compatibility." )
  bool                                endsWith( __FlashStringHelper const * Str ) const                     { return String::endsWith( TNativeString( Str ) ); }

  GNStringDeprecated( "TGNString::charAt() is not part of std::string; use at() or operator[] for platform compatibility." )
  char                                charAt( unsigned int Index ) const                                    { return String::charAt( Index ); }
  GNStringDeprecated( "TGNString::setCharAt() is not part of std::string; use at() or operator[] for platform compatibility." )
  void                                setCharAt( unsigned int Index, char Value )                           { String::setCharAt( Index, Value ); }
  GNStringDeprecated( "TGNString::getBytes() is not part of std::string; use copy() for platform compatibility." )
  void                                getBytes( unsigned char * Dst, unsigned int Size,
                                                unsigned int Index = 0 ) const                              { String::getBytes( Dst, Size, Index ); }
  GNStringDeprecated( "TGNString::toCharArray() is not part of std::string; use copy() and explicit zero termination for platform compatibility." )
  void                                toCharArray( char * Dst, unsigned int Size,
                                                   unsigned int Index = 0 ) const                            { String::toCharArray( Dst, Size, Index ); }

  GNStringDeprecated( "TGNString::indexOf() is not part of std::string; use find() for platform compatibility." )
  int                                 indexOf( char Value, unsigned int From = 0 ) const                    { return String::indexOf( Value, From ); }
  GNStringDeprecated( "TGNString::indexOf() is not part of std::string; use find() for platform compatibility." )
  int                                 indexOf( char const * Str, unsigned int From = 0 ) const              { return String::indexOf( TNativeString( Str ), From ); }
  GNStringDeprecated( "TGNString::indexOf() is not part of std::string; use find() for platform compatibility." )
  int                                 indexOf( __FlashStringHelper const * Str, unsigned int From = 0 ) const
                                                                                                            { return String::indexOf( TNativeString( Str ), From ); }
  GNStringDeprecated( "TGNString::indexOf() is not part of std::string; use find() for platform compatibility." )
  int                                 indexOf( TNativeString const & Str, unsigned int From = 0 ) const      { return String::indexOf( Str, From ); }
  GNStringDeprecated( "TGNString::lastIndexOf() is not part of std::string; use rfind() for platform compatibility." )
  int                                 lastIndexOf( char Value ) const                                       { return String::lastIndexOf( Value ); }
  GNStringDeprecated( "TGNString::lastIndexOf() is not part of std::string; use rfind() for platform compatibility." )
  int                                 lastIndexOf( char Value, unsigned int From ) const                     { return String::lastIndexOf( Value, From ); }
  GNStringDeprecated( "TGNString::lastIndexOf() is not part of std::string; use rfind() for platform compatibility." )
  int                                 lastIndexOf( TNativeString const & Str ) const                        { return String::lastIndexOf( Str ); }
  GNStringDeprecated( "TGNString::lastIndexOf() is not part of std::string; use rfind() for platform compatibility." )
  int                                 lastIndexOf( TNativeString const & Str, unsigned int From ) const      { return String::lastIndexOf( Str, From ); }
  GNStringDeprecated( "TGNString::lastIndexOf() is not part of std::string; use rfind() for platform compatibility." )
  int                                 lastIndexOf( __FlashStringHelper const * Str ) const                  { return String::lastIndexOf( TNativeString( Str ) ); }
  GNStringDeprecated( "TGNString::lastIndexOf() is not part of std::string; use rfind() for platform compatibility." )
  int                                 lastIndexOf( __FlashStringHelper const * Str, unsigned int From ) const
                                                                                                            { return String::lastIndexOf( TNativeString( Str ), From ); }
  GNStringDeprecated( "TGNString::substring() is not part of std::string; use substr() for platform compatibility." )
  TGNString                           substring( unsigned int From ) const                                  { return TGNString( String::substring( From ) ); }
  GNStringDeprecated( "TGNString::substring() is not part of std::string; use substr() for platform compatibility." )
  TGNString                           substring( unsigned int From, unsigned int To ) const                 { return TGNString( String::substring( From, To ) ); }

  GNStringDeprecated( "TGNString::replace(char,char) is not part of std::string; use std::replace() for platform compatibility." )
  void                                replace( char Find, char Replace )                                    { String::replace( Find, Replace ); }
  GNStringDeprecated( "Arduino String replacement is not part of std::string; use replace(position,count,text) for platform compatibility." )
  void                                replace( TNativeString const & Find, TNativeString const & Replace )  { String::replace( Find, Replace ); }
  GNStringDeprecated( "Arduino String replacement is not part of std::string; use replace(position,count,text) for platform compatibility." )
  void                                replace( char const * Find, TNativeString const & Replace )           { String::replace( TNativeString( Find ), Replace ); }
  GNStringDeprecated( "Arduino String replacement is not part of std::string; use replace(position,count,text) for platform compatibility." )
  void                                replace( __FlashStringHelper const * Find, TNativeString const & Replace )
                                                                                                            { String::replace( TNativeString( Find ), Replace ); }
  GNStringDeprecated( "Arduino String replacement is not part of std::string; use replace(position,count,text) for platform compatibility." )
  void                                replace( char const * Find, char const * Replace )                    { String::replace( TNativeString( Find ), TNativeString( Replace ) ); }
  GNStringDeprecated( "Arduino String replacement is not part of std::string; use replace(position,count,text) for platform compatibility." )
  void                                replace( __FlashStringHelper const * Find, char const * Replace )     { String::replace( TNativeString( Find ), TNativeString( Replace ) ); }
  GNStringDeprecated( "Arduino String replacement is not part of std::string; use replace(position,count,text) for platform compatibility." )
  void                                replace( __FlashStringHelper const * Find, __FlashStringHelper const * Replace )
                                                                                                            { String::replace( TNativeString( Find ), TNativeString( Replace ) ); }
  GNStringDeprecated( "TGNString::remove() is not part of std::string; use erase() for platform compatibility." )
  void                                remove( unsigned int Index )                                          { String::remove( Index ); }
  GNStringDeprecated( "TGNString::remove() is not part of std::string; use erase() for platform compatibility." )
  void                                remove( unsigned int Index, unsigned int Count )                      { String::remove( Index, Count ); }
  GNStringDeprecated( "TGNString::toLowerCase() is not part of std::string; use std::transform() for platform compatibility." )
  void                                toLowerCase(void)                                                     { String::toLowerCase(); }
  GNStringDeprecated( "TGNString::toUpperCase() is not part of std::string; use std::transform() for platform compatibility." )
  void                                toUpperCase(void)                                                     { String::toUpperCase(); }
  GNStringDeprecated( "TGNString::trim() is not part of std::string; use a platform-independent trim helper." )
  void                                trim(void)                                                            { String::trim(); }
  GNStringDeprecated( "TGNString::toInt() is not part of std::string; use std::stol() for platform compatibility." )
  long                                toInt(void) const                                                     { return String::toInt(); }
  GNStringDeprecated( "TGNString::toFloat() is not part of std::string; use std::stof() for platform compatibility." )
  float                               toFloat(void) const                                                   { return String::toFloat(); }
  GNStringDeprecated( "TGNString::toDouble() is not part of std::string; use std::stod() for platform compatibility." )
  double                              toDouble(void) const                                                  { return String::toDouble(); }
  GNStringDeprecated( "Boolean conversion is not part of std::string; use empty() or another explicit state test for platform compatibility." )
  explicit                            operator bool(void) const                                             { return static_cast<bool>( Base() ); }

 private:
  TNativeString &                     Base(void)                                                            { return static_cast<TNativeString &>( *this ); }
  TNativeString const &               Base(void) const                                                      { return static_cast<TNativeString const &>( *this ); }

  void                                AssignBytes( char const * Str, size_type Count );

  static int                          Compare( char const * Left, size_type LeftLength,
                                               char const * Right, size_type RightLength );

  size_type                           Find( char const * Str, size_type StrLength, size_type Pos ) const;
  size_type                           RFind( char const * Str, size_type StrLength, size_type Pos ) const;
  static bool                         ContainsChar( char const * Str, size_type StrLength, char Value );
  size_type                           FindFirstOf( char const * Str, size_type StrLength,
                                                   size_type Pos, bool Match ) const;
  size_type                           FindLastOf( char const * Str, size_type StrLength,
                                                  size_type Pos, bool Match ) const;
 };


inline TGNString & TGNString::assign( size_type Count, char Value )
 {
  String::clear();
  String::reserve( Count );
  while( Count-- )
    String::concat( Value );
  return *this;
 }

template <class TInputIterator>
inline typename std::enable_if<!std::is_integral<TInputIterator>::value, TGNString &>::type
TGNString::assign( TInputIterator First, TInputIterator Last )
 {
  String::clear();
  while( First != Last )
   {
    String::concat( *First );
    ++First;
   }
  return *this;
 }

inline void TGNString::shrink_to_fit(void)
 {
  TNativeString Str;
  Str.reserve( length() );
  Str.concat( c_str(), length() );
  String::operator=( std::move( Str ) );
 }

inline TGNString & TGNString::insert( size_type Pos, char const * Str, size_type Count )
 {
  assert( Pos <= size() );
  if( Pos > size() )
    Pos = size();

  TNativeString Result = String::substring( 0, Pos );
  if( Str && Count )
    Result.concat( Str, Count );
  Result.concat( String::substring( Pos ) );
  String::operator=( Result );
  return *this;
 }

inline TGNString::iterator TGNString::insert( const_iterator Pos, char Value )
 {
  size_type Index = Pos - cbegin();
  insert( Index, 1, Value );
  return begin() + Index;
 }

inline TGNString::iterator TGNString::insert( const_iterator Pos, size_type Count, char Value )
 {
  size_type Index = Pos - cbegin();
  insert( Index, Count, Value );
  return begin() + Index;
 }

template <class TInputIterator>
inline typename std::enable_if<!std::is_integral<TInputIterator>::value, TGNString::iterator>::type
TGNString::insert( const_iterator Pos, TInputIterator First, TInputIterator Last )
 {
  size_type Index = Pos - cbegin();
  TGNString Str;
  Str.assign( First, Last );
  insert( Index, Str );
  return begin() + Index;
 }

inline TGNString & TGNString::erase( size_type Pos, size_type Count )
 {
  assert( Pos <= size() );
  if( Pos > size() )
    return *this;

  String::remove( Pos, Count == npos ? size() - Pos : Count );
  return *this;
 }

inline TGNString::iterator TGNString::erase( const_iterator First, const_iterator Last )
 {
  size_type Pos = First - cbegin();
  erase( Pos, Last - First );
  return begin() + Pos;
 }

inline TGNString & TGNString::append( size_type Count, char Value )
 {
  while( Count-- )
    String::concat( Value );
  return *this;
 }

template <class TInputIterator>
inline typename std::enable_if<!std::is_integral<TInputIterator>::value, TGNString &>::type
TGNString::append( TInputIterator First, TInputIterator Last )
 {
  while( First != Last )
   {
    String::concat( *First );
    ++First;
   }
  return *this;
 }

inline TGNString & TGNString::replace( size_type Pos, size_type Count,
                                       char const * Str, size_type StrCount )
 {
  assert( Pos <= size() );
  if( Pos > size() )
    return *this;

  erase( Pos, Count );
  return insert( Pos, Str, StrCount );
 }

template <class TInputIterator>
inline typename std::enable_if<!std::is_integral<TInputIterator>::value, TGNString &>::type
TGNString::replace( const_iterator First, const_iterator Last,
                    TInputIterator StrFirst, TInputIterator StrLast )
 {
  TGNString Str;
  Str.assign( StrFirst, StrLast );
  return replace( First, Last, Str );
 }

inline TGNString TGNString::substr( size_type Pos, size_type Count ) const
 {
  assert( Pos <= size() );
  if( Pos > size() )
    return TGNString();

  size_type EndPos = Count == npos || Count > size() - Pos ? size() : Pos + Count;
  return TGNString( String::substring( Pos, EndPos ) );
 }

inline TGNString::size_type TGNString::copy( char * Dst, size_type Count, size_type Pos ) const
 {
  assert( Pos <= size() );
  if( !Dst || Pos > size() )
    return 0;

  Count = std::min( Count, size() - Pos );
  memcpy( Dst, c_str() + Pos, Count );
  return Count;
 }

inline void TGNString::resize( size_type Count, char Value )
 {
  if( Count < size() )
    String::remove( Count );
  else
    append( Count - size(), Value );
 }

inline void TGNString::swap( TGNString & Str )
 {
  TNativeString Temp( Base() );
  String::operator=( Str.Base() );
  Str.Base().operator=( Temp );
 }

inline TGNString::size_type TGNString::find( char Value, size_type Pos ) const
 {
  for( size_type Index = Pos; Index < size(); ++Index )
    if( String::operator[]( Index ) == Value )
      return Index;
  return npos;
 }

inline TGNString::size_type TGNString::rfind( char Value, size_type Pos ) const
 {
  if( empty() )
    return npos;

  size_type Index = Pos == npos || Pos >= size() ? size() - 1 : Pos;
  for( ;; )
   {
    if( String::operator[]( Index ) == Value )
      return Index;
    if( Index == 0 )
      break;
    --Index;
   }
  return npos;
 }

inline TGNString::size_type TGNString::find_first_not_of( char Value, size_type Pos ) const
 {
  for( size_type Index = Pos; Index < size(); ++Index )
    if( String::operator[]( Index ) != Value )
      return Index;
  return npos;
 }

inline TGNString::size_type TGNString::find_last_not_of( char Value, size_type Pos ) const
 {
  if( empty() )
    return npos;

  size_type Index = Pos == npos || Pos >= size() ? size() - 1 : Pos;
  for( ;; )
   {
    if( String::operator[]( Index ) != Value )
      return Index;
    if( Index == 0 )
      break;
    --Index;
   }
  return npos;
 }

inline void TGNString::AssignBytes( char const * Str, size_type Count )
 {
  String::clear();
  if( !Str || Count == 0 )
    return;

  String::reserve( Count );
  String::concat( Str, Count );
 }

inline int TGNString::Compare( char const * Left, size_type LeftLength,
                               char const * Right, size_type RightLength )
 {
  size_type Count = std::min( LeftLength, RightLength );
  int Result = Count ? memcmp( Left, Right, Count ) : 0;
  if( Result )
    return Result;
  if( LeftLength < RightLength )
    return -1;
  if( LeftLength > RightLength )
    return 1;
  return 0;
 }

inline TGNString::size_type TGNString::Find( char const * Str, size_type StrLength, size_type Pos ) const
 {
  if( Pos > size() )
    return npos;
  if( StrLength == 0 )
    return Pos;
  if( StrLength > size() - Pos )
    return npos;

  for( size_type Index = Pos; Index <= size() - StrLength; ++Index )
    if( memcmp( c_str() + Index, Str, StrLength ) == 0 )
      return Index;
  return npos;
 }

inline TGNString::size_type TGNString::RFind( char const * Str, size_type StrLength, size_type Pos ) const
 {
  if( StrLength > size() )
    return npos;
  if( StrLength == 0 )
    return std::min( Pos, size() );

  size_type Index = std::min( Pos, size() - StrLength );
  for( ;; )
   {
    if( memcmp( c_str() + Index, Str, StrLength ) == 0 )
      return Index;
    if( Index == 0 )
      break;
    --Index;
   }
  return npos;
 }

inline bool TGNString::ContainsChar( char const * Str, size_type StrLength, char Value )
 {
  for( size_type Index = 0; Index < StrLength; ++Index )
    if( Str[ Index ] == Value )
      return true;
  return false;
 }

inline TGNString::size_type TGNString::FindFirstOf( char const * Str, size_type StrLength,
                                                    size_type Pos, bool Match ) const
 {
  for( size_type Index = Pos; Index < size(); ++Index )
    if( ContainsChar( Str, StrLength, String::operator[]( Index ) ) == Match )
      return Index;
  return npos;
 }

inline TGNString::size_type TGNString::FindLastOf( char const * Str, size_type StrLength,
                                                   size_type Pos, bool Match ) const
 {
  if( empty() )
    return npos;

  size_type Index = Pos == npos || Pos >= size() ? size() - 1 : Pos;
  for( ;; )
   {
    if( ContainsChar( Str, StrLength, String::operator[]( Index ) ) == Match )
      return Index;
    if( Index == 0 )
      break;
    --Index;
   }
  return npos;
 }

inline TGNString operator+( TGNString Left, TGNString const & Right )
 {
  Left.append( Right );
  return Left;
 }

template <class TLeft, class TRight>
inline typename std::enable_if<
  std::is_same<typename std::decay<TLeft>::type, TGNString>::value &&
  std::is_same<typename std::decay<TRight>::type, TGNString::TNativeString>::value, TGNString>::type
operator+( TLeft && Left, TRight && Right )
 {
  TGNString Result( std::forward<TLeft>( Left ) );
  static_cast<String &>( Result ).concat( Right );
  return Result;
 }

template <class TLeft, class TRight>
inline typename std::enable_if<
  std::is_same<typename std::decay<TLeft>::type, TGNString::TNativeString>::value &&
  std::is_same<typename std::decay<TRight>::type, TGNString>::value, TGNString>::type
operator+( TLeft && Left, TRight && Right )
 {
  TGNString Result( Left );
  Result.append( Right );
  return Result;
 }

inline TGNString operator+( TGNString Left, std::string const & Right )
 {
  Left.append( Right );
  return Left;
 }

inline TGNString operator+( std::string const & Left, TGNString const & Right )
 {
  TGNString Result( Left );
  Result.append( Right );
  return Result;
 }

inline TGNString operator+( TGNString Left, char const * Right )
 {
  Left.append( Right );
  return Left;
 }

inline TGNString operator+( char const * Left, TGNString const & Right )
 {
  TGNString Result( Left );
  Result.append( Right );
  return Result;
 }

inline TGNString operator+( TGNString Left, char Right )
 {
  Left.push_back( Right );
  return Left;
 }

inline TGNString operator+( char Left, TGNString const & Right )
 {
  TGNString Result( static_cast<TGNString::size_type>( 1 ), Left );
  Result.append( Right );
  return Result;
 }

inline bool operator==( TGNString const & Left, char const * Right )
 {
  return Left.compare( Right ) == 0;
 }

inline bool operator==( char const * Left, TGNString const & Right )
 {
  return Right.compare( Left ) == 0;
 }

inline bool operator!=( TGNString const & Left, char const * Right )
 {
  return Left.compare( Right ) != 0;
 }

inline bool operator!=( char const * Left, TGNString const & Right )
 {
  return Right.compare( Left ) != 0;
 }

inline bool operator<( char const * Left, TGNString const & Right )
 {
  return Right.compare( Left ) > 0;
 }

inline bool operator<=( char const * Left, TGNString const & Right )
 {
  return Right.compare( Left ) >= 0;
 }

inline bool operator>( char const * Left, TGNString const & Right )
 {
  return Right.compare( Left ) < 0;
 }

inline bool operator>=( char const * Left, TGNString const & Right )
 {
  return Right.compare( Left ) <= 0;
 }

template<typename TString,
         typename std::enable_if<std::is_same<typename std::decay<TString>::type, std::string>::value, int>::type = 0>
inline bool operator==( TGNString const & Left, TString const & Right )
 {
  return Left.ToStdString() == Right;
 }

template<typename TString,
         typename std::enable_if<std::is_same<typename std::decay<TString>::type, std::string>::value, int>::type = 0>
inline bool operator==( TString const & Left, TGNString const & Right )
 {
  return Left == Right.ToStdString();
 }

template<typename TString,
         typename std::enable_if<std::is_same<typename std::decay<TString>::type, std::string>::value, int>::type = 0>
inline bool operator!=( TGNString const & Left, TString const & Right )
 {
  return Left.ToStdString() != Right;
 }

template<typename TString,
         typename std::enable_if<std::is_same<typename std::decay<TString>::type, std::string>::value, int>::type = 0>
inline bool operator!=( TString const & Left, TGNString const & Right )
 {
  return Left != Right.ToStdString();
 }

template<typename TString,
         typename std::enable_if<std::is_same<typename std::decay<TString>::type, std::string>::value, int>::type = 0>
inline bool operator<( TGNString const & Left, TString const & Right )
 {
  return Left.ToStdString() < Right;
 }

template<typename TString,
         typename std::enable_if<std::is_same<typename std::decay<TString>::type, std::string>::value, int>::type = 0>
inline bool operator<( TString const & Left, TGNString const & Right )
 {
  return Left < Right.ToStdString();
 }

template<typename TString,
         typename std::enable_if<std::is_same<typename std::decay<TString>::type, std::string>::value, int>::type = 0>
inline bool operator<=( TGNString const & Left, TString const & Right )
 {
  return Left.ToStdString() <= Right;
 }

template<typename TString,
         typename std::enable_if<std::is_same<typename std::decay<TString>::type, std::string>::value, int>::type = 0>
inline bool operator<=( TString const & Left, TGNString const & Right )
 {
  return Left <= Right.ToStdString();
 }

template<typename TString,
         typename std::enable_if<std::is_same<typename std::decay<TString>::type, std::string>::value, int>::type = 0>
inline bool operator>( TGNString const & Left, TString const & Right )
 {
  return Left.ToStdString() > Right;
 }

template<typename TString,
         typename std::enable_if<std::is_same<typename std::decay<TString>::type, std::string>::value, int>::type = 0>
inline bool operator>( TString const & Left, TGNString const & Right )
 {
  return Left > Right.ToStdString();
 }

template<typename TString,
         typename std::enable_if<std::is_same<typename std::decay<TString>::type, std::string>::value, int>::type = 0>
inline bool operator>=( TGNString const & Left, TString const & Right )
 {
  return Left.ToStdString() >= Right;
 }

template<typename TString,
         typename std::enable_if<std::is_same<typename std::decay<TString>::type, std::string>::value, int>::type = 0>
inline bool operator>=( TString const & Left, TGNString const & Right )
 {
  return Left >= Right.ToStdString();
 }

#define GNStringNumericPlus( Type ) \
  GNStringDeprecated( "Numeric operator+ is not part of std::string; use std::to_string() for platform compatibility." ) \
  inline TGNString operator+( TGNString Left, Type Right ) \
   { \
    static_cast<String &>( Left ).concat( Right ); \
    return Left; \
   } \
  GNStringDeprecated( "Numeric operator+ is not part of std::string; use std::to_string() for platform compatibility." ) \
  inline TGNString operator+( Type Left, TGNString const & Right ) \
   { \
    TGNString Result{ TGNString::TNativeString( Left ) }; \
    Result.append( Right ); \
    return Result; \
   }

GNStringNumericPlus( unsigned char )
GNStringNumericPlus( int )
GNStringNumericPlus( unsigned int )
GNStringNumericPlus( long )
GNStringNumericPlus( unsigned long )
GNStringNumericPlus( long long )
GNStringNumericPlus( unsigned long long )
GNStringNumericPlus( float )
GNStringNumericPlus( double )

#undef GNStringNumericPlus

#undef GNStringDeprecated

#else

typedef std::string TGNString;

#endif

//****************************************************************
// TGnStringLengthCounter / GnBuildStringExact
//****************************************************************

class TGnStringLengthCounter
 {
 public:
  typedef TGNString::size_type         size_type;

 public:
                                      TGnStringLengthCounter(void) = default;
  explicit                            TGnStringLengthCounter( size_type Length ) : FLength( Length ) {}

  size_type                           length(void) const                                           { return FLength; }
  size_type                           size(void) const                                             { return FLength; }
  bool                                empty(void) const                                            { return FLength == 0; }

  bool                                DoReserve( TGNString & Str ) const
   {
    Str.clear();
    if( FLength == 0 || Str.capacity() >= FLength )
      return true;
#if defined( GNPlatformESP )
    return static_cast<String &>( Str ).reserve( FLength ) && Str.capacity() >= FLength;
#else
    try
     {
      Str.reserve( FLength );
     }
    catch( ... )
     {
      return false;
     }
    return Str.capacity() >= FLength;
#endif
   }

  void                                AddLength( size_type Count )                                 { FLength += Count; }
  void                                push_back( char )                                            { ++FLength; }

  TGnStringLengthCounter &            append( char const * Text )
   {
    if( Text != nullptr )
      FLength += strlen( Text );
    return *this;
   }

  TGnStringLengthCounter &            append( char const * Text, size_type Count )
   {
    if( Text != nullptr )
      FLength += Count;
    return *this;
   }

  TGnStringLengthCounter &            append( size_type Count, char )                              { FLength += Count; return *this; }
  TGnStringLengthCounter &            append( TGNString const & Text )                             { FLength += Text.length(); return *this; }
#if defined( GNPlatformESP )
  TGnStringLengthCounter &            append( String const & Text )                                { FLength += Text.length(); return *this; }
  TGnStringLengthCounter &            append( std::string const & Text )                           { FLength += Text.length(); return *this; }
#endif

  TGnStringLengthCounter &            operator+=( char Value )                                     { push_back( Value ); return *this; }
  TGnStringLengthCounter &            operator+=( char const * Text )                              { return append( Text ); }
  TGnStringLengthCounter &            operator+=( TGNString const & Text )                         { return append( Text ); }
#if defined( GNPlatformESP )
  TGnStringLengthCounter &            operator+=( String const & Text )                            { return append( Text ); }
  TGnStringLengthCounter &            operator+=( std::string const & Text )                       { return append( Text ); }
  TGnStringLengthCounter &            operator+=( __FlashStringHelper const * Text )
   {
    if( Text == nullptr )
      return *this;
    PGM_P Position = reinterpret_cast<PGM_P>( Text );
    while( pgm_read_byte( Position++ ) != '\0' )
      ++FLength;
    return *this;
   }
#endif

 private:
  size_type                           FLength = 0;
 };

//****************************************************************
// TGnStringAppendWriter
//****************************************************************

// Append-only writer for APIs whose native string writer replaces the target content.
class TGnStringAppendWriter
 {
 public:
  explicit                            TGnStringAppendWriter( TGNString & Target ) : FTarget( Target ) {}

  size_t                              write( uint8_t Value )
   {
#if defined( GNPlatformESP )
    return static_cast<String &>( FTarget ).concat( static_cast<char>( Value ) ) ? 1 : 0;
#else
    try
     {
      FTarget.push_back( static_cast<char>( Value ) );
     }
    catch( ... )
     {
      return 0;
     }
    return 1;
#endif
   }

  size_t                              write( uint8_t const * Data, size_t Length )
   {
    if( Data == nullptr || Length == 0 )
      return 0;
#if defined( GNPlatformESP )
    return static_cast<String &>( FTarget ).concat( reinterpret_cast<char const *>( Data ), Length ) ? Length : 0;
#else
    try
     {
      FTarget.append( reinterpret_cast<char const *>( Data ), Length );
     }
    catch( ... )
     {
      return 0;
     }
    return Length;
#endif
   }

 private:
  TGNString &                         FTarget;
 };

inline bool GnReserveStringExact( TGNString & Str, size_t Length )
 {
  return TGnStringLengthCounter( Length ).DoReserve( Str );
 }

template <typename TWriter>
void GnAppendGenerated( TGnStringLengthCounter & Str, size_t Count, TWriter const & )
 {
  Str.AddLength( Count );
 }

template <typename TWriter>
void GnAppendGenerated( TGNString & Str, size_t, TWriter Writer )
 {
  Writer( Str );
 }

template <typename TBuilder>
bool GnBuildStringExact( TGNString & Result, TBuilder Builder )
 {
  TGnStringLengthCounter Counter;
  Builder( Counter );

  if( !Counter.DoReserve( Result ) )
    return false;

  Builder( Result );
  return Result.length() == Counter.length();
 }

#endif // GnStringH
