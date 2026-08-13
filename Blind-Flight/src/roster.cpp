#include "roster.h"
#include "party.h"     // PARTY_NAME_LEN
#include <Preferences.h>

static const char* NVS_NS = "bfroster";

static char    rosterNames[ROSTER_MAX_COUNT][PARTY_NAME_LEN];
static uint8_t rosterCount = 0;

static Preferences rosterPrefs;

static void rosterSaveAll() {
    rosterPrefs.begin(NVS_NS, false);
    rosterPrefs.putUChar("cnt", rosterCount);
    char key[4];
    for (int i = 0; i < rosterCount; i++) {
        snprintf(key, sizeof(key), "r%02d", i);
        rosterPrefs.putString(key, rosterNames[i]);
    }
    // Clear keys beyond the current count, left behind by earlier removes.
    for (int i = rosterCount; i < ROSTER_MAX_COUNT; i++) {
        snprintf(key, sizeof(key), "r%02d", i);
        rosterPrefs.remove(key);
    }
    rosterPrefs.end();
}

void rosterInit() {
    rosterPrefs.begin(NVS_NS, true);
    rosterCount = rosterPrefs.getUChar("cnt", 0);
    if (rosterCount > ROSTER_MAX_COUNT) rosterCount = ROSTER_MAX_COUNT;

    char key[4];
    for (int i = 0; i < rosterCount; i++) {
        snprintf(key, sizeof(key), "r%02d", i);
        String val = rosterPrefs.getString(key, "");
        strncpy(rosterNames[i], val.c_str(), PARTY_NAME_LEN - 1);
        rosterNames[i][PARTY_NAME_LEN - 1] = '\0';
    }
    rosterPrefs.end();

    Serial.printf("[Roster] Loaded %d names\n", rosterCount);
}

uint8_t rosterGetCount() {
    return rosterCount;
}

const char* rosterGetName(int index) {
    if (index < 0 || index >= rosterCount) return "";
    return rosterNames[index];
}

bool rosterContains(const char* name) {
    if (!name || !name[0]) return false;
    for (int i = 0; i < rosterCount; i++) {
        if (strcasecmp(rosterNames[i], name) == 0) return true;
    }
    return false;
}

bool rosterAdd(const char* name) {
    if (!name || !name[0]) return false;
    if (rosterContains(name)) return false;
    if (rosterCount >= ROSTER_MAX_COUNT) return false;

    strncpy(rosterNames[rosterCount], name, PARTY_NAME_LEN - 1);
    rosterNames[rosterCount][PARTY_NAME_LEN - 1] = '\0';
    rosterCount++;

    rosterSaveAll();
    Serial.printf("[Roster] Added '%s' (%d/%d)\n", name, rosterCount, ROSTER_MAX_COUNT);
    return true;
}

bool rosterRemove(int index) {
    if (index < 0 || index >= rosterCount) return false;

    Serial.printf("[Roster] Removing '%s' at index %d\n", rosterNames[index], index);
    for (int i = index; i < rosterCount - 1; i++) {
        memcpy(rosterNames[i], rosterNames[i + 1], PARTY_NAME_LEN);
    }
    rosterCount--;

    rosterSaveAll();
    return true;
}
