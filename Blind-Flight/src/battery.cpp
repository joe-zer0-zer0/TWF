#include "battery.h"
#include "config.h"

static float smoothedV = 0;
static int   cachedPct = 0;
static unsigned long lastSampleMs = 0;

#define BATT_SAMPLE_INTERVAL_MS  2000
#define BATT_SMOOTHING           0.2f
#define BATT_WARN_PCT            25
#define BATT_LOCKOUT_PCT         15

// Li-ion 2S discharge curve (voltage → percent)
// Measured points along a typical discharge profile.
struct VoltPct { float v; int pct; };
static const VoltPct LUT[] = {
    { 8.40f, 100 },
    { 8.20f,  90 },
    { 8.00f,  75 },
    { 7.80f,  60 },
    { 7.60f,  45 },
    { 7.40f,  30 },
    { 7.20f,  15 },
    { 7.00f,   5 },
    { 6.00f,   0 },
};
static const int LUT_SIZE = sizeof(LUT) / sizeof(LUT[0]);

static float readRawVoltage() {
    const int SAMPLES = 16;
    long sumMv = 0;
    for (int i = 0; i < SAMPLES; i++) {
        sumMv += analogReadMilliVolts(PIN_BATT_ADC);
    }
    float adcV = ((float)sumMv / SAMPLES) / 1000.0f;
    return adcV / BATT_DIV_RATIO;
}

static int voltToPercent(float v) {
    if (v >= LUT[0].v) return 100;
    if (v <= LUT[LUT_SIZE - 1].v) return 0;

    for (int i = 0; i < LUT_SIZE - 1; i++) {
        if (v >= LUT[i + 1].v) {
            float range = LUT[i].v - LUT[i + 1].v;
            float frac = (v - LUT[i + 1].v) / range;
            return LUT[i + 1].pct + (int)(frac * (LUT[i].pct - LUT[i + 1].pct) + 0.5f);
        }
    }
    return 0;
}

void batteryInit() {
    pinMode(PIN_BATT_ADC, INPUT);
    analogSetAttenuation(ADC_11db);

    smoothedV = readRawVoltage();
    cachedPct = voltToPercent(smoothedV);
    lastSampleMs = millis();

    Serial.printf("[Battery] Init: %.2fV (%d%%)\n", smoothedV, cachedPct);
}

void batteryUpdate() {
    unsigned long now = millis();
    if (now - lastSampleMs < BATT_SAMPLE_INTERVAL_MS) return;
    lastSampleMs = now;

    float raw = readRawVoltage();
    smoothedV = smoothedV * (1.0f - BATT_SMOOTHING) + raw * BATT_SMOOTHING;
    cachedPct = voltToPercent(smoothedV);
}

float batteryGetVoltage()  { return smoothedV; }
int   batteryGetPercent()  { return cachedPct; }
bool  batteryIsLow()       { return cachedPct <= BATT_WARN_PCT; }
bool  batteryIsLockout()   { return cachedPct <= BATT_LOCKOUT_PCT; }
