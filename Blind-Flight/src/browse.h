#pragma once

// ============================================================
// Blind Flight — Browse Screen (Session 12)
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
// Usage:
//   browseReset();                 // clear previous result
//   uiPushScreen(&screenBrowse);   // launch the browser
//   // ... later, after it pops back:
//   const char* name = browseGetResult();
// ============================================================

#include "ui.h"

// --- The browse screen ---
extern const Screen screenBrowse;

// Reset the browse state (call before pushing)
void browseReset();

// Get the selected name after the screen pops.
// Returns nullptr if the user cancelled/backed out.
const char* browseGetResult();
