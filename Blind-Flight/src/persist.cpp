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

// Defined with the rest of the party code at the bottom of this file;
// persistInit() has to reach it from up here.
static void partyLoadCache();

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

    partyLoadCache();
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

// ============================================================
// Flight Club event state
// ============================================================
// Its own namespace and its own version byte. PERSIST_VERSION stays at
// 1 on purpose: SessionSnapshot is unchanged, so a v1 blob written by
// an older build is still exactly as valid as it was, and bumping the
// number would throw away a resumable single flight on upgrade for no
// benefit. (The spec called for PERSIST_VERSION -> 2; this is the
// deviation, and the reason for it.)

#define PARTY_MAGIC     0xBF
#define PARTY_VERSION   1

static const char* NVS_NS_PARTY = "bfparty";

// clientNum is deliberately absent — a live WebSocket client number is
// meaningless after a reboot. token is kept because Phase 3 uses it to
// re-identify a returning phone, and surviving a brownout is the whole
// point of persisting the event at all.
#pragma pack(push, 1)
struct PartyPlayerSnap {
    char     name[PARTY_NAME_LEN];
    uint8_t  bottleForGlass[NUM_GLASSES];
    uint8_t  rankOrder[NUM_GLASSES];
    int8_t   guessForGlass[NUM_GLASSES];
    uint32_t token;
    uint8_t  poured;
    uint8_t  rankDone;
    uint8_t  guessDone;
};

struct PartySnapshot {
    uint8_t magic;
    uint8_t version;
    uint8_t phase;
    uint8_t scoring;
    uint8_t glassCount;
    uint8_t playerCount;
    uint8_t currentPlayer;
    char    bottleName[NUM_GLASSES][MAX_GLASS_NAME];
    PartyPlayerSnap players[MAX_PARTY_PLAYERS];
};
#pragma pack(pop)

static Preferences partyPrefs;
static PartySnapshot cachedParty;
static bool hasParty = false;

// Same reasoning as snapshotIsSane(): magic and version only prove which
// firmware wrote the blob. Everything below is used as an array index by
// the pour loop or the aggregate, so it is validated once here rather
// than at each reader.
static bool partySnapshotIsSane(const PartySnapshot& p) {
    if (p.phase > PARTY_DONE) return false;
    if (p.scoring > PARTY_SCORE_BOTH) return false;
    if (p.glassCount < 2 || p.glassCount > NUM_GLASSES) return false;
    if (p.playerCount < 1 || p.playerCount > MAX_PARTY_PLAYERS) return false;
    if (p.currentPlayer > p.playerCount) return false;

    for (int i = 0; i < NUM_GLASSES; i++) {
        if (memchr(p.bottleName[i], '\0', MAX_GLASS_NAME) == nullptr) return false;
    }

    for (int i = 0; i < p.playerCount; i++) {
        const PartyPlayerSnap& s = p.players[i];
        if (memchr(s.name, '\0', PARTY_NAME_LEN) == nullptr) return false;
        for (int g = 0; g < NUM_GLASSES; g++) {
            if (s.bottleForGlass[g] >= NUM_GLASSES) return false;
            if (s.rankOrder[g] > NUM_GLASSES) return false;   // 0 = unset
            if (s.guessForGlass[g] < -1 || s.guessForGlass[g] >= NUM_GLASSES) return false;
        }
    }
    return true;
}

static void partyLoadCache() {
    partyPrefs.begin(NVS_NS_PARTY, true);
    size_t len = partyPrefs.getBytesLength("party");
    if (len == sizeof(PartySnapshot)) {
        partyPrefs.getBytes("party", &cachedParty, sizeof(cachedParty));
        if (cachedParty.magic == PARTY_MAGIC && cachedParty.version == PARTY_VERSION) {
            if (partySnapshotIsSane(cachedParty)) {
                hasParty = true;
            } else {
                Serial.println("[Persist] Saved party failed validation — discarding");
            }
        }
    }
    partyPrefs.end();

    if (hasParty) {
        Serial.printf("[Persist] Found party: %d participants, phase=%d\n",
                      cachedParty.playerCount, cachedParty.phase);
    }
}

