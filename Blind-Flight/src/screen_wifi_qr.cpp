#include "screens.h"
#include "config.h"
#include "audio.h"
#include "input.h"
#include "wifi_portal.h"
#include "transitions.h"

#include <qrcode.h>

// ============================================================
// Blind Flight — Join Wi-Fi Screen
// ============================================================
// Displays a QR code encoding the AP credentials so guests can
// scan to join. Also shows the password and WebSocket PIN as
// text for manual entry.
// ============================================================

static void qrDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("JOIN WI-FI", COL_HOME);

    const char* ssid = "BlindFlight";
    const char* pass = wifiGetAPPassword();
    const char* pin  = wifiGetPIN();

    // Build the Wi-Fi QR payload
    char qrData[80];
    snprintf(qrData, sizeof(qrData), "WIFI:T:WPA;S:%s;P:%s;;", ssid, pass);

    // Generate QR code (version 3 = 29x29 modules, enough for ~40 chars)
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(3)];
    qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, qrData);

    // Center the QR code in the content area
    int qrSize = qrcode.size;  // 29 for version 3
    int scale = 4;             // 4px per module → 116px total
    int qrPx = qrSize * scale;
    int qrX = (SCREEN_W - qrPx) / 2;
    int qrY = CONTENT_Y + 4;

    // White border around QR (quiet zone)
    int quiet = scale * 2;
    tft->fillRect(qrX - quiet, qrY - quiet,
                  qrPx + quiet * 2, qrPx + quiet * 2, TFT_WHITE);

    // Draw QR modules
    for (int y = 0; y < qrSize; y++) {
        for (int x = 0; x < qrSize; x++) {
            uint16_t color = qrcode_getModule(&qrcode, x, y) ? TFT_BLACK : TFT_WHITE;
            tft->fillRect(qrX + x * scale, qrY + y * scale, scale, scale, color);
        }
    }

    // Text below QR
    int textY = qrY + qrPx + quiet + 8;

    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(FONT_SMALL);
    tft->setTextColor(COL_DIM, COL_BG);
    tft->drawString("Scan QR or enter manually:", SCREEN_W / 2, textY);

    textY += 16;
    tft->setTextSize(FONT_BODY);
    tft->setTextColor(COL_TEXT, COL_BG);
    char buf[48];
    snprintf(buf, sizeof(buf), "Pass: %s", pass);
    tft->drawString(buf, SCREEN_W / 2, textY);

    textY += 22;
    tft->setTextColor(COL_ACCENT, COL_BG);
    snprintf(buf, sizeof(buf), "PIN: %s", pin);
    tft->drawString(buf, SCREEN_W / 2, textY);

    uiDrawSoftButtons("BACK", "");
}

static void qrInput(InputEvent evt) {
    switch (evt) {
        case INPUT_BTN_LEFT:
        case INPUT_ENC_CLICK:
            audioPlayTone(TONE_SELECT);
            uiPopScreenT(TRANS_WIPE_LEFT);
            break;
        default:
            break;
    }
}

static void qrOnEnter() {}

const Screen screenWifiQR = {
    "WifiQR",
    qrDraw,
    qrInput,
    qrOnEnter
};
