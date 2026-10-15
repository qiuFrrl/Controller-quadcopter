#include <Arduino.h>
#include <math.h>
#include "config.h"
#include "glove_comm.h"
#include "glove_imu.h"
#include "leds.h"
#include "packets.h"
#include "test_control.h"

static bool s_isReading = false;
static uint32_t s_readStartMs = 0;
static uint32_t s_lastSampleMs = 0;
static uint32_t s_lastLivePrintMs = 0;
static uint16_t s_sampleCount = 0;

static float s_sumPitch = 0.0f;
static float s_sumRoll = 0.0f;
static float s_sumGyroX = 0.0f;
static float s_sumGyroY = 0.0f;
static float s_sumGyroZ = 0.0f;

static float s_minPitch = 999.0f;
static float s_maxPitch = -999.0f;
static float s_minRoll = 999.0f;
static float s_maxRoll = -999.0f;
static float s_minGyroX = 999.0f;
static float s_maxGyroX = -999.0f;
static float s_minGyroY = 999.0f;
static float s_maxGyroY = -999.0f;

void testControlBegin() {
  s_isReading = false;
  Serial.println("[TEST CONTROL] Mode uji tombol READY aktif.");
  Serial.println("  -> Tekan tombol READY (atau spasi di Serial) untuk MULAI membaca data.");
  Serial.println("  -> Tekan tombol READY lagi untuk SELESAI & translate (MAJU/MUNDUR/KIRI/KANAN).");
}

bool testControlIsReading() {
  return s_isReading;
}

static void startReading() {
  s_isReading = true;
  s_readStartMs = millis();
  s_lastSampleMs = 0;
  s_lastLivePrintMs = 0;
  s_sampleCount = 0;

  s_sumPitch = 0.0f;
  s_sumRoll = 0.0f;
  s_sumGyroX = 0.0f;
  s_sumGyroY = 0.0f;
  s_sumGyroZ = 0.0f;

  s_minPitch = 999.0f;
  s_maxPitch = -999.0f;
  s_minRoll = 999.0f;
  s_maxRoll = -999.0f;
  s_minGyroX = 999.0f;
  s_maxGyroX = -999.0f;
  s_minGyroY = 999.0f;
  s_maxGyroY = -999.0f;

  Serial.println();
  Serial.println("============================================================");
  Serial.println("[READY] >>> MULAI MEMBACA DATA GERAKAN <<<");
  Serial.println("Gerakkan tangan ke arah yang diinginkan.");
  Serial.println("Tekan tombol READY lagi untuk mengakhiri pembacaan...");
  Serial.println("============================================================");

  commStartCommand(CMD_BTN_READY, 6);
  Serial.println("  >> [ESP-NOW] Mengirim command: BTN_READY (12)");
}

static void stopReadingAndTranslate() {
  s_isReading = false;
  uint32_t durationMs = millis() - s_readStartMs;

  if (s_sampleCount == 0) {
    Serial.println("[READY] Selesai: Tidak ada data sampel IMU terbaca.");
    ledsBlinkReject(2);
    return;
  }

  float finalPitch = s_sumPitch / static_cast<float>(s_sampleCount);
  float finalRoll = s_sumRoll / static_cast<float>(s_sampleCount);
  float avgGyroX = s_sumGyroX / static_cast<float>(s_sampleCount);
  float avgGyroY = s_sumGyroY / static_cast<float>(s_sampleCount);

  float absPitch = fabs(finalPitch);
  float absRoll = fabs(finalRoll);

  const char *arah = "NETRAL / DIAM";
  bool isAction = false;
  uint8_t cmdToSend = CMD_NONE;

  if (finalPitch < -20.0f) {
    arah = "KIRI";
    cmdToSend = CMD_KIRI;
    isAction = true;
  } else if (finalRoll > 15.0f) {
    arah = "MAJU";
    cmdToSend = CMD_MAJU;
    isAction = true;
  } else if (finalPitch >= 30.0f) {
    arah = "KANAN";
    cmdToSend = CMD_KANAN;
    isAction = true;
  } else if (finalRoll < -15.0f) {
    arah = "MUNDUR";
    cmdToSend = CMD_MUNDUR;
    isAction = true;
  }

  // Kirim seketika ke Quadcopter sebelum buffer Serial terisi
  if (isAction) {
    commStartCommand(cmdToSend, 4);
    ledsBlinkSuccess(2);
  } else {
    ledsBlinkReject(1);
  }

  Serial.println();
  Serial.println("============================================================");
  Serial.print("[READY] >>> SELESAI MEMBACA! (");
  Serial.print(s_sampleCount);
  Serial.print(" sampel | ");
  Serial.print(static_cast<float>(durationMs) / 1000.0f, 2);
  Serial.println(" detik) <<<");
  Serial.println("------------------------------------------------------------");
  Serial.println("Rentang Output Terukur:");
  Serial.print("  - Sudut Pitch : avg = ");
  if (finalPitch >= 0) Serial.print("+");
  Serial.print(finalPitch, 2);
  Serial.print("°  [Rentang: ");
  Serial.print(s_minPitch, 1);
  Serial.print("° s/d ");
  Serial.print(s_maxPitch, 1);
  Serial.println("°]");

  Serial.print("  - Sudut Roll  : avg = ");
  if (finalRoll >= 0) Serial.print("+");
  Serial.print(finalRoll, 2);
  Serial.print("°  [Rentang: ");
  Serial.print(s_minRoll, 1);
  Serial.print("° s/d ");
  Serial.print(s_maxRoll, 1);
  Serial.println("°]");

  Serial.print("  - Gyro Rate X (Roll Rate)   : avg = ");
  Serial.print(avgGyroX, 1);
  Serial.print(" °/s  [Rentang: ");
  Serial.print(s_minGyroX, 1);
  Serial.print(" s/d ");
  Serial.print(s_maxGyroX, 1);
  Serial.println("]");

  Serial.print("  - Gyro Rate Y (Pitch Rate)  : avg = ");
  Serial.print(avgGyroY, 1);
  Serial.print(" °/s  [Rentang: ");
  Serial.print(s_minGyroY, 1);
  Serial.print(" s/d ");
  Serial.print(s_maxGyroY, 1);
  Serial.println("]");

  Serial.println("------------------------------------------------------------");
  Serial.print("  >> HASIL TRANSLASI: >>> [ ");
  Serial.print(arah);
  Serial.println(" ] <<<");

  if (isAction) {
    Serial.print("  >> [ESP-NOW TERKIRIM INSTAN]: ");
    Serial.print(arah);
    Serial.print(" (Command ID: ");
    Serial.print(cmdToSend);
    Serial.println(")");
  }
  Serial.println("============================================================");
  Serial.println();
}

