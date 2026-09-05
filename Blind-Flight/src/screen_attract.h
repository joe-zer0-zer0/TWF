#pragma once

// ============================================================
// Blind Flight — Attract Mode
// ============================================================
// Pre-programmed demo loop for promotional video filming and
// live event display. Cycles through all major features with
// real motor movement and simulated screen content.
//
// Entry: Diagnostics menu → "Attract"
// Exit:  Long-press encoder at any time
// Loop:  Repeats continuously until exited
//
// Phone integration: phone input is blocked (read-only display)
// during the attract sequence. wifiPortalService() is called
// throughout to keep the connection alive.
// ============================================================

#include "ui.h"

extern const Screen screenAttract;

// True while the attract demo loop is running.
// Used by wifi_portal.cpp to block phone input commands.
bool attractIsRunning();
