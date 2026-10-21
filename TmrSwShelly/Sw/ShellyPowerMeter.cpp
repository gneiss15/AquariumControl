// ShellyPowerMeter.cpp, Version: 1.02
#include <GnBasics.h>
#include "ShellyPowerMeter.h"
#include <ArduinoJson.h>
#include "esp_partition.h"
#include <cmath>
#include <cstring>

TShellyPowerMeter ShellyPowerMeter;
const esp_partition_t * TShellyPowerMeter::CalibrationPartition(void) const
 {
  const esp_partition_t * Factory = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
    static_cast<esp_partition_subtype_t>(0x88), "shelly");
  return Factory && Factory->address == 0x7F0000 && Factory->size == 0x10000 ? Factory : nullptr;
 }
bool TShellyPowerMeter::CalibrationWritable(void) const
 {
  const esp_partition_t * Factory = CalibrationPartition();
  return Factory && !Factory->readonly && !FCalibrationWriteUncertain;
 }

bool TShellyPowerMeter::Setup(void)
 {
  FReady = FSampleReceived = false;
  FReceived = 0;
  const esp_partition_t * Factory = CalibrationPartition();
  if( !Factory ) return false;
  // Read the exact device factory record; never substitute another device's calibration.
  char Record[4097] = {};
  if( esp_partition_read(Factory, 0x1000, Record, 0x1000) != ESP_OK ) return false;
  JsonDocument Document;
  if( deserializeJson(Document, Record) ) return false;
  JsonVariant Calibration = Document["factory"]["calib"];
  JsonVariant Scales = Calibration["scales0"];
  if( Calibration["done"] != true || !Scales["current_gain"].is<int>() || Scales["current_gain"].as<int>() != 0 ||
      !Scales["voltage_scale"].is<double>() || !Scales["current_scale"].is<double>() || !Scales["apower_scale"].is<double>() ) return false;
  // Stock shos_bl0942.c converts stored scale coefficients with 1e-11.
  FVoltageScale = Scales["voltage_scale"].as<double>() * 1e-11;
  FCurrentScale = Scales["current_scale"].as<double>() * 1e-11;
  FPowerScale = Scales["apower_scale"].as<double>() * 1e-11;
  if( !std::isfinite(FVoltageScale) || !std::isfinite(FCurrentScale) || !std::isfinite(FPowerScale) ||
      FVoltageScale <= 0 || FCurrentScale <= 0 || FPowerScale <= 0 ) return false;
  // Stock PlugMG3: UART1, RX GPIO5, TX GPIO4, 9600 baud, 8N1, chip address zero.
  FPort.begin(9600, SERIAL_8N1, 5, 4);
  WriteRegister(0x1D, 0x55);
  WriteRegister(0x1C, 0x5A5A5A);
  WriteRegister(0x1D, 0x55);
  WriteRegister(0x19, 0x8F);
  WriteRegister(0x14, 0x33);
  WriteRegister(0x1D, 0);
  FLastRequest = millis() - 1000;
  FReady = true;
  return true;
 }

