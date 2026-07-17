#include "screens.h"
#include "config.h"
#include "audio.h"
#include "motor.h"
#include "input.h"
#include "settings.h"
#include "transitions.h"
#include "wifi_portal.h"
#include "favorites.h"
#include "device_id.h"

// ============================================================
// Blind Flight — Settings Screen (Session 17)
// ============================================================
// Custom-drawn scrollable list with edit mode for each setting.
// Includes Re-Home action and About info sub-screen.
// ============================================================

// --- Settings list items ---
enum SettingsItem {
    SET_SOUND = 0,
    SET_BRIGHT,
    SET_WIFI,
    SET_WIFIQR,
    SET_WIFISETUP,
    SET_SPEED,
    SET_POURSIDE,
    SET_GLASSES,
    SET_DIM_DELAY,
    SET_OFF_DELAY,
    SET_FAVORITES,
    SET_CALIBRATE,
    SET_REHOME,
    SET_MOTORTEST,
    SET_ABOUT,
    SET_UPDATE,
    SET_COUNT       // 16
};

// How many items fit in the content area
static const int VISIBLE_ITEMS = CONTENT_H / MENU_ITEM_H;  // 4

// --- List state ---
static int  setSelected    = 0;
static int  setScrollOff   = 0;

// --- Edit mode ---
static bool editMode       = false;
static int  editValue      = 0;   // temp value being edited

// --- Re-Home confirmation ---
static bool rehomeConfirm  = false;

// --- Speed label lookup ---
static const char* speedLabels[]    = { "Fast", "Normal", "Slow" };
static const char* pourSideLabels[] = { "Front", "Right", "Rear", "Left" };

// ============================================================
// Helpers: build dot indicators
// ============================================================

// Write filled/empty dots into buf. e.g. level=3, max=5 → "###.."
// Returns pointer to buf for chaining.
static char* dotsStr(char* buf, int level, int maxLevel) {
    int i = 0;
    for (; i < level; i++)    buf[i] = '#';
    for (; i < maxLevel; i++) buf[i] = '.';
    buf[i] = '\0';
    return buf;
}

// ============================================================
// Helpers: draw a single settings row
// ============================================================

