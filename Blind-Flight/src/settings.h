#pragma once

// ============================================================
// Blind Flight — Settings Module (Session 17)
// ============================================================
// Persistent preferences via ESP32 NVS (Preferences library).
// Runtime getters/setters with dirty-flag tracking.
// Battery voltage monitoring via ADC voltage divider.
//
// Call settingsInit() early in setup(), before other modules.
// Call settingsSave() to flush dirty values to NVS (e.g., on
// settings screen exit).
// ============================================================

#include <Arduino.h>

// --- Initialization & persistence ---
void     settingsInit();           // Read NVS into RAM, call in setup()
void     settingsSave();           // Flush dirty values to NVS

// --- Sound ---
bool     settingsGetSoundOn();
void     settingsSetSoundOn(bool v);
uint8_t  settingsGetVolume();      // 0–4
void     settingsSetVolume(uint8_t v);

// --- Display ---
uint8_t  settingsGetBrightness();  // 0–4
void     settingsSetBrightness(uint8_t v);

// --- Wi-Fi ---
bool     settingsGetWifiOn();
void     settingsSetWifiOn(bool v);

// --- Motor ---
uint8_t  settingsGetSpinSpeed();   // 0=Fast, 1=Normal, 2=Theatrical
void     settingsSetSpinSpeed(uint8_t v);
uint8_t  settingsGetPourSide();    // 0=Front, 1=Right, 2=Rear, 3=Left
void     settingsSetPourSide(uint8_t v);
int16_t  settingsGetHomeOffset();  // microstep trim added to POUR_OFFSET
void     settingsSetHomeOffset(int16_t v);

// --- Glass count ---
uint8_t  settingsGetGlassCount();  // 2–4 (default 4)
void     settingsSetGlassCount(uint8_t v);

// --- Display timeout ---
uint16_t settingsGetDimDelay();    // seconds (default 120)
void     settingsSetDimDelay(uint16_t v);
uint16_t settingsGetOffDelay();    // seconds (default 600)
void     settingsSetOffDelay(uint16_t v);

// --- Battery (wrappers — real logic in battery module) ---
float    settingsReadBatteryV();
int      settingsBatteryPercent();

// --- Lookup tables (public for modules that need the raw values) ---
extern const uint8_t BRIGHT_MAP[5];    // level 0–4 → backlight PWM 0–255
extern const uint8_t VOLUME_DUTY[5];   // level 0–4 → LEDC duty cycle
