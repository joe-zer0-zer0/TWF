#include <Arduino.h>

#include "config.h"
#include "settings.h"
#include "battery.h"
#include "audio.h"
#include "input.h"
#include "motor.h"
#include "game.h"
#include "wifi_portal.h"
#include "diagnostics.h"
#include "persist.h"
#include "favorites.h"
#include "ota.h"
#include "device_id.h"

#ifndef HEADLESS_BUILD
#include <TFT_eSPI.h>
#include "transitions.h"
#include "ui.h"
#include "screens.h"
#include "splash.h"

TFT_eSPI tft = TFT_eSPI();
#endif

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Blind Flight v" FW_VERSION " ===\n");

    // NOTE: the running image is deliberately NOT confirmed here. It used
    // to be, on this exact line, which cancelled bootloader rollback before
    // any of the init below had a chance to fail. Confirmation now happens
    // in loop() via otaValidateTick() once the image has proven it can run.
    // See ota.h.

    deviceIdInit();

    settingsInit();
    favoritesInit();
    batteryInit();
    persistInit();
    diagInit();

#ifndef HEADLESS_BUILD
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(COL_BG);

    transInit();
    setBacklight(BRIGHT_MAP[settingsGetBrightness()]);

    audioInit();
    inputInit();
    motorInit();
    motorSetSpinSpeed(settingsGetSpinSpeed());
    motorSetPourSide(settingsGetPourSide());
    motorSetHomeOffset(settingsGetHomeOffset());
    uiInit(&tft);

    if (settingsGetWifiOn()) {
        wifiPortalInit();
    } else {
        Serial.println("[Main] Wi-Fi disabled by settings");
    }

    uiPushScreen(&screenSplash);
#else
    audioInit();
    inputInit();
    motorInit();
    motorSetSpinSpeed(settingsGetSpinSpeed());
    motorSetPourSide(settingsGetPourSide());
    motorSetHomeOffset(settingsGetHomeOffset());

    wifiPortalInit();

    Serial.println("[Main] Headless mode — Wi-Fi portal active");
#endif

    Serial.println("[Main] Setup complete, entering main loop\n");
}

void loop() {
    otaValidateTick();      // confirms a freshly-installed image once healthy

    batteryUpdate();
    audioUpdate();
    inputUpdate();

#ifndef HEADLESS_BUILD
    uiUpdate();
#endif

    wifiPortalUpdate();

    delay(1);
}
