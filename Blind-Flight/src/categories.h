#pragma once

// ============================================================
// Blind Flight — Whiskey Category Data (Session 12, updated Session 21)
// ============================================================
// Three-level hierarchy: Type → Distillery → Product
// All strings stored as const in flash (ESP32 stores const
// string literals in flash automatically).
//
// Session 21: Products are now LibraryEntry structs with
// optional metadata (proof, price tier, age statement).
//
// The browse UI allows the user to "USE THIS" at any level:
//   - Type level: glass labeled "Bourbon"
//   - Distillery level: glass labeled "Buffalo Trace"
//   - Product level: glass labeled "Eagle Rare 10yr"
// ============================================================

#include <Arduino.h>

// --- Data structures ---

struct LibraryEntry {
    const char* name;
    uint8_t proof;      // 0 = unknown
    uint8_t priceTier;  // 0 = unknown, 1-4 = $..$$$$
    uint8_t ageYears;   // 0 = NAS/unknown
};

struct WhiskeyDistillery {
    const char*          name;
    const LibraryEntry*  products;
    int                  productCount;
};

struct WhiskeyType {
    const char*              name;
    const WhiskeyDistillery* distilleries;
    int                      distilleryCount;
};

// --- Top-level data (defined in categories.cpp) ---
extern const WhiskeyType  WHISKEY_TYPES[];
extern const int          WHISKEY_TYPE_COUNT;
