# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com). Reference F-, D-, AV-, BUG- and IMP- IDs for traceability. Upstream Descent 3 changes arrive via merges and are summarised, not re-listed; see upstream's CHANGELOG for detail.

## [Unreleased]
### Added (Phase 1)
- Cvar system (F-011): typed registry with `ARCHIVE`/`CHEAT`/`CLASSIC_LOCKED`/`LATCH`/`READONLY`, atomic `tartarus.cfg` persistence, console-grammar tokenizer; 16 unit tests — 2026-11-01.
- Classic Mode gate (F-002): `classic` cvar (default on), `IsClassic()`, session-only `-classic`/`-tartarus`; `Tartarus_Init()` hook in `InitD3Systems1()`; CMake option `TARTARUS_DEFAULT_CLASSIC` — 2026-11-01.
### Changed
- Roadmap: new Phase 2 "Controls and display" (D-013); old Phases 2–6 renumbered 3–7 in all documents. F-060 (now including widescreen menus, priority Must), F-062 and F-063 moved into it — 2026-11-01.
### Added
- F-060 now also covers small-view window size, shape and position (rear view, Guide-Bot, missile cam); SPEC.md §14 — 2026-11-01.
- Features F-067 mouse flight modes (virtual joystick), F-068 advanced graphics options, F-069 options menu overhaul; SPEC.md §13 mouse-flight cvars; AV-013 stuck virtual-joystick offset — 2026-11-01.
- Steam data-directory probe (F-003): finds native, Flatpak and Snap Steam installs and extra library folders, so `-additionaldir` is optional; a missing `d3.hog` now stops with an error listing the searched directories instead of a black screen. 7 unit tests in `Descent3/tartarus/tests/` — 2026-11-01.
- Main-menu build string (F-004): "Tartarus 0.1.0-dev <revision>" over "based on Descent 3 Open Source <upstream hash>"; the upstream hash is kept in `Descent3/tartarus/UPSTREAM_REF`, updated by `tools/upstream-sync.sh --apply` — 2026-11-01.
### Fixed
- Docs: removed `-setdir` from all run instructions; upstream dropped it in 2024 and the writable directory is SDL's preference path (BUG-001) — 2026-11-01.
- `tools/upstream-sync.sh`: trial-merges `upstream/main` in a scratch worktree and fails on conflicts outside `Descent3/tartarus/` and the ARCHITECTURE.md hook-point list (IMP-002, F-001, AV-002) — 2026-11-01.
- Upstream Descent 3 merged in at `593b880f` (D-001, F-001); `upstream` remote added. Upstream README/BUILD/CHANGELOG preserved as `docs/UPSTREAM_*.md`; `.gitignore` merged with Tartarus block, exempting upstream's tracked HogMaker assets and test fixture (AV-005) — 2026-11-01.
- Project documentation set (README, FEATURES F-001–F-081, ROADMAP Phases 0–6, ARCHITECTURE, DECISIONS D-001–D-012, SPEC, ATTACK_VECTORS AV-001–AV-012, BUILD, BUGS, IMPROVEMENTS, CLAUDE) — 2026-11-01, against upstream `fa61eec`.
