#include "diagnostics.h"
#include <Preferences.h>

// ============================================================
// Blind Flight — Diagnostics Module
// ============================================================

static Preferences diagPrefs;
static const char* DIAG_NS = "bfdiag";

// RAM cache of counters (avoids repeated NVS reads for display)
static uint16_t sTotalFlights = 0;
static uint16_t sModeCounts[4] = {0, 0, 0, 0};
static uint8_t  sLogHead = 0;
static uint16_t sLogCount = 0;

// NVS key names for mode counters
static const char* MODE_KEYS[] = { "cntBasic", "cntNamed", "cntGuess", "cntRank" };

void diagInit() {
    diagPrefs.begin(DIAG_NS, true);  // read-only

    sTotalFlights = diagPrefs.getUShort("totalFlts", 0);
    for (int i = 0; i < 4; i++) {
        sModeCounts[i] = diagPrefs.getUShort(MODE_KEYS[i], 0);
    }
    sLogHead  = diagPrefs.getUChar("logHead", 0);
    sLogCount = diagPrefs.getUShort("logCount", 0);
    if (sLogCount > DIAG_LOG_SIZE) sLogCount = DIAG_LOG_SIZE;
    if (sLogHead >= DIAG_LOG_SIZE) sLogHead = 0;

    diagPrefs.end();

    Serial.printf("[Diag] Loaded: total=%d, basic=%d, named=%d, guess=%d, rank=%d, log=%d\n",
                  sTotalFlights, sModeCounts[0], sModeCounts[1],
                  sModeCounts[2], sModeCounts[3], sLogCount);
}

void diagLogFlight(uint8_t mode, int pourCount, const int* pourOrder) {
    if (pourCount < 1 || pourCount > 4) return;

    // Build the 5-byte record: [mode, g1, g2, g3, g4]
    uint8_t record[5];
    record[0] = mode;
    for (int i = 0; i < 4; i++) {
        record[i + 1] = (i < pourCount) ? (uint8_t)pourOrder[i] : 0;
    }

    // Write to NVS
    diagPrefs.begin(DIAG_NS, false);

    // Store record at current head position
    char key[8];
    snprintf(key, sizeof(key), "log%02d", sLogHead);
    diagPrefs.putBytes(key, record, 5);

    // Advance head
    sLogHead = (sLogHead + 1) % DIAG_LOG_SIZE;
    diagPrefs.putUChar("logHead", sLogHead);

    // Update count (caps at DIAG_LOG_SIZE)
    if (sLogCount < DIAG_LOG_SIZE) sLogCount++;
    diagPrefs.putUShort("logCount", sLogCount);

    // Update counters
    sTotalFlights++;
    diagPrefs.putUShort("totalFlts", sTotalFlights);

    if (mode < 4) {
        sModeCounts[mode]++;
        diagPrefs.putUShort(MODE_KEYS[mode], sModeCounts[mode]);
    }

    diagPrefs.end();

    // Log the glass order for serial debugging
    char orderStr[16];
    int pos = 0;
    for (int i = 0; i < pourCount; i++) {
        if (i > 0) orderStr[pos++] = ',';
        orderStr[pos++] = '0' + pourOrder[i];
    }
    orderStr[pos] = '\0';

    Serial.printf("[Diag] Flight #%d logged: mode=%d, order=%s\n",
                  sTotalFlights, mode, orderStr);
}

uint16_t diagGetTotalFlights() {
    return sTotalFlights;
}

uint16_t diagGetModeCount(uint8_t mode) {
    if (mode >= 4) return 0;
    return sModeCounts[mode];
}

int diagGetLogCount() {
    return sLogCount;
}

bool diagGetRecord(int index, FlightRecord* out) {
    if (index < 0 || index >= (int)sLogCount || !out) return false;

    // index 0 = most recent = (sLogHead - 1), index 1 = second most recent, etc.
    int slot = ((int)sLogHead - 1 - index + DIAG_LOG_SIZE * 2) % DIAG_LOG_SIZE;

    char key[8];
    snprintf(key, sizeof(key), "log%02d", slot);

    uint8_t record[5];
    diagPrefs.begin(DIAG_NS, true);
    size_t len = diagPrefs.getBytes(key, record, 5);
    diagPrefs.end();

    if (len != 5) return false;

    out->mode = record[0];
    out->pourCount = 0;
    for (int i = 0; i < 4; i++) {
        out->glassOrder[i] = record[i + 1];
        if (record[i + 1] > 0) out->pourCount++;
    }
    return true;
}

void diagClearAll() {
    diagPrefs.begin(DIAG_NS, false);
    diagPrefs.clear();
    diagPrefs.end();

    sTotalFlights = 0;
    for (int i = 0; i < 4; i++) sModeCounts[i] = 0;
    sLogHead = 0;
    sLogCount = 0;

    Serial.println("[Diag] All data cleared");
}
