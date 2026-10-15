#include <Arduino.h>
#include <math.h>
#include "config.h"
#include "glove_comm.h"
#include "glove_imu.h"
#include "leds.h"

static uint8_t s_rejectToggles = 0;
static uint32_t s_nextRejectMs = 0;
static bool s_rejectOn = false;
static uint8_t s_successToggles = 0;
static uint32_t s_nextSuccessMs = 0;
static bool s_successOn = false;

static bool pinConfigured(int pin) {
  return pin >= 0;
}

static void setLeds(bool green, bool red) {
  if (pinConfigured(LED_GREEN_PIN)) {
    digitalWrite(LED_GREEN_PIN, green ? HIGH : LOW);
  }
  if (pinConfigured(LED_RED_PIN)) {
    digitalWrite(LED_RED_PIN, red ? HIGH : LOW);
  }
}

void ledsBegin() {
  if (pinConfigured(LED_GREEN_PIN)) {
    pinMode(LED_GREEN_PIN, OUTPUT);
  }
  if (pinConfigured(LED_RED_PIN)) {
    pinMode(LED_RED_PIN, OUTPUT);
  }
  if (!pinConfigured(LED_GREEN_PIN) || !pinConfigured(LED_RED_PIN)) {
    Serial.println("[LED] LED_GREEN_PIN/LED_RED_PIN masih -1 di config.h.");
  }
  setLeds(false, true);
}

void ledsBlinkReject(uint8_t count) {
  s_rejectToggles = count * 2;
  s_nextRejectMs = 0;
  s_rejectOn = false;
}

void ledsBlinkSuccess(uint8_t count) {
  s_successToggles = count * 2;
  s_nextSuccessMs = 0;
  s_successOn = false;
}

void ledsUpdate() {
  uint32_t now = millis();

  if (s_rejectToggles > 0) {
    if (s_nextRejectMs == 0 || now >= s_nextRejectMs) {
      s_rejectOn = !s_rejectOn;
      s_rejectToggles--;
      s_nextRejectMs = now + 90UL;
    }
    setLeds(false, s_rejectOn);
    return;
  }

  if (s_successToggles > 0) {
    if (s_nextSuccessMs == 0 || now >= s_nextSuccessMs) {
      s_successOn = !s_successOn;
      s_successToggles--;
      s_nextSuccessMs = now + 120UL;
    }
    setLeds(s_successOn, false);
    return;
  }


  uint32_t statusAge = commStatusAgeMs();
  bool noStatusYet = statusAge == 0xFFFFFFFFUL;
  bool linkLost = !noStatusYet && !commHasFreshStatus();
  bool imuError = imuHasError();
  uint8_t state = commLastDroneState();

  if (imuError || state == DRONE_EMERGENCY) {
    bool redBlink = ((now / 150UL) % 2UL) == 0UL;
    setLeds(false, redBlink);
    return;
  }

  // Jika belum terhubung atau sinyal terputus: LED Merah
  if (noStatusYet) {
    setLeds(false, true); // Merah solid: belum ada sinyal ESP-NOW
    return;
  }

  if (linkLost) {
    bool redBlink = ((now / 200UL) % 2UL) == 0UL;
    setLeds(false, redBlink); // Merah kedip: sinyal terputus
    return;
  }

  // ESP-NOW BERHASIL TERHUBUNG: Lampu HIJAU HIDUP!
  if (state == DRONE_LANDING) {
    bool greenBlink = ((now / 500UL) % 2UL) == 0UL;
    setLeds(greenBlink, false);
  } else {
    // DISARMED, ARMED, atau FLYING saat link terhubung: Lampu hijau hidup solid
    setLeds(true, false);
  }
}
