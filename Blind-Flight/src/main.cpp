#include <Arduino.h>
#include <TFT_eSPI.h>

#include "config.h"
#include "settings.h"
#include "battery.h"
#include "audio.h"
#include "input.h"
#include "motor.h"
#include "transitions.h"
#include "ui.h"
#include "screens.h"
#include "splash.h"
#include "game.h"
#include "wifi_portal.h"
#include "diagnostics.h"
#include "persist.h"
#include "favorites.h"
#include "ota.h"

// ============================================================
// Blind Flight — Session 17: Settings & Persistent Preferences
// ============================================================
// Builds on Sessions 1–16 to add:
//
//   - Settings screen with NVS-backed persistent preferences
//   - Sound mute/volume, brightness, Wi-Fi toggle, spin speed
//   - Battery voltage monitoring via ADC voltage divider
//   - Re-Home utility action from settings
//   - About/Info screen with live battery and system stats
//   - Conditional Wi-Fi init (only if setting is enabled)
//   - Motor spin speed adjustable at runtime
//
// Test: Flash → settings defaults load → toggle settings →
//       reboot → settings persist → battery reads in About
// ============================================================

// Global TFT instance — shared via uiInit()
TFT_eSPI tft = TFT_eSPI();

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Blind Flight v" FW_VERSION " ===\n");

    // Confirm current firmware is valid (prevents OTA rollback)
    otaMarkValid();

    // --- Persistent settings (must be first — other modules read from it) ---
    settingsInit();
    favoritesInit();
    batteryInit();
    persistInit();
    diagInit();

    // --- Display init ---
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(COL_BG);

    // --- Backlight PWM init (before uiInit so backlight is on) ---
    transInit();

    // Apply saved brightness immediately
    setBacklight(BRIGHT_MAP[settingsGetBrightness()]);

    // --- Module init ---
    audioInit();            // Reads mute/volume from settings
    inputInit();
    motorInit();
    motorSetSpinSpeed(settingsGetSpinSpeed());  // Apply saved spin speed
    motorSetPourSide(settingsGetPourSide());    // Apply saved pour side
    motorSetHomeOffset(settingsGetHomeOffset()); // Apply saved calibration trim
    uiInit(&tft);

    // --- Wi-Fi portal (conditional on saved setting) ---
    if (settingsGetWifiOn()) {
        wifiPortalInit();
    } else {
        Serial.println("[Main] Wi-Fi disabled by settings");
    }

    // --- Push splash screen (animation runs in onEnter) ---
    // Splash handles: animation → wait for input → homing → home screen
    uiPushScreen(&screenSplash);

    Serial.println("[Main] Setup complete, entering main loop\n");
}

void loop() {
    // 1. Battery sampling (returns immediately unless interval elapsed)
    batteryUpdate();

    // 2. Advance non-blocking tone sequencer
    audioUpdate();

    // 3. Poll hardware inputs → fill event queue
    inputUpdate();

    // 4. Drain events to active screen, redraw if needed
    uiUpdate();

    // 5. Service Wi-Fi portal (returns immediately if not running)
    wifiPortalUpdate();

    // Yield to watchdog
    delay(1);
}
