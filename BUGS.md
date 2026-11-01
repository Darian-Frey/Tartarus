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

### BUG-004: (upstream) Screen resolution not restored correctly between runs
**Status:** fixed
**Severity:** high
**Found:** 2026-11-01 (owner report: "screen settings are not saved"; owner's session log shows the game at 512x384)
**Location:** `Descent3/config.cpp` `ConfigureDisplayResolutions()`, `Descent3/init.cpp` `LoadGameSettings()` / `SaveGameSettings()`
**Description.** Upstream saves the resolution as `RS_resolution`, an index into `Video_res_list`. That list is a built-in fallback (index 7 = 1280x720, the default) until `ConfigureDisplayResolutions()` replaces it with the display's SDL modes, sorted by size (on the reference machine index 7 = 512x384). If a run saves while the list is the fallback (SDL mode enumeration fails, which can happen under PRIME), or the display's mode list changes, the next run restores a different size. The owner's registry held 7 and the game came up at 512x384.
**Fix.** `Descent3/tartarus/display.cpp`: also save `RS_width`/`RS_height`, and restore by size after the mode list is built (`ChooseResolutionIndex`); without a saved size, or if the display no longer offers it, use the desktop resolution. The legacy index is still written for upstream compatibility but no longer read. Hooks in `config.cpp` and `init.cpp`. Unit tests in `display_prefs_tests.cpp`; verified with the owner's registry in an isolated profile (legacy: 1920x1080 instead of 512x384; saved 1280x720: restored exactly). Fixed 2026-11-01. Candidate upstream PR.

### BUG-005: (upstream) `-windowed` / `-fullscreen` permanently overwrite the saved fullscreen choice
**Status:** fixed
**Severity:** medium
**Found:** 2026-11-01 (while investigating BUG-004)
**Location:** `renderer/HardwareOpenGL.cpp` `rend_InitWindowMode()`, `Descent3/init.cpp` `SaveGameSettings()`
**Description.** The flags set `Game_fullscreen` for the session, and `SaveGameSettings()` writes it back as the preference, so one `-windowed` launch turns fullscreen off for good. Tartarus's own docs told the owner to launch with `-windowed`.
**Fix.** `Display_Init()` records the saved preference before the flags apply; `Display_SaveSettings()` rewrites `RS_fullscreen` with `FullscreenToSave()`: if the value is still what the flag forced, the stored preference is kept; a change made in Options that session is saved. Unit tested; end-to-end check (clean exit) pending with the owner. Fixed 2026-11-01. Candidate upstream PR.

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
