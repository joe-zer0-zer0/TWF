#pragma once

// ============================================================
// Blind Flight — Game Module (Session 10, updated Session 16)
// ============================================================
// Session 12 changes:
//   - Added GAME_MODE_NAMED for Browse Library flow
//   - Added glassName[] to GameSession for storing selected
//     whiskey names per glass
//   - Added GAME_NAMING sub-state: pushes browse screen,
//     then picks up the result for the pour cycle
//   - gameSetMode() to configure before pushing screenGame
//   - Reveal screen shows names when available
//
// Session 13 changes:
//   - Added GAME_MODE_GUESS for Best Guess mode
//   - Added GAME_GUESSING, GAME_GUESS_SCORE, GAME_GUESS_DETAIL
//     sub-states for the guessing round
//   - Added guessForGlass[] and guessIndex to GameSession
//   - Guessing round: match whiskey names to glasses before
//     reveal, with pool management and back-tracking
//
// Session 16 changes:
//   - Added state getter functions for Wi-Fi phone interface
//   - Added phone action handlers (name submit, guess select,
//     game start from phone)
//   - Phone and physical controls work simultaneously
// ============================================================

#include <Arduino.h>
#include "ui.h"
#include "config.h"
#include "categories.h"

// --- Game modes ---
enum GameMode {
    GAME_MODE_BASIC,    // Numbered glasses only (Session 10)
    GAME_MODE_NAMED,    // Browse library for names (Session 12)
    GAME_MODE_GUESS,    // Browse library + guessing round (Session 13)
    GAME_MODE_RANK,     // Browse library + ranking round (Session 20)
    GAME_MODE_DUPLICATE,// Twin Pour: 3 bottles, one duplicated (Session 22)
    GAME_MODE_DECOY,    // Find the Ringer: 2 bottles, 3+1 split (Session 22)
    GAME_MODE_H2H       // Head-to-Head multiplayer (Session 23)
};

// --- Game sub-states ---
enum GameState {
    GAME_COUNT_SELECT,  // Choosing how many glasses (2–4)
    GAME_NAMING,        // Named mode: browse screen is active (Session 12)
    GAME_BOTTLE_SELECT, // Duplicate/Decoy: selecting bottles (Session 22)
    GAME_POURING,       // Interactive: waiting for user to pour and confirm
    GAME_TASTING,       // Interactive: all poured, user tastes before reveal
    GAME_RATING,        // Interactive: user rating each glass 1-5 stars (Session 21)
    GAME_GUESSING,      // Interactive: user guessing one glass at a time (Session 13)
    GAME_GUESS_SCORE,   // Interactive: shows total correct out of total poured (Session 13)
    GAME_GUESS_DETAIL,  // Interactive: shows checkmark/X per glass (Session 13)
    GAME_RANKING,       // Interactive: user ranking glasses by preference (Session 20)
    GAME_CHALLENGE_GUESS,  // Duplicate/Decoy: guess pair or ringer (Session 22)
    GAME_CHALLENGE_RESULT, // Duplicate/Decoy: correct/incorrect (Session 22)
    GAME_REVEAL,        // Interactive: showing one glass reveal at a time
    GAME_DONE           // Interactive: flight complete, new/exit options
};

// --- Session data ---
#define MAX_GLASS_NAME  22      // Max characters for a glass name + null

struct GameSession {
    int  pourOrder[NUM_GLASSES];                // pourOrder[i] = glass# for pour i (1-based)
    bool glassUsed[NUM_GLASSES];                // glassUsed[i] = true if glass i+1 has been poured
    char glassName[NUM_GLASSES][MAX_GLASS_NAME]; // name per pour (Session 12)
    int  pourCount;                             // number of pours completed (0–4)
    int  currentGlass;                          // glass currently selected (1–4), 0 if none
    int  revealIndex;                           // which reveal we're showing (0-based)
    int  glassCount;                            // 2–4 glasses this flight

    // --- Guessing round (Session 13) ---
    int  guessForGlass[NUM_GLASSES];            // pour index guess per revealMap position, -1 = unset
    int  guessIndex;                            // current revealMap position being guessed (0-based)

    // --- Ranking round (Session 20) ---
    int  rankOrder[NUM_GLASSES];                // rankOrder[slot] = glass# picked for that rank, 0 = unset
    int  rankIndex;                             // current rank slot being picked (0 = 1st place)

