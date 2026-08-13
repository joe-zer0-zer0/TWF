#pragma once

// ============================================================
// Blind Flight — Head-to-Head Multiplayer Module (Session 23)
// ============================================================
// Multi-player competitive tasting with three sub-modes:
//   2x2        — 2 players, 2 bottles poured twice each
//   Random     — 2-4 players, standard randomized pour
//   Premium    — 2-4 players, one premium bottle + standard
//
// Session A: infrastructure + 2x2 sub-mode.
// Session B: Multiplayer Random sub-mode.
// ============================================================

#include <Arduino.h>
#include "ui.h"
#include "config.h"

// --- Sub-modes ---
enum H2HSubMode {
    H2H_SUB_2X2,
    H2H_SUB_RANDOM,
    H2H_SUB_PREMIUM
};

// --- Game phases ---
enum H2HPhase {
    H2H_MODE_SELECT,       // choosing sub-mode
    H2H_COUNT_SELECT,      // Random/Premium: choosing glass count (2-4)
    H2H_BOTTLE_SELECT,     // selecting bottles via browse library
    H2H_LOBBY,             // waiting for players to join
    H2H_POURING,           // interactive pour confirmation
    H2H_TASTING,           // Random/Premium: tasting time before guessing
    H2H_ASSIGN,            // 2x2: "Take Your Glasses" assignment display
    H2H_WAITING,           // waiting for all players to submit guesses
    H2H_RESULTS,           // showing results on device
    H2H_REVEAL,            // standard per-glass reveal
    H2H_DONE               // flight complete
};

// --- Player data ---
struct H2HPlayer {
    uint8_t clientNum;                  // WebSocket client ID
    char    name[H2H_NAME_LEN];
    bool    connected;
    bool    submitted;
    bool    correct;                    // overall result (Random/Premium)

    // 2x2 fields
    int     guess2x2[2];               // 2x2: pour index guess per assigned glass
    int     assignedGlasses[2];        // 2x2: glass positions (1-based)
    bool    correctPerGlass[2];        // result per assigned glass

    // Multiplayer Random fields
    int     claimedGlass;              // glass number claimed (1-based), 0 = unclaimed
    int     guessBottleIdx;            // bottle index guessed (-1 = unset)

    // Premium Pour fields
    bool    guessPremium;              // true = player thinks they got the premium
    bool    guessedPremium;            // true = player has submitted a premium guess
};

// --- The H2H screen ---
extern const Screen screenH2H;

// ============================================================
// State getters — used by wifi_portal for JSON state building
// ============================================================

bool        h2hIsActive();
H2HPhase    h2hGetPhase();
H2HSubMode  h2hGetSubMode();

// Player registry access
int              h2hGetPlayerCount();
const H2HPlayer* h2hGetPlayer(int idx);
int              h2hGetSubmittedCount();
int              h2hGetRequiredPlayers();

// Bottle info (for phone UI during guessing)
int         h2hGetBottleCount();
const char* h2hGetBottleName(int idx);

// Glass count (for Random/Premium modes)
int         h2hGetGlassCount();

// Actual bottle index in a glass (for results)
int         h2hGetBottleForGlass(int glassNum); // 1-based glass → bottle index

// Glass claim tracking (Multiplayer Random)
int         h2hGetClaimedBy(int glassNum);  // returns player idx or -1

// ============================================================
// Phone action handlers — called from wifi_portal WebSocket
// ============================================================

// Player joins lobby with display name. Returns player index
// (0-based) or -1 if lobby is full.
int  h2hPhoneJoin(uint8_t clientNum, const char* name);

// Player submits a guess. Format varies by sub-mode.
// 2x2: guess is {glass0_bottle, glass1_bottle} (bottle indices)
void h2hPhoneGuess2x2(uint8_t clientNum, int guess0, int guess1);

// Multiplayer Random: player claims a glass, then guesses a bottle
void h2hPhoneClaimGlass(uint8_t clientNum, int glassNum);
void h2hPhoneGuessRandom(uint8_t clientNum, int bottleIdx);

// Premium Pour: player guesses yes/no whether they got the premium
void h2hPhoneGuessPremium(uint8_t clientNum, bool guessPremium);

// Premium bottle info (for phone UI)
int  h2hGetPremiumGlassNum();  // which glass (1-based) has the premium bottle

// WebSocket disconnect — mark player as disconnected
void h2hPhoneDisconnect(uint8_t clientNum);

// ============================================================
// Phone-based H2H creation (Session 25)
// ============================================================

void h2hStartFromPhone(uint8_t clientNum, H2HSubMode mode);
void h2hPhoneSetGlassCount(int count);
void h2hPhoneAddBottle(const char* name);
void h2hPhoneStartGame(uint8_t clientNum);

