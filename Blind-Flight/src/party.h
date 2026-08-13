#pragma once

// ============================================================
// Blind Flight — Flight Club (multi-participant sequential flights)
// ============================================================
// Spec: docs/specs/flight_club_spec.md
//
// Internal identifiers stay neutral (party / PARTY_); "Flight Club"
// is the display name only.
//
// Every other mode runs one flight for whoever is standing at the
// device. Flight Club pours the SAME bottles for each of N
// participants, in an independently randomized glass order per
// participant, holds every result back, and reveals a cumulative
// group result once everyone has submitted.
//
// The blind property is structural rather than enforced by secrecy:
// participants know which bottles are in play, but nobody — including
// the host who is pouring — knows which bottle landed in which glass,
// and because each participant's flight is randomized separately,
// comparing notes across the room leaks nothing. That is what lets
// the host play too.
//
// PHASE 1 (this build): engine + host-proxy rank entry, rank scoring
// only, fully operable from the encoder with no phone connected.
// Phase 2 adds the guess variants; Phase 3 adds participant phones,
// which needs role-filtered state documents in wifi_portal.cpp.
// ============================================================

#include <Arduino.h>
#include "ui.h"
#include "config.h"

#define MAX_PARTY_PLAYERS   8
#define PARTY_NAME_LEN      13   // 12 display chars + null, matches H2H_NAME_LEN

// --- Scoring variant, chosen once at setup ---
// One mode with three scoring settings, not three modes: the pour
// engine is identical and only the input round and aggregate differ.
enum PartyScoring {
    PARTY_SCORE_RANK,       // rank glasses best -> worst; mean rank per bottle
    PARTY_SCORE_GUESS,      // Phase 2
    PARTY_SCORE_BOTH        // Phase 2
};

enum PartyPhase {
    PARTY_RESUME,       // a saved event was found — resume or discard
    PARTY_SETUP,        // how many participants
    PARTY_COUNT,        // glass count 2-4, fixed for the whole event
    PARTY_BOTTLES,      // enter that many bottles, once
    PARTY_NAME,         // name the participant about to taste
    PARTY_POURING,      // randomize -> name a bottle -> pour -> confirm, xN
    PARTY_COLLECT,      // "lift glass k into tray slot k"
    PARTY_COLLECTING,   // all flights poured; host menu
    PARTY_ENTRY,        // host-proxy input round for one participant
    PARTY_REVEAL,       // aggregate reveal, worst -> best
    PARTY_DONE
};

// ~40 bytes each; 8 players is well under 1 KB, so this is a non-issue
// for RAM and a comfortable NVS blob.
struct PartyPlayer {
    char     name[PARTY_NAME_LEN];
    // glass (0-based) -> bottle index. Written during this participant's
    // pour loop; it is the per-flight answer key that scoring resolves
    // against, which is why it is stored per player and not per session.
    uint8_t  bottleForGlass[NUM_GLASSES];
    uint8_t  rankOrder[NUM_GLASSES];     // rankOrder[slot] = glass# (1-based), 0 = unset
    int8_t   guessForGlass[NUM_GLASSES]; // glass (0-based) -> guessed bottle, -1 = unset (Phase 2)
    uint32_t token;                      // reconnect identity (Phase 3), 0 = never connected
    uint8_t  clientNum;                  // current socket (Phase 3), 0xFF = not connected
    bool     poured;
    bool     rankDone;
    bool     guessDone;
};

// --- The Flight Club screen (screen build only) ---
extern const Screen screenParty;

// ============================================================
// State getters
// ============================================================

// Flight Club owns the disc while this is true. ui.cpp's bare wake-press
// homing guard consults it so a wake-up tap cannot spin a carousel that
// has a full set of glasses loaded on it.
bool         partyIsActive();

PartyPhase   partyGetPhase();
int          partyGetGlassCount();
int          partyGetPlayerCount();
int          partyGetCurrentPlayer();
const char*  partyGetPlayerName(int idx);
int          partyGetBottleCount();
const char*  partyGetBottleName(int idx);

// Abort the event and clear its saved state.
void         partyAbort();
