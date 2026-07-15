#include "screens.h"
#include "config.h"
#include "audio.h"
#include "input.h"
#include "ota.h"
#include "wifi_portal.h"
#include "transitions.h"

// ============================================================
// Blind Flight — OTA Update Screen
// ============================================================
// Settings sub-screen for checking and installing firmware
// updates. Multi-step flow:
//
//   PROMPT  → "Check for Updates?" [Back / Check]
//   CHECK   → "Checking..." (fetching manifest)
//   NOUPD   → "Up to date (vX.Y.Z)" [Back]
//   AVAIL   → "Update available: vX.Y.Z" [Cancel / Install]
//   DOWNLOAD→ Progress bar [non-cancellable]
//   SUCCESS → "Update complete! Restarting..."
//   FAILURE → "Update failed: <reason>" [Back]
// ============================================================

enum OtaPhase {
    OTA_PROMPT,
    OTA_CHECKING,
    OTA_NO_UPDATE,
    OTA_AVAILABLE,
    OTA_DOWNLOADING,
    OTA_SUCCESS,
    OTA_FAILURE
};

static OtaPhase    otaPhase;
static OtaUpdateInfo otaInfo;
static char         otaErrMsg[64];
static uint8_t      otaProgress;  // 0-100

// Progress callback — stashes percentage for the draw loop
static void otaProgressCb(uint32_t done, uint32_t total) {
    if (total > 0) {
        otaProgress = (uint8_t)((done * 100UL) / total);
    }
    // Redraw progress bar directly (we're in a blocking call)
    TFT_eSPI* tft = uiGetTFT();

    int barX = 20, barY = 140, barW = SCREEN_W - 40, barH = 20;
    int fillW = (barW - 4) * otaProgress / 100;

    tft->fillRect(barX + 2, barY + 2, barW - 4, barH - 4, COL_BG);
    if (fillW > 0) {
        tft->fillRect(barX + 2, barY + 2, fillW, barH - 4, COL_ACCENT);
    }

    // Percent text
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", otaProgress);
    tft->fillRect(80, 170, 80, 20, COL_BG);
    tft->setTextColor(COL_TEXT, COL_BG);
    tft->setTextSize(FONT_BODY);
    tft->setTextDatum(MC_DATUM);
    tft->drawString(buf, SCREEN_W / 2, 180);
}

static void otaDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    switch (otaPhase) {
        case OTA_PROMPT:
            uiDrawTitleBar("UPDATE", COL_HOME);
            uiDrawCenteredText("Check for", 90, FONT_BODY, COL_TEXT);
            uiDrawCenteredText("updates?", 115, FONT_BODY, COL_TEXT);
            {
                char vBuf[24];
                snprintf(vBuf, sizeof(vBuf), "Current: v%s", FW_VERSION);
                uiDrawHint(vBuf, 155);
            }
            uiDrawSoftButtons("BACK", "CHECK");
            break;

        case OTA_CHECKING:
            uiDrawTitleBar("UPDATE", COL_HOME);
            uiDrawCenteredText("Checking...", 110, FONT_BODY, COL_TEXT);
            uiDrawHint("Fetching manifest", 145);
            uiDrawSoftButtons("", "");
            break;

        case OTA_NO_UPDATE:
            uiDrawTitleBar("UP TO DATE", COL_SELECTED);
            uiDrawCenteredText("No update", 90, FONT_BODY, COL_SELECTED);
            uiDrawCenteredText("available", 115, FONT_BODY, COL_SELECTED);
            {
                char vBuf[24];
                snprintf(vBuf, sizeof(vBuf), "v%s", FW_VERSION);
                uiDrawCenteredText(vBuf, 150, FONT_BODY, COL_ACCENT);
            }
            uiDrawSoftButtons("BACK", "");
            break;

        case OTA_AVAILABLE:
            uiDrawTitleBar("UPDATE", COL_MOVING);
            uiDrawCenteredText("Update available", 70, FONT_BODY, COL_TEXT);
            {
                char vBuf[24];
                snprintf(vBuf, sizeof(vBuf), "v%s", otaInfo.version);
                uiDrawCenteredText(vBuf, 100, FONT_LARGE, COL_ACCENT);
            }
            if (otaInfo.notes[0]) {
                uiDrawCenteredTextWrap(otaInfo.notes, 140, FONT_SMALL,
                                       COL_DIM, SCREEN_W - 32);
            }
            if (otaInfo.size > 0) {
                char sBuf[24];
                snprintf(sBuf, sizeof(sBuf), "%u KB",
                         otaInfo.size / 1024);
                uiDrawHint(sBuf, 195);
            }
            uiDrawHint("Wi-Fi clients disconnect", 215);
            uiDrawSoftButtons("CANCEL", "INSTALL");
            break;

        case OTA_DOWNLOADING:
            uiDrawTitleBar("UPDATING", COL_MOVING);
            uiDrawCenteredText("Downloading", 100, FONT_BODY, COL_TEXT);
            {
                char vBuf[24];
                snprintf(vBuf, sizeof(vBuf), "v%s", otaInfo.version);
                uiDrawCenteredText(vBuf, 120, FONT_SMALL, COL_ACCENT);
            }
            {
                int barX = 20, barY = 140, barW = SCREEN_W - 40, barH = 20;
                tft->drawRect(barX, barY, barW, barH, COL_DIM);
            }
            uiDrawHint("Do not power off", 210);
            uiDrawSoftButtons("", "");
            break;

        case OTA_SUCCESS:
            uiDrawTitleBar("COMPLETE", COL_SELECTED);
            uiDrawCenteredText("Update", 90, FONT_BODY, COL_SELECTED);
            uiDrawCenteredText("complete!", 115, FONT_BODY, COL_SELECTED);
            uiDrawHint("Restarting...", 155);
            uiDrawSoftButtons("", "");
            break;

        case OTA_FAILURE:
            uiDrawTitleBar("FAILED", COL_ERROR);
            uiDrawCenteredText("Update failed", 90, FONT_BODY, COL_ERROR);
            uiDrawCenteredTextWrap(otaErrMsg, 130, FONT_BODY,
                                   COL_TEXT, SCREEN_W - 32);
            uiDrawSoftButtons("BACK", "");
            break;
    }
}