    // --- Library metadata & rating (Session 21) ---
    const LibraryEntry* glassEntry[NUM_GLASSES]; // library entry per pour (nullptr for manual/basic)
    uint8_t glassRating[NUM_GLASSES];            // star rating per pour (0 = unrated, 1-5)
    int     ratingIndex;                         // current glass being rated (0-based into revealMap)

    // --- Duplicate/Decoy (Session 22) ---
    #define MAX_BOTTLES 3
    char    bottleName[MAX_BOTTLES][MAX_GLASS_NAME]; // bottle names (3 for Duplicate, 2 for Decoy)
    int     bottleCount;                 // bottles selected so far (0–3)
    int     bottleForGlass[NUM_GLASSES]; // bottleForGlass[glass_0based] = bottle index (0-based)
    int     challengePourOrder[NUM_GLASSES]; // pour sequence: index into bottleName[]
    int     duplicateIdx;                // which bottle is duplicated (Duplicate mode only)
    int     challengeGuess[2];           // user's guess: glass numbers (1-based), [0] only for Decoy
    int     challengeGuessCount;         // 0, 1, or 2 selections made
    bool    challengeCorrect;            // result of the guess
};

// --- Game timing constants ---
#define GAME_SELECT_PAUSE_MS    1200
#define GAME_SELECT_REVEAL_MS   600
#define GAME_SPIN_SETTLE_MS     400

// --- Set mode before pushing screenGame ---
// Call this before uiPushScreenT(&screenGame, ...).
// Defaults to GAME_MODE_BASIC if not called.
void gameSetMode(GameMode mode);

// --- The game screen ---
extern const Screen screenGame;

// ============================================================
// State getters — Wi-Fi phone interface (Session 16)
// ============================================================
// Read-only access to game state for the Wi-Fi portal module
// to build state snapshots for the phone UI.

bool      gameIsActive();
void      gameAbort();
int       gameGetGlassCount();
GameState gameGetState();
GameMode  gameGetMode();
int       gameGetPourCount();
bool      gameGetGlassUsed(int glassIdx);       // 0-based glass index
const char* gameGetGlassName(int pourIdx);       // 0-based pour index

// True while a blocking motor spin (runPourCycle/runFinalSpin) is in
// progress. The main loop — and with it the Wi-Fi portal's WebSocket,
// HTTP, and DNS servicing — is blocked for the duration of the spin,
// so this flag is broadcast to phones *before* the blocking call
// starts (see wifiPortalBroadcastNow()) rather than discovered by
// polling gameGetState() afterward.
bool      gameIsSpinning();

// Reveal info (valid when state >= GAME_REVEAL or GAME_GUESSING)
int       gameGetRevealCount();
int       gameGetRevealIndex();
int       gameGetRevealGlass(int revealIdx);     // glass# at reveal position
int       gameGetRevealPourIdx(int revealIdx);   // pour index at reveal position

// Rating info (valid when state == GAME_RATING, Session 21)
uint8_t   gameGetGlassRating(int pourIdx);       // 0-based pour index
const LibraryEntry* gameGetGlassEntry(int pourIdx); // library entry or nullptr

// Guess info (valid when state == GAME_GUESSING)
int       gameGetGuessGlass();                   // glass# being guessed
int       gameGetGuessPoolCount();
const char* gameGetGuessPoolName(int poolIdx);
int       gameGetGuessCorrect();                 // count of correct guesses
bool      gameGetGuessCorrectAt(int revealIdx);  // is guess correct at this reveal position?

// ============================================================
// Phone action handlers (Session 16)
// ============================================================
// Called from the Wi-Fi portal WebSocket handler. These set
// deferred flags that gameDraw() processes on the next cycle,
// ensuring all game state changes happen in the main loop
// context alongside normal game operations.

// Submit a whiskey name from the phone keyboard.
// Only effective when state == GAME_NAMING.
void gamePhoneSubmitName(const char* name);

// Select a guess from the pool by pool index.
// Only effective when state == GAME_GUESSING.
void gamePhoneGuessSelect(int poolIndex);

// Start a new flight from the phone.
// Only effective when no game is active.
void gameStartFromPhone(GameMode mode);

// Resume a saved session (called from persist module on boot)
void gameResumeSession(GameMode mode, GameState resumeState,
                       int glassCount, const GameSession& saved);
