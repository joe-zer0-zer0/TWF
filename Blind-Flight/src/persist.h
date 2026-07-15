#pragma once

#include <Arduino.h>
#include "game.h"

void persistInit();
bool persistHasSession();

void persistSaveGame(GameMode mode, GameState state, const GameSession& session);
bool persistLoadGame(GameMode& mode, GameState& state, int& glassCount, GameSession& session);
void persistClearSession();

const char* persistGetModeName();
int         persistGetPourCount();
int         persistGetGlassCount();