void testControlTriggerReady() {
  if (!s_isReading) {
    startReading();
  } else {
    stopReadingAndTranslate();
  }
}

bool testControlHandleSerial(char command) {
  if (command == ' ') {
    testControlTriggerReady();
    return true;
  }
  return false;
}

void testControlUpdate() {
  if (!s_isReading) {
    return;
  }

  uint32_t now = millis();
  if (s_lastSampleMs == 0 || (now - s_lastSampleMs) >= 15UL) {
    s_lastSampleMs = now;
    ImuMotion motion;
    if (imuReadMotion(motion)) {
      float p = motion.pitch;
      float r = motion.roll;
      float gx = motion.gyroX;
      float gy = motion.gyroY;
#if GLOVE_SWAP_AXES
      p = motion.roll;
      r = motion.pitch;
      gx = motion.gyroY;
      gy = motion.gyroX;
#endif
      p *= static_cast<float>(GLOVE_PITCH_SIGN);
      r *= static_cast<float>(GLOVE_ROLL_SIGN);
      gx *= static_cast<float>(GLOVE_ROLL_SIGN);
      gy *= static_cast<float>(GLOVE_PITCH_SIGN);

      s_sumPitch += p;
      s_sumRoll += r;
      s_sumGyroX += gx;
      s_sumGyroY += gy;
      s_sumGyroZ += motion.gyroZ;

      if (p < s_minPitch) s_minPitch = p;
      if (p > s_maxPitch) s_maxPitch = p;
      if (r < s_minRoll) s_minRoll = r;
      if (r > s_maxRoll) s_maxRoll = r;
      if (gx < s_minGyroX) s_minGyroX = gx;
      if (gx > s_maxGyroX) s_maxGyroX = gx;
      if (gy < s_minGyroY) s_minGyroY = gy;
      if (gy > s_maxGyroY) s_maxGyroY = gy;

      s_sampleCount++;
    }
  }

  // Live monitor setiap 300 ms saat membaca
  if (s_lastLivePrintMs == 0 || (now - s_lastLivePrintMs) >= 300UL) {
    s_lastLivePrintMs = now;
    ImuAngles a;
    if (imuReadAngles(a)) {
      float p = a.pitch;
      float r = a.roll;
#if GLOVE_SWAP_AXES
      p = a.roll;
      r = a.pitch;
#endif
      p *= static_cast<float>(GLOVE_PITCH_SIGN);
      r *= static_cast<float>(GLOVE_ROLL_SIGN);
      Serial.print("  [Sedang Membaca...] Pitch: ");
      if (p >= 0) Serial.print("+");
      Serial.print(p, 1);
      Serial.print("°, Roll: ");
      if (r >= 0) Serial.print("+");
      Serial.print(r, 1);
      Serial.println("°");
    }
  }
}
