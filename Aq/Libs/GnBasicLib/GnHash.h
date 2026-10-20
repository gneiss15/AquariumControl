// GnHash.h, Version: 2.37
#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr uint64_t GnHash64OffsetBasis = 14695981039346656037ULL;
constexpr uint64_t GnHash64Prime = 1099511628211ULL;

static inline uint64_t GnHash64( void const * data, size_t size,
                                 uint64_t hash = GnHash64OffsetBasis )
 {
  uint8_t const * bytes = static_cast<uint8_t const *>( data );
  for( size_t index = 0;  index < size;  ++index )
   {
    hash ^= bytes[ index ];
    hash *= GnHash64Prime;
   }
  return hash;
 }
