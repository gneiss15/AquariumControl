// ShellyMeterPacket.cpp, Version: 1.00
#include "ShellyPowerMeter.h"
#include <cmath>

static uint32_t Read24(const uint8_t * Bytes)
 {
  return uint32_t(Bytes[0]) | (uint32_t(Bytes[1]) << 8) | (uint32_t(Bytes[2]) << 16);
 }
bool ShellyDecodeMeterPacket(const uint8_t * Packet, size_t Size,
  double VoltageScale, double CurrentScale, double PowerScale, TShellyMeterSample & Sample)
 {
  if( !Packet || Size != 23 || Packet[0] != 0x55 ||
      !std::isfinite(VoltageScale) || !std::isfinite(CurrentScale) || !std::isfinite(PowerScale) ||
      VoltageScale <= 0 || CurrentScale <= 0 || PowerScale <= 0 ) return false;
  uint8_t Sum = 0x58;
  for( unsigned I = 0; I < 22; ++I ) Sum += Packet[I];
  if( uint8_t(~Sum) != Packet[22] ) return false;
  const uint32_t Raw = Read24(Packet + 10);
  const int32_t SignedPower = Raw & 0x800000 ? int32_t(Raw) - 0x1000000 : int32_t(Raw);
  TShellyMeterSample Next = { float(Read24(Packet + 4) * VoltageScale),
    float(Read24(Packet + 1) * CurrentScale), float(SignedPower * PowerScale) };
  if( !std::isfinite(Next.Voltage) || !std::isfinite(Next.Current) || !std::isfinite(Next.Power) ) return false;
  Sample = Next;
  return true;
 }