bool persistHasParty() {
    return hasParty;
}

void persistSaveParty(uint8_t phase, uint8_t scoring, uint8_t glassCount,
                      uint8_t playerCount, uint8_t currentPlayer,
                      const char bottleName[NUM_GLASSES][MAX_GLASS_NAME],
                      const PartyPlayer* players) {
    PartySnapshot p;
    memset(&p, 0, sizeof(p));
    p.magic         = PARTY_MAGIC;
    p.version       = PARTY_VERSION;
    p.phase         = phase;
    p.scoring       = scoring;
    p.glassCount    = glassCount;
    p.playerCount   = playerCount;
    p.currentPlayer = currentPlayer;

    for (int i = 0; i < NUM_GLASSES; i++) {
        memcpy(p.bottleName[i], bottleName[i], MAX_GLASS_NAME);
        p.bottleName[i][MAX_GLASS_NAME - 1] = '\0';
    }

    for (int i = 0; i < MAX_PARTY_PLAYERS; i++) {
        PartyPlayerSnap& s = p.players[i];
        const PartyPlayer& src = players[i];
        memcpy(s.name, src.name, PARTY_NAME_LEN);
        s.name[PARTY_NAME_LEN - 1] = '\0';
        for (int g = 0; g < NUM_GLASSES; g++) {
            s.bottleForGlass[g] = src.bottleForGlass[g];
            s.rankOrder[g]      = src.rankOrder[g];
            s.guessForGlass[g]  = src.guessForGlass[g];
        }
        s.token     = src.token;
        s.poured    = src.poured    ? 1 : 0;
        s.rankDone  = src.rankDone  ? 1 : 0;
        s.guessDone = src.guessDone ? 1 : 0;
    }

    partyPrefs.begin(NVS_NS_PARTY, false);
    partyPrefs.putBytes("party", &p, sizeof(p));
    partyPrefs.end();

    cachedParty = p;
    hasParty = true;
}

bool persistLoadParty(uint8_t& phase, uint8_t& scoring, uint8_t& glassCount,
                      uint8_t& playerCount, uint8_t& currentPlayer,
                      char bottleName[NUM_GLASSES][MAX_GLASS_NAME],
                      PartyPlayer* players) {
    if (!hasParty) return false;

    phase         = cachedParty.phase;
    scoring       = cachedParty.scoring;
    glassCount    = cachedParty.glassCount;
    playerCount   = cachedParty.playerCount;
    currentPlayer = cachedParty.currentPlayer;

    for (int i = 0; i < NUM_GLASSES; i++) {
        memcpy(bottleName[i], cachedParty.bottleName[i], MAX_GLASS_NAME);
    }

    for (int i = 0; i < MAX_PARTY_PLAYERS; i++) {
        const PartyPlayerSnap& s = cachedParty.players[i];
        PartyPlayer& dst = players[i];
        memcpy(dst.name, s.name, PARTY_NAME_LEN);
        for (int g = 0; g < NUM_GLASSES; g++) {
            dst.bottleForGlass[g] = s.bottleForGlass[g];
            dst.rankOrder[g]      = s.rankOrder[g];
            dst.guessForGlass[g]  = s.guessForGlass[g];
        }
        dst.token     = s.token;
        dst.clientNum = 0xFF;         // no socket survives a reboot
        dst.poured    = s.poured != 0;
        dst.rankDone  = s.rankDone != 0;
        dst.guessDone = s.guessDone != 0;
    }

    return true;
}

void persistClearParty() {
    if (!hasParty) return;
    partyPrefs.begin(NVS_NS_PARTY, false);
    partyPrefs.remove("party");
    partyPrefs.end();
    hasParty = false;
    Serial.println("[Persist] Party cleared");
}

int persistGetPartyPlayerCount() {
    return hasParty ? cachedParty.playerCount : 0;
}

int persistGetPartyPouredCount() {
    if (!hasParty) return 0;
    int n = 0;
    for (int i = 0; i < cachedParty.playerCount; i++) {
        if (cachedParty.players[i].poured) n++;
    }
    return n;
}
