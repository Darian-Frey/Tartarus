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

## Won't Fix

## Deferred

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