bool TShellyPowerMeter::SetCalibration(double Voltage, double Current, double Power)
 {
  if( !std::isfinite(Voltage) || !std::isfinite(Current) || !std::isfinite(Power) ||
      Voltage < 1 || Current < 1 || Power < 1 || Voltage > 1e10 || Current > 1e10 || Power > 1e10 ) return false;
  const esp_partition_t * Factory = CalibrationPartition();
  if( !Factory || Factory->readonly || FCalibrationWriteUncertain ) return false;

  static constexpr uint32_t RecordOffset = 0x1000;
  static constexpr size_t SectorSize = 0x1000;
  uint8_t Original[SectorSize], Candidate[SectorSize], Readback[SectorSize];
  if( esp_partition_read(Factory, RecordOffset, Original, sizeof(Original)) != ESP_OK ) return false;
  size_t RecordSize = 0;
  while( RecordSize < sizeof(Original) && Original[RecordSize] != 0xFF ) ++RecordSize;
  if( RecordSize == 0 || RecordSize == sizeof(Original) ) return false;
  for( size_t I = RecordSize; I < sizeof(Original); ++I )
    if( Original[I] != 0xFF ) return false;

  JsonDocument Document;
  if( deserializeJson(Document, reinterpret_cast<const char *>(Original), RecordSize) || Document.overflowed() ) return false;
  JsonVariant Calibration = Document["factory"]["calib"];
  JsonVariant Scales = Calibration["scales0"];
  if( Calibration["done"] != true || !Scales["current_gain"].is<int>() || Scales["current_gain"].as<int>() != 0 ||
      !Scales["voltage_scale"].is<double>() || !Scales["current_scale"].is<double>() ||
      !Scales["apower_scale"].is<double>() ) return false;

  Scales["voltage_scale"] = Voltage;
  Scales["current_scale"] = Current;
  Scales["apower_scale"] = Power;
  char Serialized[SectorSize];
  const size_t SerializedSize = serializeJson(Document, Serialized, sizeof(Serialized));
  if( SerializedSize == 0 || SerializedSize >= sizeof(Serialized) ) return false;
  memset(Candidate, 0xFF, sizeof(Candidate));
  memcpy(Candidate, Serialized, SerializedSize);

  const bool Erased = esp_partition_erase_range(Factory, RecordOffset, SectorSize) == ESP_OK;
  const bool Written = Erased && esp_partition_write(Factory, RecordOffset, Candidate, sizeof(Candidate)) == ESP_OK;
  const bool Verified = Written && esp_partition_read(Factory, RecordOffset, Readback, sizeof(Readback)) == ESP_OK &&
    memcmp(Readback, Candidate, sizeof(Candidate)) == 0;
  if( !Verified )
   {
    const bool RestoredErase = esp_partition_erase_range(Factory, RecordOffset, SectorSize) == ESP_OK;
    const bool RestoredWrite = RestoredErase && esp_partition_write(Factory, RecordOffset, Original, sizeof(Original)) == ESP_OK;
    const bool RestoredRead = RestoredWrite && esp_partition_read(Factory, RecordOffset, Readback, sizeof(Readback)) == ESP_OK &&
      memcmp(Readback, Original, sizeof(Original)) == 0;
    FCalibrationWriteUncertain = !RestoredRead;
    return false;
   }

  FVoltageScale = Voltage * 1e-11;
  FCurrentScale = Current * 1e-11;
  FPowerScale = Power * 1e-11;
  return true;
 }
void TShellyPowerMeter::WriteRegister(uint8_t Register, uint32_t Value)
 {
  uint8_t Packet[6] = { 0xA8, Register, uint8_t(Value), uint8_t(Value >> 8), uint8_t(Value >> 16), 0 };
  uint8_t Sum = 0;
  for( unsigned I = 0; I < 5; ++I ) Sum += Packet[I];
  Packet[5] = uint8_t(~Sum);
  FPort.write(Packet, sizeof(Packet));
  FPort.flush();
  delay(2);
 }
void TShellyPowerMeter::Loop(void)
 {
  if( !FReady ) return;
  const uint32_t Now = millis();
  if( FReceived && uint32_t(Now - FLastByte) > 200 ) FReceived = 0;
  unsigned Budget = 128;
  while( Budget-- && FPort.available() )
   {
    const int Byte = FPort.read();
    if( Byte < 0 ) break;
    if( !FReceived && Byte != 0x55 ) continue;
    FLastByte = Now;
    FPacket[FReceived++] = uint8_t(Byte);
    if( FReceived == sizeof(FPacket) )
     {
      if( ShellyDecodeMeterPacket(FPacket, sizeof(FPacket), FVoltageScale, FCurrentScale, FPowerScale, FSample) )
       { FLastValid = Now; FSampleReceived = true; }
      FReceived = 0;
     }
   }
  if( uint32_t(Now - FLastRequest) >= 1000 )
   {
    const uint8_t Request[] = { 0x58, 0xAA };
    FPort.write(Request, sizeof(Request));
    FLastRequest = Now;
   }
 }
bool TShellyPowerMeter::Valid(void) const { return FReady && FSampleReceived && uint32_t(millis() - FLastValid) < 5000; }
float TShellyPowerMeter::Voltage(void) const { return Valid() ? FSample.Voltage : NAN; }
float TShellyPowerMeter::Current(void) const { return Valid() ? FSample.Current : NAN; }
float TShellyPowerMeter::Power(void) const { return Valid() ? FSample.Power : NAN; }
