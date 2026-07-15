#pragma once

#include <Arduino.h>

void     batteryInit();
void     batteryUpdate();

float    batteryGetVoltage();
int      batteryGetPercent();
bool     batteryIsLow();
bool     batteryIsLockout();