static void drawSettingsItem(int screenIdx, int itemIdx, bool selected, bool editing) {
    TFT_eSPI* tft = uiGetTFT();

    int y = CONTENT_Y + screenIdx * MENU_ITEM_H;
    int x = MENU_ITEM_X;
    int w = MENU_ITEM_W;
    int h = MENU_ITEM_H;

    // Background
    uint16_t bgCol = selected ? (editing ? COL_ACCENT : COL_HIGHLIGHT) : COL_BG;
    tft->fillRect(x, y, w, h, bgCol);

    // Text colors
    uint16_t labelCol = COL_TEXT;
    uint16_t valCol   = editing ? COL_BG : COL_ACCENT;

    tft->setTextSize(FONT_BODY);
    tft->setTextDatum(ML_DATUM);
    int textY = y + h / 2;

    // Draw label on the left
    const char* label = "";
    switch (itemIdx) {
        case SET_SOUND:  label = "Sound";      break;
        case SET_BRIGHT: label = "Brightness"; break;
        case SET_WIFI:      label = "Wi-Fi";      break;
        case SET_WIFIQR:    label = "Join Wi-Fi"; break;
        case SET_WIFISETUP: label = "Wi-Fi Setup"; break;
        case SET_SPEED:    label = "Spin Speed"; break;
        case SET_POURSIDE:  label = "Pour Side";  break;
        case SET_GLASSES:   label = "Glasses";   break;
        case SET_DIM_DELAY: label = "Dim";       break;
        case SET_OFF_DELAY: label = "Sleep";     break;
        case SET_FAVORITES: label = "Favorites"; break;
        case SET_CALIBRATE: label = "Calibrate"; break;
        case SET_REHOME:    label = "Re-Home";   break;
        case SET_MOTORTEST: label = "Motor Test"; break;
        case SET_ABOUT:     label = "About";     break;
        case SET_UPDATE:    label = "Update";    break;
    }
    tft->setTextColor(labelCol, bgCol);
    tft->drawString(label, x + 4, textY);

    // Draw value on the right
    char valBuf[20] = "";
    char dotBuf[6];
    int rightX = x + w - 4;
    tft->setTextDatum(MR_DATUM);
    tft->setTextColor(valCol, bgCol);

    int dispValue = editing ? editValue : -1;  // use editValue if editing

    switch (itemIdx) {
        case SET_SOUND: {
            bool on;
            uint8_t vol;
            if (editing) {
                on  = (dispValue > 0);
                vol = on ? dispValue : 0;
            } else {
                on  = settingsGetSoundOn();
                vol = settingsGetVolume();
            }
            if (!on) {
                snprintf(valBuf, sizeof(valBuf), "OFF");
            } else {
                dotsStr(dotBuf, vol, 4);
                snprintf(valBuf, sizeof(valBuf), "ON %s", dotBuf);
            }
            break;
        }
        case SET_BRIGHT: {
            uint8_t br = editing ? (uint8_t)dispValue : settingsGetBrightness();
            dotsStr(dotBuf, br + 1, 5);  // level 0 = 1 dot, level 4 = 5 dots
            snprintf(valBuf, sizeof(valBuf), "%s", dotBuf);
            break;
        }
        case SET_WIFI: {
            bool on = editing ? (dispValue != 0) : settingsGetWifiOn();
            snprintf(valBuf, sizeof(valBuf), "%s", on ? "ON" : "OFF");
            break;
        }
        case SET_SPEED: {
            uint8_t spd = editing ? (uint8_t)dispValue : settingsGetSpinSpeed();
            if (spd > 2) spd = 2;
            snprintf(valBuf, sizeof(valBuf), "%s", speedLabels[spd]);
            break;
        }
        case SET_POURSIDE: {
            uint8_t ps = editing ? (uint8_t)dispValue : settingsGetPourSide();
            if (ps > 3) ps = 3;
            snprintf(valBuf, sizeof(valBuf), "%s", pourSideLabels[ps]);
            break;
        }
        case SET_GLASSES: {
            uint8_t gc = editing ? (uint8_t)dispValue : settingsGetGlassCount();
            snprintf(valBuf, sizeof(valBuf), "%d", gc);
            break;
        }
        case SET_DIM_DELAY: {
            uint16_t d = editing ? (uint16_t)dispValue : settingsGetDimDelay();
            if (d >= 60) {
                snprintf(valBuf, sizeof(valBuf), "%dm", d / 60);
            } else {
                snprintf(valBuf, sizeof(valBuf), "%ds", d);
            }
            break;
        }
        case SET_OFF_DELAY: {
            uint16_t d = editing ? (uint16_t)dispValue : settingsGetOffDelay();
            snprintf(valBuf, sizeof(valBuf), "%dm", d / 60);
            break;
        }
        case SET_FAVORITES: {
            snprintf(valBuf, sizeof(valBuf), "%d/%d >", favoritesGetCount(), FAV_MAX_COUNT);
            break;
        }
        case SET_CALIBRATE: {
            int16_t off = settingsGetHomeOffset();
            if (off == 0) {
                snprintf(valBuf, sizeof(valBuf), "0 >");
            } else {
                snprintf(valBuf, sizeof(valBuf), "%+d >", off);
            }
            break;
        }
        case SET_WIFIQR: {
            if (wifiPortalIsRunning() && !wifiIsSTAMode()) {
                snprintf(valBuf, sizeof(valBuf), "QR >");
            } else {
                tft->setTextColor(COL_DIM, bgCol);
                snprintf(valBuf, sizeof(valBuf), wifiIsSTAMode() ? "STA" : "Off");
            }
            break;
        }
        case SET_WIFISETUP: {
            if (wifiIsSTAMode()) {
                snprintf(valBuf, sizeof(valBuf), "STA >");
            } else if (wifiHasCredentials()) {
                snprintf(valBuf, sizeof(valBuf), "AP >");
            } else {
                tft->setTextColor(COL_DIM, bgCol);
                snprintf(valBuf, sizeof(valBuf), ">");
            }
            break;
        }
        case SET_UPDATE: {
            if (wifiIsSTAMode()) {
                snprintf(valBuf, sizeof(valBuf), "v%s >", FW_VERSION);
            } else {
                tft->setTextColor(COL_DIM, bgCol);
                snprintf(valBuf, sizeof(valBuf), "No Wi-Fi");
            }
            break;
        }
        case SET_REHOME:
        case SET_MOTORTEST:
        case SET_ABOUT:
            // No value — action items
            tft->setTextColor(COL_DIM, bgCol);
            snprintf(valBuf, sizeof(valBuf), ">");
            break;
    }

    tft->drawString(valBuf, rightX, textY);
}

