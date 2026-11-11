// TmrSwCompactStatus.cpp, Version: 1.11
// Status data is written directly by the shared contract-based UDP writer.

#define Do_TUdpStatusData
#include <GnBasics.h>
#include <GnJsonSchema.h>
#include <GnDeviceIdentity.h>
#include "GnUdpStatusWriter.h"
#include "TmrSwStatusApi.h"

bool TTmrSwStatusApi::CompactStatus( uint64_t & Signature, TGnUdpInfoParts * Parts,
                                    TGNString & Packet ) const
 {
  TUdpStatusData Status;
  FillStatus( Status );
  return GnBuildCompactStatus( Status, GnDeviceIdentity.WireId(), Signature, Parts, Packet );
 }
