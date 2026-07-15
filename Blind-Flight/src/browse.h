#pragma once

// ============================================================
// Blind Flight — Browse Screen (Session 12, updated Session 21)
// ============================================================
// Three-level hierarchical browser for the whiskey library:
//   Level 0: Whiskey Types
//   Level 1: Distilleries within selected type
//   Level 2: Products within selected distillery
//
// At any level, the user can:
//   - Encoder click: drill deeper (or select at product level)
//   - Right button (USE): accept current highlighted name
//   - Left button (BACK): go up one level (or exit at top)
//
// After selection, the screen pops and the selected name is
// available via browseGetResult(). A nullptr result means
// the user backed out without selecting.
//
// Session 21: browseGetEntry() returns a pointer to the
// LibraryEntry when the user selected at product level.
// Returns nullptr for type/distillery/manual/favorites.
//
// Usage:
//   browseReset();                 // clear previous result
//   uiPushScreen(&screenBrowse);   // launch the browser
//   // ... later, after it pops back:
//   const char* name = browseGetResult();
//   const LibraryEntry* entry = browseGetEntry();  // may be nullptr
// ============================================================

#include "ui.h"
#include "categories.h"

// --- The browse screen ---
extern const Screen screenBrowse;

// Reset the browse state (call before pushing)
void browseReset();

// Get the selected name after the screen pops.
// Returns nullptr if the user cancelled/backed out.
const char* browseGetResult();

// Get the LibraryEntry for the selection (Session 21).
// Non-null only when user selected at product level from the library.
// nullptr for type-level, distillery-level, manual entry, or favorites.
const LibraryEntry* browseGetEntry();
