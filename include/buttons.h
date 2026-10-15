#pragma once

#include <Arduino.h>

enum ButtonId : uint8_t {
  BUTTON_ARM = 0,
  BUTTON_EMERGENCY = 1,
  BUTTON_READY = 2,
  BUTTON_COUNT = 3
};

void buttonsBegin();
void buttonsUpdate();
bool buttonPressedEdge(ButtonId button);
