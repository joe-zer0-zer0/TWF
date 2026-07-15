#include "screens.h"
#include "config.h"
#include "audio.h"
#include "input.h"
#include "wifi_portal.h"
#include "transitions.h"

// ============================================================
// Blind Flight — Wi-Fi Setup Screen
// ============================================================
// Settings sub-screen for viewing/managing Wi-Fi connection.
//
// If no credentials saved: shows instructions to use AP mode
//   and phone UI to configure.
// If credentials saved: shows SSID, IP, signal strength, and
//   options to Forget or Reconnect.
// ============================================================

enum WifiSetupItem {
    WSET_FORGET = 0,
    WSET_RECONNECT,
    WSET_COUNT
};

static int  wsetSelected = 0;
static bool forgetConfirm = false;

// RSSI to bar count (1-4)
static int rssiToBars(int8_t rssi) {
    if (rssi > -50) return 4;
    if (rssi > -60) return 3;
    if (rssi > -70) return 2;
    return 1;
}

static void drawRssiBars(TFT_eSPI* tft, int x, int y, int bars) {
    int barW = 4, gap = 2, maxH = 16;
    for (int i = 0; i < 4; i++) {
        int h = 4 + i * 4;
        int bx = x + i * (barW + gap);
        int by = y + (maxH - h);
        uint16_t col = (i < bars) ? COL_SELECTED : COL_DIM;
        tft->fillRect(bx, by, barW, h, col);
    }
}

static void wifiSetupDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("WI-FI SETUP", COL_HOME);

    if (!wifiHasCredentials()) {
        // No credentials — show bootstrap instructions
        uiDrawCenteredText("No network", 70, FONT_BODY, COL_TEXT);
        uiDrawCenteredText("configured", 95, FONT_BODY, COL_TEXT);

        uiDrawCenteredText("Connect to", 135, FONT_SMALL, COL_DIM);
        uiDrawCenteredText("BlindFlight Wi-Fi", 150, FONT_SMALL, COL_ACCENT);
        uiDrawCenteredText("on your phone, then", 165, FONT_SMALL, COL_DIM);
        uiDrawCenteredText("open flight.local", 180, FONT_SMALL, COL_ACCENT);
        uiDrawCenteredText("to configure", 195, FONT_SMALL, COL_DIM);

        uiDrawSoftButtons("BACK", "");

    } else if (forgetConfirm) {
        uiDrawCenteredText("Forget network?", 80, FONT_BODY, COL_TEXT);
        uiDrawCenteredText(wifiGetSSID(), 110, FONT_BODY, COL_ACCENT);
        uiDrawHint("AP mode on next restart", 150);
        uiDrawSoftButtons("CANCEL", "FORGET");

    } else {
        // Show connection info
        int y = CONTENT_Y + 8;
        int labelX = 16;
        int valX = SCREEN_W - 16;

        tft->setTextSize(FONT_BODY);

        // Mode
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_TEXT, COL_BG);
        tft->drawString("Mode", labelX, y);
        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_ACCENT, COL_BG);
        tft->drawString(wifiIsSTAMode() ? "STA" : "AP", valX, y);

        // SSID
        y += 26;
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_TEXT, COL_BG);
        tft->drawString("SSID", labelX, y);
        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_ACCENT, COL_BG);
        tft->drawString(wifiGetSSID(), valX, y);

        // IP
        y += 26;
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_TEXT, COL_BG);
        tft->drawString("IP", labelX, y);
        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_ACCENT, COL_BG);
        tft->drawString(wifiGetIP(), valX, y);

        // Signal strength (STA only)
        if (wifiIsSTAMode() && wifiIsConnected()) {
            y += 26;
            tft->setTextDatum(ML_DATUM);
            tft->setTextColor(COL_TEXT, COL_BG);
            tft->drawString("Signal", labelX, y);
            int bars = rssiToBars(wifiGetRSSI());
            drawRssiBars(tft, valX - 30, y - 8, bars);
        } else if (wifiIsSTAMode()) {
            y += 26;
            tft->setTextDatum(ML_DATUM);
            tft->setTextColor(COL_ERROR, COL_BG);
            tft->drawString("Disconnected", labelX, y);
        }

        // Action items
        y += 36;
        for (int i = 0; i < WSET_COUNT; i++) {
            bool sel = (i == wsetSelected);
            uint16_t bg = sel ? COL_HIGHLIGHT : COL_BG;
            tft->fillRect(MENU_ITEM_X, y, MENU_ITEM_W, MENU_ITEM_H - 4, bg);

            tft->setTextSize(FONT_BODY);
            tft->setTextDatum(ML_DATUM);

            const char* label = (i == WSET_FORGET) ? "Forget Network" : "Reconnect";
            uint16_t col = (i == WSET_FORGET) ? COL_ERROR : COL_ACCENT;
            tft->setTextColor(col, bg);
            tft->drawString(label, MENU_ITEM_X + 4, y + (MENU_ITEM_H - 4) / 2);

            y += MENU_ITEM_H;
        }

        uiDrawSoftButtons("BACK", "SELECT");
    }
}

static void wifiSetupOnEnter() {
    wsetSelected = 0;
    forgetConfirm = false;
}

static void wifiSetupInput(InputEvent evt) {
    if (forgetConfirm) {
        if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
            wifiForgetCredentials();
            forgetConfirm = false;
            audioPlayTone(TONE_CONFIRM);
            uiRequestRedraw();
            return;
        }
        if (evt == INPUT_BTN_LEFT) {
            forgetConfirm = false;
            audioPlayTone(TONE_SELECT);
            uiRequestRedraw();
            return;
        }
        return;
    }

    if (!wifiHasCredentials()) {
        // No credentials — only Back works
        if (evt == INPUT_BTN_LEFT || evt == INPUT_ENC_CLICK) {
            audioPlayTone(TONE_SELECT);
            uiPopScreenT(TRANS_WIPE_LEFT);
        }
        return;
    }

    switch (evt) {
        case INPUT_ENC_CW:
            if (wsetSelected < WSET_COUNT - 1) {
                wsetSelected++;
                audioPlayTone(TONE_CLICK);
                uiRequestRedraw();
            }
            break;

        case INPUT_ENC_CCW:
            if (wsetSelected > 0) {
                wsetSelected--;
                audioPlayTone(TONE_CLICK);
                uiRequestRedraw();
            }
            break;

        case INPUT_BTN_RIGHT:
        case INPUT_ENC_CLICK:
            if (wsetSelected == WSET_FORGET) {
                forgetConfirm = true;
                audioPlayTone(TONE_SELECT);
                uiRequestRedraw();
            } else if (wsetSelected == WSET_RECONNECT) {
                audioPlayTone(TONE_SELECT);

                TFT_eSPI* tft = uiGetTFT();
                tft->fillScreen(COL_BG);
                uiDrawTitleBar("CONNECTING", COL_HOME);
                uiDrawCenteredText("Reconnecting...", 120, FONT_BODY, COL_TEXT);
                uiDrawHint(wifiGetSSID(), 150);

                wifiReconnect();

                uiRequestRedraw();
            }
            break;

        case INPUT_BTN_LEFT:
            audioPlayTone(TONE_SELECT);
            uiPopScreenT(TRANS_WIPE_LEFT);
            break;

        default:
            break;
    }
}

const Screen screenWifiSetup = {
    "WifiSetup",
    wifiSetupDraw,
    wifiSetupInput,
    wifiSetupOnEnter
};
