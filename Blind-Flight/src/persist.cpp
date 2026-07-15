#include "persist.h"
#include "config.h"
#include <Preferences.h>

#define PERSIST_MAGIC   0xBF
#define PERSIST_VERSION 1

#pragma pack(push, 1)
struct SessionSnapshot {
    uint8_t  magic;
    uint8_t  version;
    uint8_t  mode;
    uint8_t  glassCount;
    uint8_t  pourCount;
    uint8_t  savedState;
    uint8_t  pourOrder[NUM_GLASSES];
    uint8_t  glassUsed[NUM_GLASSES];
    char     glassName[NUM_GLASSES][MAX_GLASS_NAME];
    int8_t   guessForGlass[NUM_GLASSES];
    uint8_t  rankOrder[NUM_GLASSES];
};
#pragma pack(pop)

static Preferences prefs;
static const char* NVS_NS = "bfsess";
static bool hasValid = false;

static SessionSnapshot cachedSnap;

void persistInit() {
    prefs.begin(NVS_NS, true);
    size_t len = prefs.getBytesLength("snap");
    if (len == sizeof(SessionSnapshot)) {
        prefs.getBytes("snap", &cachedSnap, sizeof(cachedSnap));
        if (cachedSnap.magic == PERSIST_MAGIC && cachedSnap.version == PERSIST_VERSION) {
            hasValid = true;
            Serial.printf("[Persist] Found session: mode=%d pours=%d/%d\n",
                          cachedSnap.mode, cachedSnap.pourCount, cachedSnap.glassCount);
        }
    }
    prefs.end();

    if (!hasValid) {
        Serial.println("[Persist] No saved session");
    }
}

bool persistHasSession() {
    return hasValid;
}

void persistSaveGame(GameMode mode, GameState state, const GameSession& session) {
    SessionSnapshot snap;
    snap.magic      = PERSIST_MAGIC;
    snap.version    = PERSIST_VERSION;
    snap.mode       = (uint8_t)mode;
    snap.glassCount = (uint8_t)session.glassCount;
    snap.pourCount  = (uint8_t)session.pourCount;
    snap.savedState = (uint8_t)state;

    for (int i = 0; i < NUM_GLASSES; i++) {
        snap.pourOrder[i]     = (uint8_t)session.pourOrder[i];
        snap.glassUsed[i]     = session.glassUsed[i] ? 1 : 0;
        memcpy(snap.glassName[i], session.glassName[i], MAX_GLASS_NAME);
        snap.guessForGlass[i] = (int8_t)session.guessForGlass[i];
        snap.rankOrder[i]     = (uint8_t)session.rankOrder[i];
    }

    prefs.begin(NVS_NS, false);
    prefs.putBytes("snap", &snap, sizeof(snap));
    prefs.end();

    cachedSnap = snap;
    hasValid = true;
}

bool persistLoadGame(GameMode& mode, GameState& state, int& glassCount, GameSession& session) {
    if (!hasValid) return false;

    mode       = (GameMode)cachedSnap.mode;
    state      = (GameState)cachedSnap.savedState;
    glassCount = cachedSnap.glassCount;

    // Challenge modes can't be restored (bottle mapping not in snapshot)
    if (mode == GAME_MODE_DUPLICATE || mode == GAME_MODE_DECOY) {
        hasValid = false;
        return false;
    }

    session.pourCount    = cachedSnap.pourCount;
    session.currentGlass = 0;
    session.revealIndex  = 0;
    session.guessIndex   = 0;
    session.glassCount   = cachedSnap.glassCount;
    session.rankIndex    = 0;

    for (int i = 0; i < NUM_GLASSES; i++) {
        session.pourOrder[i]     = cachedSnap.pourOrder[i];
        session.glassUsed[i]     = cachedSnap.glassUsed[i] != 0;
        memcpy(session.glassName[i], cachedSnap.glassName[i], MAX_GLASS_NAME);
        session.guessForGlass[i] = cachedSnap.guessForGlass[i];
        session.rankOrder[i]     = cachedSnap.rankOrder[i];
    }

    return true;
}

void persistClearSession() {
    if (!hasValid) return;
    prefs.begin(NVS_NS, false);
    prefs.remove("snap");
    prefs.end();
    hasValid = false;
    Serial.println("[Persist] Session cleared");
}

const char* persistGetModeName() {
    if (!hasValid) return "";
    switch ((GameMode)cachedSnap.mode) {
        case GAME_MODE_BASIC:     return "Basic Flight";
        case GAME_MODE_NAMED:     return "Full Flight";
        case GAME_MODE_GUESS:     return "Best Guess";
        case GAME_MODE_RANK:      return "Ranked Flight";
        case GAME_MODE_DUPLICATE: return "Twin Pour";
        case GAME_MODE_DECOY:     return "Find the Ringer";
        default:                  return "Flight";
    }
}

int persistGetPourCount() {
    return hasValid ? cachedSnap.pourCount : 0;
}

int persistGetGlassCount() {
    return hasValid ? cachedSnap.glassCount : 0;
}
