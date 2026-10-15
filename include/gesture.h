#pragma once

#include <Arduino.h>
#include "packets.h"

enum GestureDecision : uint8_t {
  GESTURE_NONE = CMD_NONE,
  GESTURE_MAJU = CMD_MAJU,
  GESTURE_MUNDUR = CMD_MUNDUR,
  GESTURE_KIRI = CMD_KIRI,
  GESTURE_KANAN = CMD_KANAN
};

typedef void (*GestureResultCallback)(GestureDecision decision, bool valid);

void gestureBegin();
bool gestureStart(GestureResultCallback callback);
void gestureUpdate();
bool gestureIsActive();
