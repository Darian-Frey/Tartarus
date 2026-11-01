# Bugs

Catalogue of bugs discovered during development. Per Maintenance Rule 8,
bugs are logged here when found, not silently fixed. The author decides
whether to fix immediately, defer, or leave alone.

Bugs inherited from upstream Descent 3 are logged here too, tagged
`(upstream)` in the title, with status `deferred` unless Tartarus fixes
them locally; where a fix is general, prefer sending it upstream and
recording the PR link in Notes.

Status vocabulary: open | fixed | wontfix | deferred.
Severity vocabulary: low | medium | high.

## Open

_None._

## Fixed

### BUG-007: (upstream) Right and middle mouse buttons swapped on Linux
**Status:** fixed
**Severity:** high
**Found:** 2026-11-01 (owner report: the middle button fires missiles and the right button does not)
**Location:** `ddio/lnxmouse.cpp` `sdlMouseButtonDownFilter()` / `sdlMouseButtonUpFilter()`
**Description.** SDL numbers mouse buttons 1 = left, 2 = middle (`SDL_BUTTON_MIDDLE`), 3 = right (`SDL_BUTTON_RIGHT`). The Linux driver maps SDL button 2 to `MOUSE_RB` and 3 to `MOUSE_CB`, so the engine's "right" button is the physical middle one and vice versa. With the stock bindings (button 2 = secondary fire, button 3 = flare) the middle button fired missiles and the right button fired flares; after F-067 the right button recentred the virtual joystick instead of the middle one.
**Fix.** Both handlers swap SDL's middle and right numbers before dispatching, so the engine sees left = 1, right = 2, middle = 3 as on Windows. Existing pilot bindings keep their meaning (right = secondary, middle = flare / recentre). Compatibility fix, active in Classic Mode too. Verified in level 1 by scripted run: the physical middle button now recentres the virtual joystick; owner to confirm right-button missiles. Fixed 2026-11-01. Candidate upstream PR.

