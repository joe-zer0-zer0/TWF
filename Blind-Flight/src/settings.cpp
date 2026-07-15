#include "settings.h"
#include "config.h"
#include "battery.h"
#include <Preferences.h>

// ============================================================
// Blind Flight — Settings Module (Session 17)
// ============================================================

// --- Lookup tables ---
// Brightness level 0–4 → backlight PWM (20% floor to 100%)
const uint8_t BRIGHT_MAP[5] = { 51, 102, 153, 204, 255 };

// Volume level 0–4 → LEDC duty cycle (0 = silent, 4 = max).
// For a square wave, perceived loudness peaks near 50% duty (128/255)
// and falls off toward either rail — 100% duty is a constant level,
// i.e. no AC signal into the buzzer coil, and can sound quieter than
// a lower duty step. Capped at 128 (50%) instead of running to 255
// (Session 19) so the table stays monotonically louder with level.
// Re-check on the bench if max volume still sounds off.
const uint8_t VOLUME_DUTY[5] = { 0, 32, 64, 96, 128 };

// Spin speed labels: 0=Fast, 1=Normal, 2=Theatrical
// (label array lives in screen_settings.cpp; motor params in motor.cpp)

// --- NVS ---
static Preferences prefs;
static const char* NVS_NS = "bf";

// --- RAM shadow of stored settings ---
static bool    sSoundOn   = true;
static uint8_t sVolume    = 3;      // 0–4
static uint8_t sBrightness = 4;    // 0–4
static bool    sWifiOn    = false;
static uint8_t sSpinSpeed = 1;     // 0=Fast, 1=Normal, 2=Theatrical
static uint8_t sPourSide  = 0;    // 0=Front, 1=Right, 2=Rear, 3=Left
static int16_t sHomeOffset = 0;   // microstep trim for pour alignment
static uint8_t  sGlassCount = 4;  // 2–4 glasses per flight
static uint16_t sDimDelay  = DEFAULT_DIM_DELAY;   // seconds
static uint16_t sOffDelay  = DEFAULT_OFF_DELAY;   // seconds
static bool    sDirty     = false;

// ============================================================
// Init / Save
// ============================================================

void settingsInit() {
    prefs.begin(NVS_NS, true);  // read-only open

    sSoundOn    = prefs.getBool("sndOn",   true);
    sVolume     = prefs.getUChar("sndVol", 3);
    sBrightness = prefs.getUChar("bright", 4);
    sWifiOn     = prefs.getBool("wifiOn",  false);
    sSpinSpeed  = prefs.getUChar("spinSpd", 1);
    sPourSide   = prefs.getUChar("pourSd",  0);
    sHomeOffset = prefs.getShort("homeOff", 0);
    sGlassCount = prefs.getUChar("glassCt", 4);
    sDimDelay   = prefs.getUShort("dimDly", DEFAULT_DIM_DELAY);
    sOffDelay   = prefs.getUShort("offDly", DEFAULT_OFF_DELAY);

    prefs.end();

    // Clamp to valid ranges
    if (sVolume > 4)     sVolume = 4;
    if (sBrightness > 4) sBrightness = 4;
    if (sSpinSpeed > 2)  sSpinSpeed = 2;
    if (sPourSide > 3)   sPourSide = 3;
    if (sHomeOffset < -200) sHomeOffset = -200;
    if (sHomeOffset >  200) sHomeOffset =  200;
    if (sGlassCount < 2) sGlassCount = 2;
    if (sGlassCount > 4) sGlassCount = 4;
    if (sDimDelay < 30)   sDimDelay = 30;
    if (sDimDelay > 300)  sDimDelay = 300;
    if (sOffDelay < 120)  sOffDelay = 120;
    if (sOffDelay > 1800) sOffDelay = 1800;

    sDirty = false;

    Serial.printf("[Settings] Loaded: snd=%d vol=%d bright=%d wifi=%d spd=%d pour=%d homeOff=%d\n",
                  sSoundOn, sVolume, sBrightness, sWifiOn, sSpinSpeed, sPourSide, sHomeOffset);
}

void settingsSave() {
    if (!sDirty) return;

    prefs.begin(NVS_NS, false);  // read-write

    prefs.putBool("sndOn",    sSoundOn);
    prefs.putUChar("sndVol",  sVolume);
    prefs.putUChar("bright",  sBrightness);
    prefs.putBool("wifiOn",   sWifiOn);
    prefs.putUChar("spinSpd", sSpinSpeed);
    prefs.putUChar("pourSd",  sPourSide);
    prefs.putShort("homeOff", sHomeOffset);
    prefs.putUChar("glassCt",  sGlassCount);
    prefs.putUShort("dimDly",  sDimDelay);
    prefs.putUShort("offDly",  sOffDelay);

    prefs.end();

    sDirty = false;
    Serial.println("[Settings] Saved to NVS");
}

// ============================================================
// Sound
// ============================================================

bool settingsGetSoundOn()       { return sSoundOn; }
void settingsSetSoundOn(bool v) { sSoundOn = v; sDirty = true; }

uint8_t settingsGetVolume()          { return sVolume; }
void    settingsSetVolume(uint8_t v) { if (v > 4) v = 4; sVolume = v; sDirty = true; }

// ============================================================
// Display
// ============================================================

uint8_t settingsGetBrightness()          { return sBrightness; }
void    settingsSetBrightness(uint8_t v) { if (v > 4) v = 4; sBrightness = v; sDirty = true; }

// ============================================================
// Wi-Fi
// ============================================================

bool settingsGetWifiOn()       { return sWifiOn; }
void settingsSetWifiOn(bool v) { sWifiOn = v; sDirty = true; }

// ============================================================
// Motor
// ============================================================

uint8_t settingsGetSpinSpeed()          { return sSpinSpeed; }
void    settingsSetSpinSpeed(uint8_t v) { if (v > 2) v = 2; sSpinSpeed = v; sDirty = true; }

uint8_t settingsGetPourSide()          { return sPourSide; }
void    settingsSetPourSide(uint8_t v) { if (v > 3) v = 3; sPourSide = v; sDirty = true; }

int16_t settingsGetHomeOffset()           { return sHomeOffset; }
void    settingsSetHomeOffset(int16_t v)  {
    if (v < -200) v = -200;
    if (v >  200) v =  200;
    sHomeOffset = v;
    sDirty = true;
}

// ============================================================
// Glass count
// ============================================================

uint8_t settingsGetGlassCount()          { return sGlassCount; }
void    settingsSetGlassCount(uint8_t v) {
    if (v < 2) v = 2;
    if (v > 4) v = 4;
    sGlassCount = v;
    sDirty = true;
}

// ============================================================
// Display timeout
// ============================================================

uint16_t settingsGetDimDelay()           { return sDimDelay; }
void     settingsSetDimDelay(uint16_t v) {
    if (v < 30)  v = 30;
    if (v > 300) v = 300;
    sDimDelay = v;
    sDirty = true;
}

uint16_t settingsGetOffDelay()           { return sOffDelay; }
void     settingsSetOffDelay(uint16_t v) {
    if (v < 120)  v = 120;
    if (v > 1800) v = 1800;
    sOffDelay = v;
    sDirty = true;
}

// ============================================================
// Battery ADC — thin wrappers over battery module
// ============================================================

float settingsReadBatteryV() {
    return batteryGetVoltage();
}

int settingsBatteryPercent() {
    return batteryGetPercent();
}
