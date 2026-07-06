#include "audio.h"
#include "config.h"
#include "settings.h"
#include "driver/ledc.h"

// ============================================================
// Tone definitions
// ============================================================

// Short tick for encoder rotation
const Note TONE_CLICK[] = {
    {3000, 15},
    {0, 0}
};

// Two-note rising chirp for selection actions
const Note TONE_SELECT[] = {
    {1800, 40},
    {2400, 60},
    {0, 0}
};

// Three-note ascending for confirm/go
const Note TONE_CONFIRM[] = {
    {1200, 60},
    {1600, 60},
    {2200, 80},
    {0, 0}
};

// Low descending buzz for errors
const Note TONE_ERROR[] = {
    {400, 150},
    {0, 30},
    {250, 250},
    {0, 0}
};

// Bright chime for motor arrival
const Note TONE_ARRIVE[] = {
    {2000, 60},
    {0, 20},
    {2400, 60},
    {0, 20},
    {3000, 100},
    {0, 0}
};

// Quick ascending sweep for homing success
const Note TONE_HOME_FOUND[] = {
    {800,  50},
    {1200, 50},
    {1600, 50},
    {2400, 80},
    {0, 0}
};

// ============================================================
// Sequencer state
// ============================================================
static const Note* currentSequence = nullptr;
static int currentNoteIndex = 0;
static unsigned long noteStartTime = 0;
static bool toneActive = false;

// ============================================================
// Volume / Mute state (Session 17)
// ============================================================
static bool    sMuted  = false;
static uint8_t sVolLvl = 3;       // 0–4

// Apply the current volume duty after starting a tone
static void applyVolumeDuty() {
    if (sVolLvl < 5) {
        ledcWrite(BUZZER_CHANNEL, VOLUME_DUTY[sVolLvl]);
    }
}

// ============================================================
// Idle-silence helper (Session 19)
// ============================================================
// ledcWriteTone(ch, 0) stops the LEDC channel via ledc_stop() with
// idle_level=0, which parks the pin LOW. On the MH-FMD buzzer module
// (active-low driver) that means the driver transistor conducts
// continuously between tones — silent, but it sinks DC through the
// coil the whole time the buzzer is idle (wasted battery, coil
// heating). Call ledc_stop() directly with idle_level=1 so the pin
// sits HIGH when idle instead (transistor off, no DC through the coil).
static void buzzerIdle() {
    ledc_stop((ledc_mode_t)(BUZZER_CHANNEL / 8),
              (ledc_channel_t)(BUZZER_CHANNEL % 8),
              1 /* idle_level: HIGH */);
}

// ============================================================
// API implementation
// ============================================================

void audioInit() {
    ledcSetup(BUZZER_CHANNEL, 2000, BUZZER_RESOLUTION);
    ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);
    buzzerIdle();

    // Read initial state from settings
    sMuted  = !settingsGetSoundOn();
    sVolLvl = settingsGetVolume();
}

void audioPlayTone(const Note* sequence) {
    // If muted, skip entirely
    if (sMuted) return;

    currentSequence = sequence;
    currentNoteIndex = 0;
    noteStartTime = millis();
    toneActive = true;

    if (sequence[0].freq > 0) {
        ledcWriteTone(BUZZER_CHANNEL, sequence[0].freq);
        applyVolumeDuty();
    } else {
        buzzerIdle();
    }
}

void audioUpdate() {
    if (!toneActive || currentSequence == nullptr) return;

    const Note& note = currentSequence[currentNoteIndex];

    // End marker check
    if (note.freq == 0 && note.duration == 0) {
        buzzerIdle();
        toneActive = false;
        currentSequence = nullptr;
        return;
    }

    // Advance when current note expires
    if (millis() - noteStartTime >= note.duration) {
        currentNoteIndex++;
        const Note& next = currentSequence[currentNoteIndex];

        if (next.freq == 0 && next.duration == 0) {
            buzzerIdle();
            toneActive = false;
            currentSequence = nullptr;
            return;
        }

        noteStartTime = millis();
        if (next.freq > 0) {
            ledcWriteTone(BUZZER_CHANNEL, next.freq);
            applyVolumeDuty();
        } else {
            buzzerIdle();
        }
    }
}

void audioStopTone() {
    buzzerIdle();
    toneActive = false;
    currentSequence = nullptr;
}

bool audioIsPlaying() {
    return toneActive;
}

// ============================================================
// Volume / Mute (Session 17)
// ============================================================

void audioSetMute(bool muted) {
    sMuted = muted;
    if (muted) {
        audioStopTone();  // Silence any playing tone
    }
}

void audioSetVolume(uint8_t level) {
    if (level > 4) level = 4;
    sVolLvl = level;
    // If a tone is currently playing, adjust duty immediately
    if (toneActive && currentSequence != nullptr) {
        const Note& note = currentSequence[currentNoteIndex];
        if (note.freq > 0) {
            applyVolumeDuty();
        }
    }
}

bool audioGetMute()       { return sMuted; }
uint8_t audioGetVolume()  { return sVolLvl; }
