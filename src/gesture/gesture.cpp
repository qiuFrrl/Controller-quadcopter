#include <Arduino.h>
#include <math.h>
#include "config.h"
#include "gesture.h"
#include "glove_imu.h"

static bool s_active = false;
static uint32_t s_startedMs = 0;
static uint32_t s_lastSampleMs = 0;
static float s_rollSum = 0.0f;
static float s_pitchSum = 0.0f;
static uint16_t s_samples = 0;
static GestureResultCallback s_callback = nullptr;

void gestureBegin() {
  s_active = false;
}

bool gestureStart(GestureResultCallback callback) {
  if (s_active || imuHasError()) {
    return false;
  }
  s_active = true;
  s_startedMs = millis();
  s_lastSampleMs = 0;
  s_rollSum = 0.0f;
  s_pitchSum = 0.0f;
  s_samples = 0;
  s_callback = callback;
  Serial.println("[GESTURE] Mulai sampling gesture.");
  return true;
}

bool gestureIsActive() {
  return s_active;
}

static GestureDecision decide(float roll, float pitch, bool &valid) {
  float finalPitch = pitch;
  float finalRoll = roll;
#if GLOVE_SWAP_AXES
  finalPitch = roll;
  finalRoll = pitch;
#endif
  finalPitch *= static_cast<float>(GLOVE_PITCH_SIGN);
  finalRoll *= static_cast<float>(GLOVE_ROLL_SIGN);

  valid = true;
  if (finalPitch < -20.0f) {
    return GESTURE_KIRI;
  }
  if (finalRoll > 15.0f) {
    return GESTURE_MAJU;
  }
  if (finalPitch >= 30.0f) {
    return GESTURE_KANAN;
  }
  if (finalRoll < -15.0f) {
    return GESTURE_MUNDUR;
  }

  valid = false;
  return GESTURE_NONE;
}

static const char *decisionName(GestureDecision decision) {
  if (decision == GESTURE_MAJU) {
    return "MAJU";
  }
  if (decision == GESTURE_MUNDUR) {
    return "MUNDUR";
  }
  if (decision == GESTURE_KIRI) {
    return "KIRI";
  }
  if (decision == GESTURE_KANAN) {
    return "KANAN";
  }
  return "NONE";
}

void gestureUpdate() {
  if (!s_active) {
    return;
  }

  uint32_t now = millis();
  if (s_lastSampleMs == 0 || (now - s_lastSampleMs) >= 10UL) {
    s_lastSampleMs = now;
    ImuAngles angles;
    if (imuReadAngles(angles)) {
      s_rollSum += angles.roll;
      s_pitchSum += angles.pitch;
      s_samples++;
    }
  }

  if ((now - s_startedMs) < GESTURE_SAMPLE_MS) {
    return;
  }

  float roll = s_samples > 0 ? s_rollSum / static_cast<float>(s_samples) : 0.0f;
  float pitch = s_samples > 0 ? s_pitchSum / static_cast<float>(s_samples) : 0.0f;
  bool valid = false;
  GestureDecision decision = decide(roll, pitch, valid);

  Serial.print("[GESTURE] roll=");
  Serial.print(roll, 2);
  Serial.print(" pitch=");
  Serial.print(pitch, 2);
  Serial.print(" sampel=");
  Serial.print(s_samples);
  Serial.print(" keputusan=");
  Serial.println(valid ? decisionName(decision) : "DITOLAK");

  GestureResultCallback callback = s_callback;
  s_active = false;
  s_callback = nullptr;
  if (callback != nullptr) {
    callback(decision, valid);
  }
}