// ============================================================
// Helpers: draw the full settings list
// ============================================================

static void drawSettingsList() {
    for (int i = 0; i < VISIBLE_ITEMS && (setScrollOff + i) < SET_COUNT; i++) {
        int itemIdx = setScrollOff + i;
        bool isSel  = (itemIdx == setSelected);
        drawSettingsItem(i, itemIdx, isSel, isSel && editMode);
    }

    // Clear any unused rows
    TFT_eSPI* tft = uiGetTFT();
    int drawnCount = SET_COUNT - setScrollOff;
    if (drawnCount < VISIBLE_ITEMS) {
        for (int i = drawnCount; i < VISIBLE_ITEMS; i++) {
            int y = CONTENT_Y + i * MENU_ITEM_H;
            tft->fillRect(MENU_ITEM_X, y, MENU_ITEM_W, MENU_ITEM_H, COL_BG);
        }
    }

    // Scroll indicator (only if list overflows)
    if (SET_COUNT > VISIBLE_ITEMS) {
        int trackY = CONTENT_Y + SCROLL_BAR_PAD;
        int trackH = CONTENT_H - 2 * SCROLL_BAR_PAD;

        tft->fillRect(SCROLL_BAR_X, trackY, SCROLL_BAR_W, trackH, COL_SCROLL_BG);

        int thumbH = max(8, trackH * VISIBLE_ITEMS / SET_COUNT);
        int thumbY = trackY + (trackH - thumbH) * setScrollOff / (SET_COUNT - VISIBLE_ITEMS);

        tft->fillRect(SCROLL_BAR_X, thumbY, SCROLL_BAR_W, thumbH, COL_SCROLL_FG);
    }
}

// ============================================================
// Scroll management
// ============================================================

static void ensureVisible() {
    if (setSelected < setScrollOff) {
        setScrollOff = setSelected;
    } else if (setSelected >= setScrollOff + VISIBLE_ITEMS) {
        setScrollOff = setSelected - VISIBLE_ITEMS + 1;
    }
}

// ============================================================
// Edit mode: start / apply / cancel
// ============================================================

static void editStart() {
    editMode = true;
    switch (setSelected) {
        case SET_SOUND:
            // Combined: 0 = OFF, 1-4 = ON at that volume
            editValue = settingsGetSoundOn() ? settingsGetVolume() : 0;
            if (editValue < 1 && settingsGetSoundOn()) editValue = 1;
            break;
        case SET_BRIGHT:
            editValue = settingsGetBrightness();
            break;
        case SET_WIFI:
            editValue = settingsGetWifiOn() ? 1 : 0;
            break;
        case SET_SPEED:
            editValue = settingsGetSpinSpeed();
            break;
        case SET_POURSIDE:
            editValue = settingsGetPourSide();
            break;
        case SET_GLASSES:
            editValue = settingsGetGlassCount();
            break;
        case SET_DIM_DELAY:
            editValue = settingsGetDimDelay();
            break;
        case SET_OFF_DELAY:
            editValue = settingsGetOffDelay();
            break;
        default:
            editMode = false;
            break;
    }
}

static void editApply() {
    switch (setSelected) {
        case SET_SOUND:
            if (editValue == 0) {
                settingsSetSoundOn(false);
                audioSetMute(true);
            } else {
                settingsSetSoundOn(true);
                settingsSetVolume(editValue);
                audioSetMute(false);
                audioSetVolume(editValue);
            }
            break;
        case SET_BRIGHT:
            settingsSetBrightness(editValue);
            setBacklight(BRIGHT_MAP[editValue]);
            break;
        case SET_WIFI:
            settingsSetWifiOn(editValue != 0);
            // Start/stop Wi-Fi immediately
            if (editValue && !wifiPortalIsRunning()) {
                wifiPortalInit();
            } else if (!editValue && wifiPortalIsRunning()) {
                wifiPortalStop();
            }
            break;
        case SET_SPEED:
            settingsSetSpinSpeed(editValue);
            motorSetSpinSpeed(editValue);
            break;
        case SET_POURSIDE:
            settingsSetPourSide(editValue);
            motorSetPourSide(editValue);
            break;
        case SET_GLASSES:
            settingsSetGlassCount(editValue);
            break;
        case SET_DIM_DELAY:
            settingsSetDimDelay(editValue);
            break;
        case SET_OFF_DELAY:
            settingsSetOffDelay(editValue);
            break;
    }
    editMode = false;
}

