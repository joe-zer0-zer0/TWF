#include "settings.h"
#include "config.h"
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

    prefs.end();

    // Clamp to valid ranges
    if (sVolume > 4)     sVolume = 4;
    if (sBrightness > 4) sBrightness = 4;
    if (sSpinSpeed > 2)  sSpinSpeed = 2;
    if (sPourSide > 3)   sPourSide = 3;

    sDirty = false;

    // Configure battery ADC pin
    pinMode(PIN_BATT_ADC, INPUT);
    analogSetAttenuation(ADC_11db);  // Full 0–3.3V range

    Serial.printf("[Settings] Loaded: snd=%d vol=%d bright=%d wifi=%d spd=%d pour=%d\n",
                  sSoundOn, sVolume, sBrightness, sWifiOn, sSpinSpeed, sPourSide);
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

// ============================================================
// Battery ADC
// ============================================================

float settingsReadBatteryV() {
    // Multi-sample average to smooth ADC noise.
    // analogReadMilliVolts() (Session 19) applies the ESP32's factory
    // eFuse ADC calibration (offset + nonlinearity correction) instead
    // of the raw-count linear approximation (raw * 3.3/4095), which can
    // be off by 100–200 mV — worst near the top of the 11 dB range,
    // right where full-charge readings sit.
    const int SAMPLES = 16;
    long sumMv = 0;
    for (int i = 0; i < SAMPLES; i++) {
        sumMv += analogReadMilliVolts(PIN_BATT_ADC);
    }
    float adcV = ((float)sumMv / SAMPLES) / 1000.0f;

    // Reverse the voltage divider to get battery voltage
    float battV = adcV / BATT_DIV_RATIO;

    return battV;
}

int settingsBatteryPercent() {
    float v = settingsReadBatteryV();

    if (v >= BATT_FULL_V) return 100;
    if (v <= BATT_EMPTY_V) return 0;

    float pct = (v - BATT_EMPTY_V) / (BATT_FULL_V - BATT_EMPTY_V) * 100.0f;
    return (int)(pct + 0.5f);
}
