#pragma once

// ============================================================
// Blind Flight — Saved Participant Roster
// ============================================================
// Names of people who have played before, so a repeat guest is a
// single click instead of 20-30 seconds of encoder text entry.
//
// This exists because Flight Club is the one mode where name entry
// happens N times per event. Eight participants typed character-by-
// character is several minutes of setup before any whiskey is poured
// — the difference between "works without a phone" and "is pleasant
// without a phone".
//
// Modelled on favorites.cpp: same flat NVS layout, same compaction
// on remove. Deliberately not merged with it — favorites hold bottle
// names at MAX_GLASS_NAME, these hold people at PARTY_NAME_LEN, and
// a shared store would have to widen one of them for no reason.
// ============================================================

#include <Arduino.h>

#define ROSTER_MAX_COUNT 20

void        rosterInit();
uint8_t     rosterGetCount();
const char* rosterGetName(int index);
bool        rosterContains(const char* name);

// Adds the name if it is not already present. Returns true only when a
// new entry was actually stored, so callers can stay quiet about the
// "played last time too" case rather than reporting a spurious failure.
bool        rosterAdd(const char* name);
bool        rosterRemove(int index);
