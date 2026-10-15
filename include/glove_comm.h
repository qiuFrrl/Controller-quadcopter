#pragma once

#include <Arduino.h>
#include "packets.h"

void commBegin();
void commTick();
bool commDroneMacConfigured();
bool commHasFreshStatus();
uint8_t commLastDroneState();
uint32_t commStatusAgeMs();
void commPrintDiagnostics();

bool commStartCommand(uint8_t command, uint8_t repeatTicks);
void commEmergencyCommand();
bool commIsBusy();
void commSendImmediate(uint8_t command, uint16_t throttle = 0);
