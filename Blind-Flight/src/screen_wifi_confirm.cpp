#include "screens.h"
#include "config.h"
#include "audio.h"
#include "input.h"
#include "wifi_portal.h"
#include "transitions.h"

// ============================================================
// Blind Flight — Wi-Fi Connect Confirmation Screen
// ============================================================
// Shown when a phone sends a wifi_connect command. Requires
// physical button press to approve joining a new network.
// Auto-pops if the pending request is cancelled or times out.
// ============================================================

static void confirmDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();

    if (!wifiGetPendingConnect()) {
        uiPopScreenT(TRANS_WIPE_LEFT);
        return;
    }

    tft->fillScreen(COL_BG);
    uiDrawTitleBar("CONNECT?", COL_MOVING);

    uiDrawCenteredText("Join network:", 90, FONT_BODY, COL_TEXT);

    const char* ssid = wifiGetPendingSSID();
    uiDrawCenteredText(ssid, 120, FONT_BODY, COL_ACCENT);

    uiDrawHint("Requested from phone", 160);
    uiDrawHint("AP will shut down", 180);

    uiDrawSoftButtons("DENY", "ALLOW");
}

static void confirmInput(InputEvent evt) {
    switch (evt) {
        case INPUT_BTN_RIGHT:
        case INPUT_ENC_CLICK:
            audioPlayTone(TONE_CONFIRM);
            wifiConfirmConnect(true);
            uiPopScreenT(TRANS_WIPE_LEFT);
            break;

        case INPUT_BTN_LEFT:
            audioPlayTone(TONE_SELECT);
            wifiConfirmConnect(false);
            uiPopScreenT(TRANS_WIPE_LEFT);
            break;

        default:
            break;
    }
}

static void confirmOnEnter() {}

const Screen screenWifiConfirm = {
    "WifiConfirm",
    confirmDraw,
    confirmInput,
    confirmOnEnter
};