static void otaOnEnter() {
    otaPhase = OTA_PROMPT;
    otaProgress = 0;
    otaErrMsg[0] = '\0';
    memset(&otaInfo, 0, sizeof(otaInfo));
}

static void doCheck() {
    otaPhase = OTA_CHECKING;
    uiRequestRedraw();

    // Force a synchronous redraw so the "Checking..." text shows
    // before the blocking HTTP call.
    otaDraw(true);

    bool ok = otaCheckForUpdate(OTA_MANIFEST_URL, otaInfo,
                                otaErrMsg, sizeof(otaErrMsg));
    if (!ok) {
        otaPhase = OTA_FAILURE;
    } else if (otaInfo.available) {
        otaPhase = OTA_AVAILABLE;
    } else {
        otaPhase = OTA_NO_UPDATE;
    }
    uiRequestRedraw();
}

static void doInstall() {
    otaPhase = OTA_DOWNLOADING;
    otaProgress = 0;
    uiRequestRedraw();
    otaDraw(true);

    bool ok = otaPerformUpdate(otaInfo.url, otaInfo.size,
                               otaProgressCb,
                               otaErrMsg, sizeof(otaErrMsg));
    if (ok) {
        otaPhase = OTA_SUCCESS;
        uiRequestRedraw();
        otaDraw(true);

        audioPlayTone(TONE_CONFIRM);
        delay(2000);
        ESP.restart();
    } else {
        otaPhase = OTA_FAILURE;
        audioPlayTone(TONE_ERROR);
        uiRequestRedraw();
    }
}

static void otaInput(InputEvent evt) {
    switch (otaPhase) {
        case OTA_PROMPT:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_SELECT);
                doCheck();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                uiPopScreenT(TRANS_WIPE_LEFT);
            }
            break;

        case OTA_NO_UPDATE:
        case OTA_FAILURE:
            if (evt == INPUT_BTN_LEFT || evt == INPUT_BTN_RIGHT ||
                evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_SELECT);
                uiPopScreenT(TRANS_WIPE_LEFT);
            }
            break;

        case OTA_AVAILABLE:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_CONFIRM);
                doInstall();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                uiPopScreenT(TRANS_WIPE_LEFT);
            }
            break;

        // CHECKING, DOWNLOADING, SUCCESS — no input accepted
        default:
            break;
    }
}

const Screen screenOtaUpdate = {
    "OtaUpdate",
    otaDraw,
    otaInput,
    otaOnEnter
};
