#pragma once

#include <Arduino.h>

void testControlBegin();
void testControlUpdate();
bool testControlHandleSerial(char command);
void testControlTriggerReady();
bool testControlIsReading();