### BUG-006: (upstream) Mouse pointer far too fast in TelCom screens (briefing, ship select)
**Status:** fixed
**Severity:** medium
**Found:** 2026-11-01 (owner report: the mouse becomes very fast on the briefing screen before a mission)
**Location:** `Descent3/TelCom.cpp` `TelComShow()` (`ddio_MouseSetVCoords(Game_window_w, Game_window_h)`) and `TelComHandleAllEvents()` (`ddio_MouseGetState` ~line 2672); `Descent3/TelComGoals.cpp` ~line 535; `ui/UISystem.cpp` `kDefaultMouseScale`; `ddio/lnxmouse.cpp` `sdlMouseMotionFilter()`
**Description.** Upstream commit `5b18e22e` ("Fix mouselook", 2025-04) made the Linux mouse driver sum motion deltas between reads, and moves the pointer position by that running sum on every motion event (`x += dx` where `dx` is accumulated). To keep menus usable it gave the UI library a 20x virtual mouse space (`ddio_MouseSetVCoords(w * 20, h * 20)`) and divides positions by `kDefaultMouseScale` = 20 in `ui_MousePoll()`. TelCom was not updated: it sets a 1x space and reads raw `ddio_MouseGetState` positions, so the same hand movement moves the pointer about 20 times further than in the main menu, and more the faster the mouse moves (the running sum makes the motion grow with the number of events per frame).
**Fix.** Option (a), chosen by the owner 2026-11-01: TelCom gets the same 20x virtual mouse space as the UI library and divides its two position reads by the same factor (`tartarus::kUiMouseScale` in `Descent3/tartarus/mouse_compat.h`). Hooks in `TelCom.cpp` (include, `ddio_MouseSetVCoords`, the read in `TelComHandleAllEvents`) and `TelComGoals.cpp` (include, tooltip read). Compatibility fix, active in Classic Mode too. Verified by injecting identical mouse motion on the main menu and in TelCom (Shift+Tab in a level): the pointers travel comparable distances; owner confirmed on the pre-mission briefing, 2026-11-01. Fixed 2026-11-01.
**Remaining.** Option (b), fixing the driver itself (the pointer should move by each event's own delta, `x += xrel`, while the summed `dx`/`dy` stay for mouselook, then the 20x scaling goes from the UI and TelCom), is the real cause and an upstream PR candidate; it belongs with F-067, which reworks that code. Not yet checked against upstream's issue tracker (`gh` is not logged in here).

### BUG-004: (upstream) Screen resolution not restored correctly between runs
**Status:** fixed
**Severity:** high
**Found:** 2026-11-01 (owner report: "screen settings are not saved"; owner's session log shows the game at 512x384)
**Location:** `Descent3/config.cpp` `ConfigureDisplayResolutions()`, `Descent3/init.cpp` `LoadGameSettings()` / `SaveGameSettings()`
**Description.** Upstream saves the resolution as `RS_resolution`, an index into `Video_res_list`. That list is a built-in fallback (index 7 = 1280x720, the default) until `ConfigureDisplayResolutions()` replaces it with the display's SDL modes, sorted by size (on the reference machine index 7 = 512x384). If a run saves while the list is the fallback (SDL mode enumeration fails, which can happen under PRIME), or the display's mode list changes, the next run restores a different size. The owner's registry held 7 and the game came up at 512x384.
**Fix.** `Descent3/tartarus/display.cpp`: also save `RS_width`/`RS_height`, and restore by size after the mode list is built (`ChooseResolutionIndex`); without a saved size, or if the display no longer offers it, use the desktop resolution. The legacy index is still written for upstream compatibility but no longer read. Hooks in `config.cpp` and `init.cpp`. Unit tests in `display_prefs_tests.cpp`; verified with the owner's registry in an isolated profile (legacy: 1920x1080 instead of 512x384; saved 1280x720: restored exactly); owner confirmed the resolution survives a restart, 2026-11-01. Fixed 2026-11-01. Candidate upstream PR.

### BUG-005: (upstream) `-windowed` / `-fullscreen` permanently overwrite the saved fullscreen choice
**Status:** fixed
**Severity:** medium
**Found:** 2026-11-01 (while investigating BUG-004)
**Location:** `renderer/HardwareOpenGL.cpp` `rend_InitWindowMode()`, `Descent3/init.cpp` `SaveGameSettings()`
**Description.** The flags set `Game_fullscreen` for the session, and `SaveGameSettings()` writes it back as the preference, so one `-windowed` launch turns fullscreen off for good. Tartarus's own docs told the owner to launch with `-windowed`.
**Fix.** `Display_Init()` records the saved preference before the flags apply; `Display_SaveSettings()` rewrites `RS_fullscreen` with `FullscreenToSave()`: if the value is still what the flag forced, the stored preference is kept; a change made in Options that session is saved. Unit tested; owner confirmed end to end (fullscreen survives a `-windowed` launch), 2026-11-01. Fixed 2026-11-01. Candidate upstream PR.

## Won't Fix

## Deferred

### BUG-003: (upstream?) Reticle images `ret_pr0a.ogf` / `ret_pr0b.ogf` not found at level start
**Status:** deferred
**Severity:** low
**Found:** 2026-11-01 (console log mirror, level 1, Steam Linux data)
**Location:** `Descent3/hud.cpp` `InitReticle()` (around line 1830); names listed near line 1703
**Description.** Each level start logs `Unable to find bitmap ret_pr0a.ogf` and `ret_pr0b.ogf` (16 lines in one session). The reticle still draws; the effect of the missing segments is not yet known. May be data the Steam Linux depot lacks, or a name mismatch upstream.
**Notes.** Investigate when the HUD is touched in Phase 2 (F-060). Check whether the files exist in any HOG (`d3.hog`, `extra*.hog`) before calling it upstream.

### BUG-001: (upstream) USAGE.md documents the removed `-setdir` and `-useexedir`
**Status:** deferred
**Severity:** low
**Found:** 2026-11-01 (Phase 0 first run)
**Location:** `USAGE.md` §Base directories and §Command-line options (upstream file, kept unmodified)
**Description.** Upstream commit `8800f1b4` (2024-10-08) removed both options; the writable base directory is now always SDL's preference path (`~/.local/share/Outrage Entertainment/Descent 3/` on Linux). USAGE.md still documents them and says the writable directory defaults to the current working directory. Passing `-setdir` is silently ignored.
**Notes.** Tartarus docs corrected (BUILD.md §4, README, CLAUDE.md, ARCHITECTURE.md). A general fix, so a candidate upstream PR.

### BUG-002: (upstream) "Ignoring nonexistent base directory: " with an empty path at every launch
**Status:** deferred
**Severity:** low
**Found:** 2026-11-01 (Phase 0 first run)
**Location:** `cfile/CMakeLists.txt` (`DEFAULT_ADDITIONAL_DIRS_CPP_EXPR`), `cfile/default_base_directories.h.in`
**Description.** With `DEFAULT_ADDITIONAL_DIRS` empty (the default), the CMake string handling wraps it as `""`, giving a one-element list holding an empty path; `cf_AddDefaultBaseDirectories()` passes it to `cf_AddBaseDirectory()`, which logs a warning. Harmless, but noise in every log.
**Notes.** Fix is to emit an empty initialiser when the option is empty. A general fix, so a candidate upstream PR. Related observation (by design upstream, not a bug): in portable builds `D3_DATADIR` is `"."`, so the current working directory is always a read-only base directory.
