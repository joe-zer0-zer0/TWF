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

// The magic byte and version only prove the blob was written by this
// firmware, not that its contents are sane — a partial NVS write or a
// snapshot saved by a build with different limits passes both checks.
// Everything here is used as an array index or a switch selector
// downstream; `pourCount` in particular indexes pourOrder[NUM_GLASSES] in
// runPourCycle(). Validate once, on load, so every reader is covered
// rather than just persistLoadGame().
static bool snapshotIsSane(const SessionSnapshot& s) {
    if (s.mode > GAME_MODE_H2H)     return false;
    if (s.savedState > GAME_DONE)    return false;
    if (s.glassCount < 1 || s.glassCount > NUM_GLASSES) return false;
    if (s.pourCount > s.glassCount) return false;

    for (int i = 0; i < NUM_GLASSES; i++) {
        // pourOrder holds 1-based glass numbers; 0 means "not yet set".
        if (s.pourOrder[i] > NUM_GLASSES) return false;
        if (s.rankOrder[i] > NUM_GLASSES) return false;
        // guessForGlass is a 0-based pour index, or -1 for no guess.
        if (s.guessForGlass[i] < -1 || s.guessForGlass[i] >= NUM_GLASSES) return false;
        // A name that lost its terminator would run off the end of the row.
        if (memchr(s.glassName[i], '\0', MAX_GLASS_NAME) == nullptr) return false;
    }
    return true;
}

void persistInit() {
    prefs.begin(NVS_NS, true);
    size_t len = prefs.getBytesLength("snap");
    if (len == sizeof(SessionSnapshot)) {
        prefs.getBytes("snap", &cachedSnap, sizeof(cachedSnap));
        if (cachedSnap.magic == PERSIST_MAGIC && cachedSnap.version == PERSIST_VERSION) {
            if (snapshotIsSane(cachedSnap)) {
                hasValid = true;
                Serial.printf("[Persist] Found session: mode=%d pours=%d/%d\n",
                              cachedSnap.mode, cachedSnap.pourCount, cachedSnap.glassCount);
            } else {
                Serial.println("[Persist] Saved session failed validation — discarding");
            }
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
        case GAME_MODE_GUESS_RANK: return "Guess + Ranked";
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