static void editCancel() {
    // Restore live preview state for brightness
    if (setSelected == SET_BRIGHT) {
        setBacklight(BRIGHT_MAP[settingsGetBrightness()]);
    }
    editMode = false;
}

static void editAdjust(int delta) {
    switch (setSelected) {
        case SET_SOUND:
            editValue += delta;
            if (editValue < 0) editValue = 0;
            if (editValue > 4) editValue = 4;
            // Live preview: adjust volume immediately
            if (editValue == 0) {
                audioSetMute(true);
            } else {
                audioSetMute(false);
                audioSetVolume(editValue);
            }
            break;
        case SET_BRIGHT:
            editValue += delta;
            if (editValue < 0) editValue = 0;
            if (editValue > 4) editValue = 4;
            // Live preview: adjust brightness immediately
            setBacklight(BRIGHT_MAP[editValue]);
            break;
        case SET_WIFI:
            editValue = editValue ? 0 : 1;  // toggle
            break;
        case SET_SPEED:
            editValue += delta;
            if (editValue < 0) editValue = 0;
            if (editValue > 2) editValue = 2;
            break;
        case SET_POURSIDE:
            editValue += delta;
            if (editValue < 0) editValue = 3;
            if (editValue > 3) editValue = 0;
            break;
        case SET_GLASSES:
            editValue += delta;
            if (editValue < 2) editValue = 2;
            if (editValue > 4) editValue = 4;
            break;
        case SET_DIM_DELAY:
            editValue += delta * 30;  // 30s steps
            if (editValue < 30)  editValue = 30;
            if (editValue > 300) editValue = 300;
            break;
        case SET_OFF_DELAY:
            editValue += delta * 60;  // 1m steps
            if (editValue < 120)  editValue = 120;
            if (editValue > 1800) editValue = 1800;
            break;
    }
}

// ============================================================
// SETTINGS SCREEN — Screen callbacks
// ============================================================

static void settingsDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    uiDrawTitleBar("SETTINGS", COL_ACCENT);
    drawSettingsList();

    if (rehomeConfirm) {
        uiDrawSoftButtons("CANCEL", "CONFIRM");
    } else if (editMode) {
        uiDrawSoftButtons("CANCEL", "OK");
    } else {
        uiDrawSoftButtons("BACK", "SELECT");
    }
}

static void settingsOnEnter() {
    setSelected = 0;
    setScrollOff = 0;
    editMode = false;
    rehomeConfirm = false;
}

