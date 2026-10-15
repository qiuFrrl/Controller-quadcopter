#pragma once

#include <Arduino.h>

struct ImuRawSample {
  int16_t ax;
  int16_t ay;
  int16_t az;
  int16_t gx;
  int16_t gy;
  int16_t gz;
};

struct ImuAngles {
  float roll;
  float pitch;
};

struct ImuMotion {
  float roll;
  float pitch;
  float gyroX;
  float gyroY;
  float gyroZ;
};

void imuBegin();
bool imuReady();
bool imuReadRaw(ImuRawSample &sample);
bool imuReadAngles(ImuAngles &angles);
bool imuReadMotion(ImuMotion &motion);
void imuHandleSerial(char command);
void imuPrintDiagnostics();
bool imuHasError();

