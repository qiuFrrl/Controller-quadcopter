#include <Arduino.h>
#include <math.h>
#include <Wire.h>
#include "buttons.h"
#include "config.h"
#include "gesture.h"
#include "glove_comm.h"
#include "glove_imu.h"
#include "leds.h"
#include "packets.h"
#include "test_control.h"

static uint32_t s_lastTickMs = 0;
static bool s_pendingGestureDirection = false;
static uint8_t s_gestureDirection = CMD_NONE;
static bool s_pendingReadyBeforeDirection = false;

static void onGestureResult(GestureDecision decision, bool valid) {
  if (!valid || decision == GESTURE_NONE) {
    ledsBlinkReject(2);
    Serial.println("[GESTURE] Gerakan netral / tidak valid.");
    return;
  }

  s_gestureDirection = static_cast<uint8_t>(decision);
  s_pendingReadyBeforeDirection = false; // Langsung kirim perintah arah ke drone!
  s_pendingGestureDirection = true;
  ledsBlinkSuccess(2);
  Serial.printf("[GESTURE] Terdeteksi arah ID=%d. Dikirim ke Drone!\n", s_gestureDirection);
}

static void handleSerial() {
  while (Serial.available() > 0) {
    char command = static_cast<char>(Serial.read());
    if (command == '\r' || command == '\n' || command == ' ') {
      testControlTriggerReady();
      continue;
    }
    if (testControlHandleSerial(command)) {
      continue;
    }
    if (command == 'd') {
      commPrintDiagnostics();
    }
    imuHandleSerial(command);
  }
  imuHandleSerial(0);
}

static void handleReadyButton() {
  if (testControlIsReading()) {
    testControlTriggerReady();
    return;
  }

  uint8_t state = commLastDroneState();
  if (commHasFreshStatus() && state != DRONE_ARMED && state != DRONE_FLYING) {
    ledsBlinkReject(3);
    Serial.println("[GESTURE] Ditolak: Drone belum ARMED!");
    return;
  }

  if (imuHasError()) {
    ledsBlinkReject(3);
    Serial.println("[GESTURE] Ditolak: IMU Glove error!");
    return;
  }

  if (!gestureStart(onGestureResult)) {
    ledsBlinkReject(2);
  }
}

static void flushPendingGestureCommand() {
  if (!s_pendingGestureDirection || commIsBusy()) {
    return;
  }

  if (s_pendingReadyBeforeDirection) {
    if (commStartCommand(CMD_BTN_READY, 4)) {
      s_pendingReadyBeforeDirection = false;
    }
    return;
  }

  if (commStartCommand(s_gestureDirection, 6)) {
    s_pendingGestureDirection = false;
    s_gestureDirection = CMD_NONE;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  uint32_t startWait = millis();
  while (!Serial && (millis() - startWait) < 2000UL) {
    yield();
  }
  delay(200);

  Serial.println();
  Serial.println("============================================================");
  Serial.println("       [BOOT] GloveController ESP32-C3 SuperMini");
  Serial.println("============================================================");
  buttonsBegin();
  ledsBegin();
  Wire.begin(8, 9);
  imuBegin();
  gestureBegin();
  testControlBegin();
  commBegin();
  Serial.println("============================================================");
  Serial.println("Sistem Siap! Tekan tombol READY atau tekan ENTER di Serial");
  Serial.println("untuk mulai membaca data gerakan.");
  Serial.println("============================================================");
  s_lastTickMs = millis();
}

void loop() {
  handleSerial();
  gestureUpdate();
  testControlUpdate();
  flushPendingGestureCommand();

  uint32_t now = millis();
  if ((now - s_lastTickMs) < CONTROL_TICK_MS) {
    return;
  }
  s_lastTickMs += CONTROL_TICK_MS;

  buttonsUpdate();

  if (buttonPressedEdge(BUTTON_EMERGENCY)) {
    commEmergencyCommand();
  }
  if (buttonPressedEdge(BUTTON_ARM)) {
    commStartCommand(CMD_BTN_ARM_LAND, 6);
  }
  if (buttonPressedEdge(BUTTON_READY)) {
    handleReadyButton();
  }

  ledsUpdate();
  commTick();

  static uint32_t s_lastStandbyPrint = 0;
  if (!testControlIsReading() && (now - s_lastStandbyPrint) >= 3000UL) {
    s_lastStandbyPrint = now;
    Serial.println("[STANDBY] Glove aktif. Tekan tombol READY (atau tekan ENTER di Serial) untuk uji gesture.");
  }
}
