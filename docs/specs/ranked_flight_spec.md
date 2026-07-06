Session — Ranked Flight Mode (Core Logic)
Prerequisite files to upload
Since this touches the game state machine and menu, upload the current versions of:

game.h, game.cpp
screens.cpp (menu + tasting hint dispatch)
ui.h (confirm ScrollList API hasn't changed)
config.h (confirm NUM_GLASSES, color constants match)

No changes needed to motor.cpp, audio.cpp, input.cpp, browse.cpp/h, wifi_portal.*, settings.* — don't upload those.
Design decisions (confirmed)

Standalone mode GAME_MODE_RANK, reusing the browse-library naming/pour flow from Named/Best Guess.
Ranking entry is sequential pool-pick: one glass chosen per slot, favorite first.
Reveal order is rank-reversed — least favorite (4th) revealed first, favorite (1st) revealed last.
No score/detail states — ranking has no "correct" answer, so it flows straight into the existing GAME_REVEAL.
Phone/WebSocket parity is a separate follow-up session.

Changes by file
game.h

Add GAME_MODE_RANK to GameMode
Add GAME_RANKING to GameState
Add to GameSession: int rankOrder[NUM_GLASSES], int rankIndex

game.cpp

resetSession(): clear rankOrder[] (-1 or 0 sentinel) and rankIndex = 0
modeUsesNames(): include GAME_MODE_RANK
getModeTitleText(): "RANKED FLIGHT"
New static pool state: rankPoolLabels[], rankPoolGlass[], rankPoolCount, rankList (mirrors guessPoolLabels/guessPoolPourIdx/guessList)
New helper buildRankPool() — populates pool from session.glassUsed[] glass numbers not yet in rankOrder[], mirrors buildGuessPool()'s exclude/pre-highlight logic
New draw function drawRanking(bool fullRedraw) — title bar shows ordinal ("1ST PLACE?" … "4TH PLACE?") via a small ordinal-string helper; body is uiScrollListDraw(&rankList); soft buttons "BACK" / "LOCK IN"
Input handling for GAME_RANKING (parallel to existing GAME_GUESSING case): LOCK IN commits rankOrder[rankIndex], increments rankIndex; BACK decrements and restores the glass to the pool; on rankIndex == revealMapCount after commit, build revealMap[] in rank-reversed order and transition to GAME_REVEAL
Modify buildRevealMap() call site: when mode is Rank, build the rank-order variant instead of the glass-number variant (either branch inside the same function on currentMode, or a sibling buildRevealMapFromRank() — I'd lean toward a mode check inside the existing function to avoid duplicating the pourIdx lookup)
drawReveal(): when currentMode == GAME_MODE_RANK, add a second label under "Glass N" showing place ("4TH PLACE" etc., derived from revealMapCount - 1 - idx)
drawDone(): no logic change needed — already just walks revealMap[] in whatever order it was built, so rank order falls out for free
Tasting screen: hint/button text for Rank mode → "Press to Rank" / "RANK" (parallel to the existing Guess-mode branch)
Dispatch drawRanking in the screen's state switch; wire GAME_RANKING into whatever transition currently sends GAME_TASTING → GAME_GUESSING

screens.cpp

menuLabels[] / menuSessionNotes[]: add "Ranked Flight", bump MENU_COUNT to 5
menuInput(): new else if (sel == 3) branch — gameSetMode(GAME_MODE_RANK); uiPushScreen(&screenGame); (non-T push, same reasoning as Named/Guess); shift Settings to sel == 4

Testing checklist

 "Ranked Flight" appears in menu, launches naming flow correctly (browse library works same as Best Guess)
 All 4 glasses pour and tasting screen shows "Press to Rank"
 Ranking screen shows "1ST PLACE?" first, pool has all 4 glasses
 Picking a glass advances to "2ND PLACE?" with 3 remaining in pool
 BACK from "2ND PLACE?" returns to "1ST PLACE?" with previous pick restored to pool
 After 4th pick, auto-advances to Reveal
 Reveal shows 4th place first, counts down to 1st place last, with correct name/pour# per glass
 Progress dots and NEXT/FINISH button behave same as other modes
 Done screen lists results in the same rank order (4th → 1st)
 New Flight from Done screen resets rankOrder/rankIndex cleanly, no stale pool state on a second play