#pragma once

// ============================================================
// Blind Flight — Device Identity Module
// ============================================================
// Derives a permanent, unique serial number from the ESP32's
// factory-burned eFuse MAC address. Format: "BF-XXXXXXXXXXXX"
// (12 hex digits). Survives reflashing and cannot be overwritten.
//
// Call deviceIdInit() early in setup(), before wifi_portal.
// ============================================================

#include <Arduino.h>

void deviceIdInit();

// "BF-XXXXXXXXXXXX" — 16 chars including null terminator
const char* deviceGetSerial();

// Raw 48-bit MAC as uint64_t (lower 48 bits valid)
uint64_t deviceGetMAC();
