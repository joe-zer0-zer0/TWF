#pragma once

// ============================================================
// Blind Flight — Whiskey Category Data (Session 12)
// ============================================================
// Three-level hierarchy: Type → Distillery → Product
// All strings stored as const in flash (ESP32 stores const
// string literals in flash automatically).
//
// The browse UI allows the user to "USE THIS" at any level:
//   - Type level: glass labeled "Bourbon"
//   - Distillery level: glass labeled "Buffalo Trace"
//   - Product level: glass labeled "Eagle Rare 10yr"
// ============================================================

#include <Arduino.h>

// --- Data structures ---

struct WhiskeyDistillery {
    const char*  name;
    const char** products;      // Array of product name strings
    int          productCount;
};

struct WhiskeyType {
    const char*              name;
    const WhiskeyDistillery* distilleries;
    int                      distilleryCount;
};

// --- Top-level data (defined in categories.cpp) ---
extern const WhiskeyType  WHISKEY_TYPES[];
extern const int          WHISKEY_TYPE_COUNT;
