// ShellyPowerMeter.h, Version: 1.01
#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>
#include <esp_partition.h>

struct TShellyMeterSample
 {
  float Voltage, Current, Power;
 };
bool ShellyDecodeMeterPacket(const uint8_t * Packet, size_t Size,
  double VoltageScale, double CurrentScale, double PowerScale, TShellyMeterSample & Sample);

class TShellyPowerMeter
 {
 private:
  HardwareSerial FPort{1};
  uint8_t FPacket[23] = {};
  unsigned FReceived = 0;
  uint32_t FLastRequest = 0, FLastByte = 0, FLastValid = 0;
  bool FReady = false, FSampleReceived = false;
  bool FCalibrationWriteUncertain = false;
  double FVoltageScale = 0, FCurrentScale = 0, FPowerScale = 0;
  TShellyMeterSample FSample = {};
  void WriteRegister(uint8_t Register, uint32_t Value);
  const esp_partition_t * CalibrationPartition(void) const;
 public:
  bool Setup(void);
  void Loop(void);
  bool Valid(void) const;
  float Voltage(void) const;
  float Current(void) const;
  float Power(void) const;
  double VoltageCalibration(void) const { return FVoltageScale / 1e-11; }
  double CurrentCalibration(void) const { return FCurrentScale / 1e-11; }
  double PowerCalibration(void) const { return FPowerScale / 1e-11; }
  bool CalibrationWritable(void) const;
  bool CalibrationWriteUncertain(void) const { return FCalibrationWriteUncertain; }
  bool SetCalibration(double Voltage, double Current, double Power);
 };
extern TShellyPowerMeter ShellyPowerMeter;
