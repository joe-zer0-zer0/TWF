#include "favorites.h"
#include "game.h"      // MAX_GLASS_NAME
#include <Preferences.h>

static const char* NVS_NS = "bffav";

static char    favNames[FAV_MAX_COUNT][MAX_GLASS_NAME];
static uint8_t favCount = 0;

static Preferences favPrefs;

static void favSaveAll() {
    favPrefs.begin(NVS_NS, false);
    favPrefs.putUChar("cnt", favCount);
    char key[4];
    for (int i = 0; i < favCount; i++) {
        snprintf(key, sizeof(key), "f%02d", i);
        favPrefs.putString(key, favNames[i]);
    }
    // Clear any keys beyond current count (from prior deletes)
    for (int i = favCount; i < FAV_MAX_COUNT; i++) {
        snprintf(key, sizeof(key), "f%02d", i);
        favPrefs.remove(key);
    }
    favPrefs.end();
}

void favoritesInit() {
    favPrefs.begin(NVS_NS, true);
    favCount = favPrefs.getUChar("cnt", 0);
    if (favCount > FAV_MAX_COUNT) favCount = FAV_MAX_COUNT;

    char key[4];
    for (int i = 0; i < favCount; i++) {
        snprintf(key, sizeof(key), "f%02d", i);
        String val = favPrefs.getString(key, "");
        strncpy(favNames[i], val.c_str(), MAX_GLASS_NAME - 1);
        favNames[i][MAX_GLASS_NAME - 1] = '\0';
    }
    favPrefs.end();

    Serial.printf("[Fav] Loaded %d favorites\n", favCount);
}

uint8_t favoritesGetCount() {
    return favCount;
}

const char* favoritesGetName(int index) {
    if (index < 0 || index >= favCount) return "";
    return favNames[index];
}

bool favoritesContains(const char* name) {
    if (!name || !name[0]) return false;
    for (int i = 0; i < favCount; i++) {
        if (strcasecmp(favNames[i], name) == 0) return true;
    }
    return false;
}

bool favoritesAdd(const char* name) {
    if (!name || !name[0]) return false;
    if (favCount >= FAV_MAX_COUNT) return false;
    if (favoritesContains(name)) return false;

    strncpy(favNames[favCount], name, MAX_GLASS_NAME - 1);
    favNames[favCount][MAX_GLASS_NAME - 1] = '\0';
    favCount++;

    favSaveAll();
    Serial.printf("[Fav] Added '%s' (%d/%d)\n", name, favCount, FAV_MAX_COUNT);
    return true;
}

bool favoritesRemove(int index) {
    if (index < 0 || index >= favCount) return false;

    Serial.printf("[Fav] Removing '%s' at index %d\n", favNames[index], index);

    // Compact: shift entries down
    for (int i = index; i < favCount - 1; i++) {
        memcpy(favNames[i], favNames[i + 1], MAX_GLASS_NAME);
    }
    favCount--;

    favSaveAll();
    return true;
}
