#include <Arduino.h>
#include <math.h>
#include "config.h"
#include "buttons.h"

struct ButtonState {
  int pin;
  bool stablePressed;
  bool lastReadingPressed;
  bool edge;
  uint32_t lastChangeMs;
};

static ButtonState s_buttons[BUTTON_COUNT] = {
  {BTN_ARM_PIN, false, false, false, 0},
  {BTN_EMERGENCY_PIN, false, false, false, 0},
  {BTN_READY_PIN, false, false, false, 0}
};

static bool pinConfigured(int pin) {
  return pin >= 0;
}

void buttonsBegin() {
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    if (pinConfigured(s_buttons[i].pin)) {
      pinMode(s_buttons[i].pin, INPUT_PULLUP);
    } else {
      Serial.println("[BTN] Ada pin tombol yang masih -1 di config.h.");
    }
  }
}

void buttonsUpdate() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    s_buttons[i].edge = false;
    if (!pinConfigured(s_buttons[i].pin)) {
      continue;
    }

    bool readingPressed = digitalRead(s_buttons[i].pin) == LOW;
    if (readingPressed != s_buttons[i].lastReadingPressed) {
      s_buttons[i].lastReadingPressed = readingPressed;
      s_buttons[i].lastChangeMs = now;
    }

    if ((now - s_buttons[i].lastChangeMs) >= 30UL && readingPressed != s_buttons[i].stablePressed) {
      s_buttons[i].stablePressed = readingPressed;
      if (s_buttons[i].stablePressed) {
        s_buttons[i].edge = true;
      }
    }
  }
}

bool buttonPressedEdge(ButtonId button) {
  if (button >= BUTTON_COUNT) {
    return false;
  }
  return s_buttons[button].edge;
}