static void settingsInput(InputEvent evt) {
    // --- Re-Home confirmation mode ---
    if (rehomeConfirm) {
        if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
            rehomeConfirm = false;
            audioPlayTone(TONE_CONFIRM);
            // Run blocking homing sequence
            bool ok = runHomingSequence();
            if (ok) {
                audioPlayTone(TONE_HOME_FOUND);
            }
            // Redraw settings screen after homing
            uiRequestRedraw();
            return;
        }
        if (evt == INPUT_BTN_LEFT) {
            rehomeConfirm = false;
            audioPlayTone(TONE_SELECT);
            uiRequestRedraw();
            return;
        }
        return;
    }

    // --- Edit mode ---
    if (editMode) {
        switch (evt) {
            case INPUT_ENC_CW:
                editAdjust(1);
                audioPlayTone(TONE_CLICK);
                drawSettingsList();
                // Update soft buttons if needed
                uiDrawSoftButtons("CANCEL", "OK");
                return;
            case INPUT_ENC_CCW:
                editAdjust(-1);
                audioPlayTone(TONE_CLICK);
                drawSettingsList();
                uiDrawSoftButtons("CANCEL", "OK");
                return;
            case INPUT_BTN_RIGHT:
            case INPUT_ENC_CLICK:
                editApply();
                audioPlayTone(TONE_CONFIRM);
                uiRequestRedraw();
                return;
            case INPUT_BTN_LEFT:
                editCancel();
                audioPlayTone(TONE_SELECT);
                uiRequestRedraw();
                return;
            default:
                return;
        }
    }

    // --- Normal list navigation ---
    switch (evt) {
        case INPUT_ENC_CW:
            if (setSelected < SET_COUNT - 1) {
                setSelected++;
                ensureVisible();
                audioPlayTone(TONE_CLICK);
                drawSettingsList();
            }
            break;

        case INPUT_ENC_CCW:
            if (setSelected > 0) {
                setSelected--;
                ensureVisible();
                audioPlayTone(TONE_CLICK);
                drawSettingsList();
            }
            break;

        case INPUT_ENC_LONG:
            if (setSelected == SET_ABOUT) {
                // Hidden: long-press encoder on About opens diagnostics
                audioPlayTone(TONE_CONFIRM);
                extern const Screen screenDiagnostics;
                uiPushScreenT(&screenDiagnostics, TRANS_WIPE_LEFT);
            }
            break;

        case INPUT_BTN_RIGHT:
        case INPUT_ENC_CLICK:
            if (setSelected == SET_REHOME) {
                // Show confirmation prompt
                rehomeConfirm = true;
                audioPlayTone(TONE_SELECT);
                uiRequestRedraw();
            } else if (setSelected == SET_ABOUT) {
                // Push About screen
                audioPlayTone(TONE_SELECT);
                extern const Screen screenAbout;
                uiPushScreenT(&screenAbout, TRANS_WIPE_LEFT);
            } else if (setSelected == SET_FAVORITES) {
                audioPlayTone(TONE_SELECT);
                extern const Screen screenFavorites;
                uiPushScreenT(&screenFavorites, TRANS_WIPE_LEFT);
            } else if (setSelected == SET_WIFIQR) {
                if (wifiPortalIsRunning() && !wifiIsSTAMode()) {
                    audioPlayTone(TONE_SELECT);
                    extern const Screen screenWifiQR;
                    uiPushScreenT(&screenWifiQR, TRANS_WIPE_LEFT);
                } else {
                    audioPlayTone(TONE_ERROR);
                }
            } else if (setSelected == SET_WIFISETUP) {
                audioPlayTone(TONE_SELECT);
                extern const Screen screenWifiSetup;
                uiPushScreenT(&screenWifiSetup, TRANS_WIPE_LEFT);
            } else if (setSelected == SET_CALIBRATE) {
                audioPlayTone(TONE_SELECT);
                extern const Screen screenCalibrate;
                uiPushScreenT(&screenCalibrate, TRANS_WIPE_LEFT);
            } else if (setSelected == SET_MOTORTEST) {
                // Push motor diagnostics screen
                audioPlayTone(TONE_SELECT);
                extern const Screen screenMotorTest;
                uiPushScreenT(&screenMotorTest, TRANS_WIPE_LEFT);
            } else if (setSelected == SET_UPDATE) {
                if (wifiIsSTAMode()) {
                    audioPlayTone(TONE_SELECT);
                    extern const Screen screenOtaUpdate;
                    uiPushScreenT(&screenOtaUpdate, TRANS_WIPE_LEFT);
                } else {
                    audioPlayTone(TONE_ERROR);
                }
            } else {
                // Enter edit mode for value items
                editStart();
                audioPlayTone(TONE_SELECT);
                uiRequestRedraw();
            }
            break;

        case INPUT_BTN_LEFT:
            // Save settings and go back
            settingsSave();
            audioPlayTone(TONE_SELECT);
            uiPopScreenT(TRANS_WIPE_LEFT);
            break;

        default:
            break;
    }
}

const Screen screenSettings = {
    "Settings",
    settingsDraw,
    settingsInput,
    settingsOnEnter
};

// ============================================================
// ABOUT SCREEN
// ============================================================
// Shows firmware version, battery voltage/%, Wi-Fi client
// count, and motor homing state. Battery and Wi-Fi update
// live on a 1-second timer via partial redraw.
// ============================================================

static unsigned long aboutLastRefresh = 0;
static float aboutLastV   = 0;
static int   aboutLastPct = 0;
static int   aboutLastClients = 0;

