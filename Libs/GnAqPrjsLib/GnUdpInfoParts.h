// GnUdpInfoParts.h, Version: 1.00
#pragma once
#include <GnBasics.h>
#include <cstdio>
#include <map>

using TGnUdpInfoParts = std::map<TGNString,TGNString>;

inline TGNString GnUdpInfoHash( TGnUdpInfoParts const & Parts )
 {
  uint64_t Hash = GnHash64OffsetBasis;
  for( auto const & [Name, Value] : Parts )
   {
    Hash = GnHash64(Name.data(),Name.size(),Hash);
    Hash = GnHash64(":",1,Hash);
    Hash = GnHash64(Value.data(),Value.size(),Hash);
    Hash = GnHash64("\n",1,Hash);
   }
  char Buffer[17];
  std::snprintf(Buffer,sizeof(Buffer),"%016llx",static_cast<unsigned long long>(Hash));
  return Buffer;
 }
