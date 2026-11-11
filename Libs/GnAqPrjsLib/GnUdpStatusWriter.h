// GnUdpStatusWriter.h, Version: 1.01
#pragma once

#include <GnBasics.h>
#include <GnUdpInfoParts.h>

template<bool> struct GnImpl_TUdpStatusData;
using TUdpStatusData = GnImpl_TUdpStatusData<false>;

bool GnBuildCompactStatus( TUdpStatusData const & Status, TGNString const & Id,
                           uint64_t & Signature, TGnUdpInfoParts * Parts,
                           TGNString & Packet );
