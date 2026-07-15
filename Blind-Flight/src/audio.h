#pragma once

// ============================================================
// Blind Flight — Audio Module
// ============================================================
// Non-blocking tone sequencer for passive buzzer.
// Each sound effect is a Note array terminated by {0, 0}.
// Call audioUpdate() every loop iteration.
//
// Session 17 additions:
//   - audioSetMute(bool) — globally mute/unmute all tones
//   - audioSetVolume(uint8_t) — set volume level 0–4
//   - Reads initial mute/volume state from settings module
// ============================================================

#include <Arduino.h>

// A single note in a tone sequence
struct Note {
    uint16_t freq;      // Hz (0 = silence/rest)
    uint16_t duration;  // ms (0 in both fields = end marker)
};

// --- Tone library (defined in audio.cpp) ---
extern const Note TONE_CLICK[];
extern const Note TONE_SELECT[];
extern const Note TONE_CONFIRM[];
extern const Note TONE_ERROR[];
extern const Note TONE_ARRIVE[];
extern const Note TONE_HOME_FOUND[];

// --- API ---
void audioInit();
void audioUpdate();                 // Call every loop — advances sequencer
void audioPlayTone(const Note* sequence);   // Start a tone (replaces current)
void audioStopTone();               // Silence immediately

// --- Volume / Mute (Session 17) ---
void audioSetMute(bool muted);      // Mute: audioPlayTone becomes no-op
void audioSetVolume(uint8_t level); // 0–4 — adjusts LEDC duty after each tone
uint8_t audioGetVolume();
