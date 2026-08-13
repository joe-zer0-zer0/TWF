#pragma once

#include <Arduino.h>
#include "game.h"
#include "party.h"

void persistInit();
bool persistHasSession();

void persistSaveGame(GameMode mode, GameState state, const GameSession& session);
bool persistLoadGame(GameMode& mode, GameState& state, int& glassCount, GameSession& session);
void persistClearSession();

const char* persistGetModeName();
int         persistGetPourCount();
int         persistGetGlassCount();

// ============================================================
// Flight Club event state — a separate blob, deliberately
// ============================================================
// A party is a different shape from a single flight and, uniquely, it
// is LONG: 30-60 minutes of real time with several completed flights
// already banked. Widening SessionSnapshot to cover it would put the
// whole event at the mercy of the single-flight save path, so this
// gets its own NVS namespace, its own version byte, and its own
// validation. The existing snapshot struct is untouched.

bool persistHasParty();

void persistSaveParty(uint8_t phase, uint8_t scoring, uint8_t glassCount,
                      uint8_t playerCount, uint8_t currentPlayer,
                      const char bottleName[NUM_GLASSES][MAX_GLASS_NAME],
                      const PartyPlayer* players);

bool persistLoadParty(uint8_t& phase, uint8_t& scoring, uint8_t& glassCount,
                      uint8_t& playerCount, uint8_t& currentPlayer,
                      char bottleName[NUM_GLASSES][MAX_GLASS_NAME],
                      PartyPlayer* players);

void persistClearParty();

// Participant count and how many of them have finished their flight,
// for the "resume?" prompt — reading it must not require loading the
// whole event.
int persistGetPartyPlayerCount();
int persistGetPartyPouredCount();
