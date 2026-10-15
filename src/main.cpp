#include <Arduino.h>
#include <math.h>
#include "buttons.h"
#include "config.h"
#include "gesture.h"
#include "glove_comm.h"
#include "glove_imu.h"
#include "leds.h"
#include "packets.h"

static uint32_t s_lastTickMs = 0;
static bool s_pendingGestureDirection = false;
static uint8_t s_gestureDirection = CMD_NONE;
static bool s_pendingReadyBeforeDirection = false;

static void onGestureResult(GestureDecision decision, bool valid) {
  if (!valid || decision == GESTURE_NONE) {
    ledsBlinkReject(2);
    return;
  }

  s_gestureDirection = static_cast<uint8_t>(decision);
  s_pendingReadyBeforeDirection = true;
  s_pendingGestureDirection = true;
}

static void handleSerial() {
  while (Serial.available() > 0) {
    char command = static_cast<char>(Serial.read());
    if (command == '\r' || command == '\n') {
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
  if (!commHasFreshStatus()) {
    ledsBlinkReject(3);
    return;
  }

  uint8_t state = commLastDroneState();
  if (state == DRONE_ARMED) {
    commStartCommand(CMD_BTN_READY, 6);
    return;
  }

  if (state == DRONE_FLYING) {
    if (imuHasError()) {
      ledsBlinkReject(3);
      return;
    }
    if (!gestureStart(onGestureResult)) {
      ledsBlinkReject(2);
    }
    return;
  }

  ledsBlinkReject(3);
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
  uint32_t startWait = millis();
  while (!Serial && (millis() - startWait) < 1500UL) {
    yield();
  }

  Serial.println();
  Serial.println("[BOOT] GloveController ESP32-C3 SuperMini");
  buttonsBegin();
  ledsBegin();
  imuBegin();
  gestureBegin();
  commBegin();
  s_lastTickMs = millis();
}

void loop() {
  handleSerial();
  gestureUpdate();
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
}
