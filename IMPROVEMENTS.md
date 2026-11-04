# Improvements

Catalogue of code-quality improvements, refactors, and architectural
changes proposed during development. Per Maintenance Rule 8, improvements
are logged here when noticed, not silently applied. The author decides
whether to apply, defer, or decline.

This is the dual of BUGS.md: bugs are broken; improvements work but
could be better.

Status vocabulary: suggested | applied | declined | deferred.
Effort vocabulary: trivial | small | medium | large.

## Suggested

### IMP-003: Console line wrapping and translucent background
**Status:** suggested
**Found:** 2026-11-01 (first in-game console test)
**Location:** `Descent3/tartarus/console.cpp` `Console_Render()`
**Effort:** small
**Description.** Long lines (e.g. `version` output at 512 px width) are cut off at the right edge, and the background is a solid fill because `rend_FillRect` clears rather than blends.
**Proposal.** Wrap at render time using `grtext_GetTextLineWidth`; draw the background as an alpha-blended 2D polygon so the game shows through.
**Trade-offs.** Wrapping at render time makes scroll offsets count wrapped rows; keep offsets in source lines and accept uneven paging.

### IMP-005: Increase the size of the in-game menus
**Status:** suggested (the deck solves this for options, Save, Load, Keys, the Guide-Bot menu and message boxes, D-016; multiplayer dialogs remain)
**Found:** 2026-11-02 (owner request)
**Location:** `Descent3/newui_core.cpp` `newuiMenu::Create()` and the other `newui` windows; `ui/UISystem.cpp` (`UI_aspect_x/y`); related to F-069
**Effort:** medium
**Description.** In a level the game renders at the display resolution, and `newui` windows (Options on F2, save/load, Guide-Bot and other in-game dialogs) are drawn at the native pixel size of their 640x480-era artwork, centred (`newuiMenu::Create()` sizes the window from its background bitmap). At 1280x720 the Options window is about a third of the screen width and its text is small; at 1920x1080 and above it gets smaller still. At the main menu the same dialogs look larger because menus draw on a 640x480 canvas that is scaled to the window.
**Proposal.** Draw in-game `newui` windows on the same 640x480 canvas the main menu uses (an offscreen target scaled to fit the window with the blurred bar fill, F-060), or apply a uniform UI scale (e.g. `Max_window_h / 480`) to window placement, artwork, fonts and mouse hit-testing. Expose it as a cvar (e.g. `ui_scale`, with an "auto" default) and fold it into F-069.
**Also.** HUD text has the same root cause: the game picks HUD fonts from a few fixed sizes by frame width, so with `r_renderscale` above 1 (F-068) the HUD text shrinks on screen and below 1 it grows and softens. A UI scale would fix both.
**Trade-offs.** The canvas approach is simpler and consistent with the main menu, but the in-game view behind the menu must still render around it. A true scale factor touches every `newui` drawing and hit-test path; the bitmap artwork will look soft when magnified.

### IMP-006: Recreate the menus with a cassette-futurism look and feel
**Status:** suggested (in progress: accepted as D-016; the in-level options deck is built, F-069)
**Found:** 2026-11-02 (owner request)
**Location:** new `Descent3/tartarus/` UI module; upstream `newui` screens (`config.cpp` options, `menu.cpp`/`mmItem.cpp` main menu, pilot, save/load); related to F-069, IMP-005
**Effort:** large
**Description.** The owner would like the menus redesigned in a cassette-futurism style: the retro-future of late-1970s/1980s hardware (chunky bevelled panels, monochrome amber or green phosphor text on dark CRT screens, scanlines and slight glow, segmented and seven-segment readouts, toggle switches, VU-style meters, tape-deck style buttons, utilitarian labels and stencilled numbering). Upstream's menus are fixed-size `newui` bitmap artwork designed for 640x480 (see IMP-005), so restyling them means replacing the UI layer, not reskinning it.
**Proposal.** Build a Tartarus menu layer drawn procedurally (rectangles, lines, text and simple shaders through the existing renderer), laid out in resolution-independent units so it scales to any display (which also resolves IMP-005 and the F-060 menu-canvas item). Start with the options menu as part of F-069 (pages generated from cvar metadata), then the in-game pause/options, then the main menu, pilot and save/load screens. A theme table (colours, phosphor glow, scanline strength, font) keeps the look adjustable, and a plain theme stays available for accessibility. Optional companion (owner, 2026-11-02): a retro CRT filter for the game view itself (scanlines, phosphor mask, slight curvature and bloom) as a post-process pass on the offscreen frame before it is scaled to the window (`rend_Flip()`), with `r_crt_*` cvars; it shares the scanline/glow code with the menus. Any new art (fonts, panel textures, sounds) must be original or GPL-compatible (AV-005, D-003 licence review for fonts).
**Trade-offs.** Large: every screen that moves over needs its input, focus and keyboard navigation rebuilt and tested. Upstream's `newui` screens keep working until each is replaced, so the change can land screen by screen. Open question for the owner: should Classic Mode keep the stock menus (stricter "Classic is classic", AV-001) or use the new look too (menus are not gameplay)?

