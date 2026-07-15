#pragma once

// ============================================================
// Blind Flight — Diagnostics Module
// ============================================================
// NVS-backed flight log and usage counters. Stores a ring
// buffer of recent flight records (glass order + mode) and
// per-mode flight counters.
//
// Storage layout (NVS namespace "bfdiag"):
//   "totalFlts" : uint16  — total flights completed
//   "cntBasic"  : uint16  — Basic mode count
//   "cntNamed"  : uint16  — Named mode count
//   "cntGuess"  : uint16  — Best Guess mode count
//   "cntRank"   : uint16  — Ranked Flight mode count
//   "logHead"   : uint8   — ring buffer write index
//   "log00".."log31" : uint8[5] — flight records (mode + 4 glasses)
// ============================================================

#include <Arduino.h>

#define DIAG_LOG_SIZE  32   // max flight records in ring buffer

struct FlightRecord {
    uint8_t mode;           // GameMode enum value
    uint8_t pourCount;      // glasses actually poured (1-4)
    uint8_t glassOrder[4];  // glass numbers in pour order (1-4, 0 = unused)
};

void diagInit();

// Log a completed flight. Call from game.cpp when entering GAME_DONE.
void diagLogFlight(uint8_t mode, int pourCount, const int* pourOrder);

// --- Counters ---
uint16_t diagGetTotalFlights();
uint16_t diagGetModeCount(uint8_t mode);  // 0=Basic, 1=Named, 2=Guess, 3=Rank

// --- Flight log ---
int  diagGetLogCount();                    // number of stored records (0..DIAG_LOG_SIZE)
bool diagGetRecord(int index, FlightRecord* out);  // 0 = most recent

// --- Clear all data ---
void diagClearAll();