static void aboutDrawValues() {
    TFT_eSPI* tft = uiGetTFT();
    char buf[32];

    int labelX = 16;
    int valX   = SCREEN_W - 16;
    int startY = 80;
    int lineH  = 24;

    tft->setTextSize(FONT_BODY);

    // Battery
    int y = startY + lineH;
    tft->fillRect(120, y - 8, 120, 20, COL_BG);  // clear value area
    snprintf(buf, sizeof(buf), "%.2fV (%d%%)", aboutLastV, aboutLastPct);
    tft->setTextColor(COL_ACCENT, COL_BG);
    tft->setTextDatum(MR_DATUM);
    tft->drawString(buf, valX, y);

    // Wi-Fi
    y = startY + 2 * lineH;
    tft->fillRect(120, y - 8, 120, 20, COL_BG);
    if (wifiPortalIsRunning()) {
        if (wifiIsSTAMode()) {
            snprintf(buf, sizeof(buf), "STA %dc", aboutLastClients);
        } else {
            snprintf(buf, sizeof(buf), "AP %dc", aboutLastClients);
        }
        tft->setTextColor(COL_SELECTED, COL_BG);
    } else {
        snprintf(buf, sizeof(buf), "OFF");
        tft->setTextColor(COL_DIM, COL_BG);
    }
    tft->setTextDatum(MR_DATUM);
    tft->drawString(buf, valX, y);
}

static void aboutDraw(bool fullRedraw) {
    TFT_eSPI* tft = uiGetTFT();

    if (fullRedraw) {
        tft->fillScreen(COL_BG);
        uiDrawTitleBar("ABOUT", COL_HOME);

        int labelX = 16;
        int startY = 80;
        int lineH  = 24;

        // Version
        tft->setTextSize(FONT_BODY);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_TEXT, COL_BG);
        tft->drawString("Firmware", labelX, startY);

        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_ACCENT, COL_BG);
        char vBuf[12];
        snprintf(vBuf, sizeof(vBuf), "v%s", FW_VERSION);
        tft->drawString(vBuf, SCREEN_W - 16, startY);

        // Labels
        int y = startY + lineH;
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_TEXT, COL_BG);
        tft->drawString("Batt", labelX, y);

        y += lineH;
        tft->drawString("Wi-Fi", labelX, y);

        // SSID (if connected)
        if (wifiPortalIsRunning() && wifiIsSTAMode()) {
            y += lineH;
            tft->setTextSize(FONT_SMALL);
            tft->setTextDatum(ML_DATUM);
            tft->setTextColor(COL_DIM, COL_BG);
            tft->drawString("SSID", labelX + 8, y);
            tft->setTextDatum(MR_DATUM);
            tft->setTextColor(COL_ACCENT, COL_BG);
            tft->drawString(wifiGetSSID(), SCREEN_W - 16, y);

            y += 16;
            tft->setTextDatum(ML_DATUM);
            tft->setTextColor(COL_DIM, COL_BG);
            tft->drawString("IP", labelX + 8, y);
            tft->setTextDatum(MR_DATUM);
            tft->setTextColor(COL_ACCENT, COL_BG);
            tft->drawString(wifiGetIP(), SCREEN_W - 16, y);
            tft->setTextSize(FONT_BODY);
        }

        y += lineH;
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_TEXT, COL_BG);
        tft->drawString("Glasses", labelX, y);
        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_ACCENT, COL_BG);
        tft->drawString(String(NUM_GLASSES), SCREEN_W - 16, y);

        y += lineH;
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_TEXT, COL_BG);
        tft->drawString("Motor", labelX, y);
        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(motorIsHomed() ? COL_SELECTED : COL_DIM, COL_BG);
        tft->drawString(motorIsHomed() ? "Homed" : "Not Homed", SCREEN_W - 16, y);

        // Device serial number
        y += lineH;
        tft->setTextSize(FONT_SMALL);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_DIM, COL_BG);
        tft->drawString("Serial", labelX, y);
        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_ACCENT, COL_BG);
        tft->drawString(deviceGetSerial(), SCREEN_W - 16, y);
        tft->setTextSize(FONT_BODY);

        uiDrawSoftButtons("BACK", "");

        // Read initial values
        aboutLastV       = settingsReadBatteryV();
        aboutLastPct     = settingsBatteryPercent();
        aboutLastClients = wifiPortalGetClientCount();
        aboutLastRefresh = millis();

        aboutDrawValues();
    }

    // Partial refresh: update battery and Wi-Fi every second
    if (millis() - aboutLastRefresh >= 1000) {
        aboutLastRefresh = millis();
        aboutLastV       = settingsReadBatteryV();
        aboutLastPct     = settingsBatteryPercent();
        aboutLastClients = wifiPortalGetClientCount();
        aboutDrawValues();
    }
}

static void aboutInput(InputEvent evt) {
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

static void aboutOnEnter() {
    aboutLastRefresh = 0;  // Force immediate read on first draw
}

const Screen screenAbout = {
    "About",
    aboutDraw,
    aboutInput,
    aboutOnEnter
};