### IMP-007: Precise frame limiter
**Status:** suggested
**Found:** 2026-11-02 (F-068 frame-cap test)
**Location:** `Descent3/GameLoop.cpp` `GameFrame()` (`Min_allowed_frametime`, `timer_GetMSTime()`), `CalcFrameTime()`
**Effort:** small
**Description.** Upstream's frame limiter and frame timer count whole milliseconds. Caps therefore land on 1000/n fps (`r_maxfps 144` gives a 6 ms frame, about 166 fps), and a frame faster than 1 ms would measure as zero time, which is why `r_maxfps 0` still stops at 1000 fps.
**Proposal.** Time frames with the microsecond clock (`D3::ChronoTimer` has sub-millisecond resolution) and sleep/spin to a fractional target; keep `Frametime` from ever being zero. Pairs with F-061 (fixed-timestep physics).
**Trade-offs.** Touches the core loop timing; verify that game speed, physics and demo playback are unchanged at 60 fps.

### IMP-008: Translucent lines for the stick indicator
**Status:** suggested
**Found:** 2026-11-03 (F-069 scanlines)
**Location:** `renderer/HardwareOpenGL.cpp` `rend_DrawLine()`, `Descent3/tartarus/mouse_flight.cpp` `MouseFlight_Render()`
**Effort:** trivial
**Description.** `rend_DrawLine()` forces `AT_ALWAYS`, so the alpha set in `MouseFlight_Render()` has no effect: the virtual-joystick ring is opaque.
**Proposal.** Remove the alpha calls and pick a dimmer colour, or draw the ring with a small alpha-capable helper.
**Trade-offs.** None of note.

### IMP-009: Quieter start-up and level logs
**Status:** suggested
**Found:** 2026-11-03 (owner asked about errors in the terminal)
**Location:** `Descent3/OsirisLoadandBind.cpp` `Osiris_BindScriptsToObject()`; `Descent3/init.cpp` `LoadGameSettings()`
**Effort:** trivial
**Description.** Two harmless upstream messages make a normal run look broken. (1) Every object type with no default script logs `ERROR ... OSIRIS: Unable to load module () to bind to object (...)`, hundreds per level; the empty `()` means there is no script to load, and objects with one load correctly. (2) `Game settings contain a display resolution index that is out of bounds` appears because the saved index is checked against the short fallback list before the display's modes are known; since BUG-004 the resolution is restored by size, so the index no longer matters.
**Proposal.** Skip the message when the module name is empty, and skip the index check (or lower it to debug), each as a marked hook.
**Trade-offs.** Two more small edits in upstream files.

## Applied

### IMP-004: Pause the game while the cheat menu (or console) is open
**Status:** applied
**Found:** 2026-11-01 (F-012 in-game test)
**Location:** `Descent3/tartarus/cheats.cpp`, `console.cpp`; `GameFrame()` in `Descent3/GameLoop.cpp`
**Effort:** small
**Description.** Robots keep attacking while the player types a password. Upstream's `PauseGame()` cannot be used as is: while `Game_paused` is set, `GameFrame()` stops calling `ProcessKeys()`, so the menu could never be closed. Scaling `Frametime` to 0 is not safe either: some code divides by it (e.g. `AImain.cpp`, `Controls.cpp`).
**Proposal.** Stop simulation the way `Game_paused` does but keep feeding keys to `tartarus::HandleKey()` (one extra call in the paused branch of `GameFrame()`), then use it for both the cheat menu and `con_pause` (F-010).
**Trade-offs.** One more hook point in `GameFrame()`; sound and music pause/resume must match upstream's `PauseGame()`/`ResumeGame()` exactly.
**Applied.** 2026-11-02 as proposed: `con_pause` (default 1) calls upstream `PauseGame()` while the console or cheat menu is open (single-player, in a level), `tartarus::PausedFrame()` in the paused branch of `GameFrame()` feeds them keys, and closing calls `ResumeGame()`. Verified: "Game paused"/"Game resumed" around both, typing works while paused.

### IMP-001: Cross-reference checker for the standard documents
**Status:** applied
**Found:** 2026-11-01 (documentation scaffolding session)
**Location:** `tools/check_xrefs.py` (new)
**Effort:** small
**Description.** The docs already cross-link F-/D-/AV- IDs in both directions by hand; Maintenance Rule 6 says this decays without tooling.
**Proposal.** Script walks all `*.md`, builds the ID graph, reports IDs referenced but undefined and references that are unidirectional. Run as a pre-commit hook and in CI.
**Trade-offs.** One more script to maintain; false positives on prose mentions of IDs unless the parser is restricted to `**Related:**` lines and tables.
**Notes.** Phase 1 deliverable per D-011.
**Applied.** 2026-11-02: `tools/check_xrefs.py` (errors: undefined or duplicate IDs; warnings: one-way `**Related**` links; `--strict` fails on warnings) and the `Docs cross-references` GitHub workflow running it on every push (D-015 keeps that workflow on push while the game build runs on demand). First run found 1 error and 21 one-way links, all fixed.

### IMP-002: Upstream-sync helper
**Status:** applied
**Found:** 2026-11-01
**Location:** `tools/upstream-sync.sh` (new)
**Effort:** small
**Description.** Monthly upstream merges are the main maintenance cost of the fork strategy (D-001, AV-002).
**Proposal.** Fetch `upstream`, attempt merge on a scratch branch, list conflicting files, fail if any lie outside `Descent3/tartarus/` or the ARCHITECTURE.md hook-point list, print the `// TARTARUS:` marker count.
**Trade-offs.** Encodes the hook-point list twice (script and doc) unless the script parses ARCHITECTURE.md's table — do that.
**Notes.** Phase 0 deliverable. Applied 2026-11-01 as `tools/upstream-sync.sh`: dry run in a throwaway worktree by default, `--apply` merges with `--no-commit`; the hook-point list is parsed from ARCHITECTURE.md, plus `Descent3/tartarus/` and the root files Tartarus replaces (README/BUILD/CHANGELOG/.gitignore).


## Declined

## Deferred
