#include <Arduino.h>
#include <math.h>
#include <Wire.h>
#include <Preferences.h>
#include "config.h"
#include "glove_imu.h"

static const uint8_t MPU_ADDR_PRIMARY = 0x68;
static const uint8_t MPU_ADDR_FALLBACK = 0x69;
static const uint8_t REG_PWR_MGMT_1 = 0x6B;
static const uint8_t REG_CONFIG = 0x1A;
static const uint8_t REG_GYRO_CONFIG = 0x1B;
static const uint8_t REG_ACCEL_CONFIG = 0x1C;
static const uint8_t REG_ACCEL_XOUT_H = 0x3B;
static const float ACCEL_LSB_PER_G = 4096.0f;

static Preferences s_prefs;
static uint8_t s_addr = MPU_ADDR_PRIMARY;
static bool s_ready = false;
static bool s_error = true;
static uint8_t s_failCount = 0;
static float s_accelOffsetX = 0.0f;
static float s_accelOffsetY = 0.0f;
static float s_accelOffsetZ = 0.0f;
static float s_gyroOffsetX = 0.0f;
static float s_gyroOffsetY = 0.0f;
static float s_gyroOffsetZ = 0.0f;
static float s_neutralRoll = 0.0f;
static float s_neutralPitch = 0.0f;
static bool s_printLive = false;
static uint32_t s_printLiveUntil = 0;
static uint32_t s_lastLivePrint = 0;

enum CalibrationMode : uint8_t {
  CAL_NONE = 0,
  CAL_FLAT = 1,
  CAL_NEUTRAL = 2
};

static CalibrationMode s_calMode = CAL_NONE;
static uint16_t s_calTarget = 0;
static uint16_t s_calValid = 0;
static uint32_t s_lastCalSampleMs = 0;
static int64_t s_calAx = 0;
static int64_t s_calAy = 0;
static int64_t s_calAz = 0;
static int64_t s_calGx = 0;
static int64_t s_calGy = 0;
static int64_t s_calGz = 0;

static bool pinConfigured(int pin) {
  return pin >= 0;
}

static bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(s_addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool probeAddress(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static void loadCalibration() {
  s_prefs.begin("glove", false);
  s_accelOffsetX = s_prefs.getFloat("aox", 0.0f);
  s_accelOffsetY = s_prefs.getFloat("aoy", 0.0f);
  s_accelOffsetZ = s_prefs.getFloat("aoz", 0.0f);
  s_gyroOffsetX = s_prefs.getFloat("gox", 0.0f);
  s_gyroOffsetY = s_prefs.getFloat("goy", 0.0f);
  s_gyroOffsetZ = s_prefs.getFloat("goz", 0.0f);
  s_neutralRoll = s_prefs.getFloat("nroll", 0.0f);
  s_neutralPitch = s_prefs.getFloat("npitch", 0.0f);
}

static void saveCalibration() {
  s_prefs.putFloat("aox", s_accelOffsetX);
  s_prefs.putFloat("aoy", s_accelOffsetY);
  s_prefs.putFloat("aoz", s_accelOffsetZ);
  s_prefs.putFloat("gox", s_gyroOffsetX);
  s_prefs.putFloat("goy", s_gyroOffsetY);
  s_prefs.putFloat("goz", s_gyroOffsetZ);
}

static void saveNeutral() {
  s_prefs.putFloat("nroll", s_neutralRoll);
  s_prefs.putFloat("npitch", s_neutralPitch);
}

static void markReadResult(bool ok) {
  if (ok) {
    s_failCount = 0;
    s_error = false;
    return;
  }
  if (s_failCount < 255) {
    s_failCount++;
  }
  if (s_failCount >= 5) {
    s_error = true;
  }
}

static ImuAngles rawToAngles(const ImuRawSample &sample, bool subtractNeutral) {
  float ax = (static_cast<float>(sample.ax) / ACCEL_LSB_PER_G) - s_accelOffsetX;
  float ay = (static_cast<float>(sample.ay) / ACCEL_LSB_PER_G) - s_accelOffsetY;
  float az = (static_cast<float>(sample.az) / ACCEL_LSB_PER_G) - s_accelOffsetZ;

  float roll = atan(ay / sqrt((ax * ax) + (az * az))) * 180.0f / static_cast<float>(M_PI);
  float pitch = -atan(ax / sqrt((ay * ay) + (az * az))) * 180.0f / static_cast<float>(M_PI);

  if (subtractNeutral) {
    roll -= s_neutralRoll;
    pitch -= s_neutralPitch;
  }

  ImuAngles angles = {roll, pitch};
  return angles;
}

void imuBegin() {
  if (!pinConfigured(SDA_PIN) || !pinConfigured(SCL_PIN)) {
    Serial.println("[IMU] SDA_PIN/SCL_PIN belum diisi di config.h.");
    s_ready = false;
    s_error = true;
    return;
  }

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setTimeOut(5);
  loadCalibration();

  if (probeAddress(MPU_ADDR_PRIMARY)) {
    s_addr = MPU_ADDR_PRIMARY;
  } else if (probeAddress(MPU_ADDR_FALLBACK)) {
    s_addr = MPU_ADDR_FALLBACK;
  } else {
    Serial.println("[IMU] MPU6050 tidak ditemukan di 0x68 atau 0x69.");
    s_ready = false;
    s_error = true;
    return;
  }

  bool ok = writeRegister(REG_PWR_MGMT_1, 0x00);
  ok = ok && writeRegister(REG_CONFIG, 0x03);
  ok = ok && writeRegister(REG_GYRO_CONFIG, 0x08);
  ok = ok && writeRegister(REG_ACCEL_CONFIG, 0x10);
  s_ready = ok;
  s_error = !ok;

  Serial.print("[IMU] alamat 0x");
  Serial.print(s_addr, HEX);
  Serial.println(ok ? " siap." : " gagal konfigurasi register.");
}

bool imuReady() {
  return s_ready && !s_error;
}

bool imuHasError() {
  return s_error;
}

bool imuReadRaw(ImuRawSample &sample) {
  if (!s_ready) {
    markReadResult(false);
    return false;
  }

  Wire.beginTransmission(s_addr);
  Wire.write(REG_ACCEL_XOUT_H);
  if (Wire.endTransmission(false) != 0) {
    markReadResult(false);
    return false;
  }

  uint8_t count = Wire.requestFrom(static_cast<int>(s_addr), 14);
  if (count != 14) {
    markReadResult(false);
    return false;
  }

  sample.ax = static_cast<int16_t>((Wire.read() << 8) | Wire.read());
  sample.ay = static_cast<int16_t>((Wire.read() << 8) | Wire.read());
  sample.az = static_cast<int16_t>((Wire.read() << 8) | Wire.read());
  Wire.read();
  Wire.read();
  sample.gx = static_cast<int16_t>((Wire.read() << 8) | Wire.read());
  sample.gy = static_cast<int16_t>((Wire.read() << 8) | Wire.read());
  sample.gz = static_cast<int16_t>((Wire.read() << 8) | Wire.read());

  markReadResult(true);
  return true;
}

bool imuReadAngles(ImuAngles &angles) {
  ImuRawSample sample;
  if (!imuReadRaw(sample)) {
    return false;
  }
  angles = rawToAngles(sample, true);
  return true;
}

static void startCalibration(CalibrationMode mode) {
  if (s_calMode != CAL_NONE) {
    Serial.println("[IMU] Kalibrasi masih berjalan, tunggu sampai selesai.");
    return;
  }
  if (!imuReady()) {
    Serial.println("[IMU] Kalibrasi ditolak: IMU belum siap.");
    return;
  }

  s_calMode = mode;
  s_calTarget = mode == CAL_FLAT ? 1000 : 500;
  s_calValid = 0;
  s_lastCalSampleMs = 0;
  s_calAx = 0;
  s_calAy = 0;
  s_calAz = 0;
  s_calGx = 0;
  s_calGy = 0;
  s_calGz = 0;

  if (mode == CAL_FLAT) {
    Serial.println("[IMU] Kalibrasi 'c': diamkan glove datar, sampling non-blocking 1000 sampel...");
  } else {
    Serial.println("[IMU] Kalibrasi 'n': tahan tangan pada posisi netral, sampling non-blocking 500 sampel...");
  }
}

static void finishCalibration() {
  if (s_calValid == 0) {
    Serial.println("[IMU] Kalibrasi gagal: tidak ada sampel valid.");
    s_calMode = CAL_NONE;
    return;
  }

  ImuRawSample avg;
  avg.ax = static_cast<int16_t>(s_calAx / s_calValid);
  avg.ay = static_cast<int16_t>(s_calAy / s_calValid);
  avg.az = static_cast<int16_t>(s_calAz / s_calValid);
  avg.gx = static_cast<int16_t>(s_calGx / s_calValid);
  avg.gy = static_cast<int16_t>(s_calGy / s_calValid);
  avg.gz = static_cast<int16_t>(s_calGz / s_calValid);

  if (s_calMode == CAL_FLAT) {
    s_accelOffsetX = static_cast<float>(avg.ax) / ACCEL_LSB_PER_G;
    s_accelOffsetY = static_cast<float>(avg.ay) / ACCEL_LSB_PER_G;
    s_accelOffsetZ = (static_cast<float>(avg.az) / ACCEL_LSB_PER_G) - 1.0f;
    s_gyroOffsetX = static_cast<float>(avg.gx) / 65.5f;
    s_gyroOffsetY = static_cast<float>(avg.gy) / 65.5f;
    s_gyroOffsetZ = static_cast<float>(avg.gz) / 65.5f;
    saveCalibration();
    Serial.println("[IMU] Kalibrasi datar tersimpan ke NVS.");
  } else if (s_calMode == CAL_NEUTRAL) {
    ImuAngles angles = rawToAngles(avg, false);
    s_neutralRoll = angles.roll;
    s_neutralPitch = angles.pitch;
    saveNeutral();
    Serial.println("[IMU] Sudut netral tersimpan ke NVS.");
  }

  s_calMode = CAL_NONE;
  imuPrintDiagnostics();
}

static void updateCalibration() {
  if (s_calMode == CAL_NONE) {
    return;
  }

  uint32_t now = millis();
  if (s_lastCalSampleMs != 0 && (now - s_lastCalSampleMs) < 2UL) {
    return;
  }
  s_lastCalSampleMs = now;

  ImuRawSample sample;
  if (imuReadRaw(sample)) {
    s_calAx += sample.ax;
    s_calAy += sample.ay;
    s_calAz += sample.az;
    s_calGx += sample.gx;
    s_calGy += sample.gy;
    s_calGz += sample.gz;
    s_calValid++;
  }

  if (s_calValid >= s_calTarget) {
    finishCalibration();
  }
}

void imuHandleSerial(char command) {
  if (command == 'c') {
    startCalibration(CAL_FLAT);
  } else if (command == 'n') {
    startCalibration(CAL_NEUTRAL);
  } else if (command == 'p') {
    s_printLive = true;
    s_printLiveUntil = millis() + 5000UL;
    s_lastLivePrint = 0;
    Serial.println("[IMU] Cetak sudut live selama 5 detik.");
  } else if (command == 'd') {
    imuPrintDiagnostics();
  }

  updateCalibration();

  if (s_printLive && millis() >= s_printLiveUntil) {
    s_printLive = false;
  }
  if (s_printLive && (millis() - s_lastLivePrint) >= 100UL) {
    s_lastLivePrint = millis();
    ImuAngles angles;
    if (imuReadAngles(angles)) {
      Serial.print("[IMU] roll=");
      Serial.print(angles.roll, 2);
      Serial.print(" pitch=");
      Serial.println(angles.pitch, 2);
    }
  }
}

void imuPrintDiagnostics() {
  Serial.print("[IMU] ready=");
  Serial.print(s_ready ? "ya" : "tidak");
  Serial.print(" error=");
  Serial.print(s_error ? "ya" : "tidak");
  Serial.print(" cal=");
  Serial.print(static_cast<uint8_t>(s_calMode));
  Serial.print("/");
  Serial.print(s_calValid);
  Serial.print(" addr=0x");
  Serial.print(s_addr, HEX);
  Serial.print(" accelOffset=");
  Serial.print(s_accelOffsetX, 4);
  Serial.print(",");
  Serial.print(s_accelOffsetY, 4);
  Serial.print(",");
  Serial.print(s_accelOffsetZ, 4);
  Serial.print(" gyroOffset=");
  Serial.print(s_gyroOffsetX, 4);
  Serial.print(",");
  Serial.print(s_gyroOffsetY, 4);
  Serial.print(",");
  Serial.print(s_gyroOffsetZ, 4);
  Serial.print(" neutral=");
  Serial.print(s_neutralRoll, 2);
  Serial.print(",");
  Serial.println(s_neutralPitch, 2);
}
