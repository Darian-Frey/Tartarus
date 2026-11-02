# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com). Reference F-, D-, AV-, BUG- and IMP- IDs for traceability. Upstream Descent 3 changes arrive via merges and are summarised, not re-listed; see upstream's CHANGELOG for detail.

## [Unreleased]
### Added (Phase 2)
- Raw mouse (F-062): `m_raw` reads the mouse every frame instead of upstream's 20 times a second (off by default for the stock mouse until its sensitivity is tuned; always on for the virtual joystick, which now responds up to 50 ms sooner), `m_sensitivity`, `m_accel`. Console command `where` prints the ship's position and orientation. 4 unit tests — 2026-11-02.
- Options (F2): the Mouse tab is now the **Tartarus** tab and adds a rear-view mirror choice for the left small window: Stock, Top left or Top right (wide mirror), plus "Custom" when set from the console (F-060) — 2026-11-02.
- Small-view windows (rear view, Guide-Bot, missile camera) in the HUD layout: size, shape and position per window with `hud_sv_left_*` / `hud_sv_right_*` and `hud_sv_bigger_scale`; the defaults reproduce the stock layout exactly (F-060). 6 unit tests — 2026-11-01.
- Command-line `-skipbriefing` (no pre-level briefing or ship selection) and `-skipcinematics` (in-game cinematics such as level intros end as soon as a key could skip them; the ship is placed where the cinematic would leave it). For quicker testing: level 1 is flyable in about 9 s instead of 21 s — 2026-11-01.
- Menus no longer sit between black bars on widescreen displays: the bars show a blurred, darkened copy of the screen (`r_bar_fill`, `r_bar_fill_brightness`) (F-060) — 2026-11-01.
- Field of view 60–120° in Tartarus mode (stock 72–90, kept in Classic Mode): Options → Video slider and `r_fov` in the console (F-060) — 2026-11-01.
- Virtual-joystick mouse (F-067, D-013): `m_mode 1` (default in Tartarus mode) treats the mouse offset from centre as a turn rate, so the ship keeps turning until the mouse comes back; `m_mode 2` auto-centres. Dead zone, radius, curve, sensitivity, invert and roll options (`m_vj_*`), an on-screen indicator, recentre on the middle mouse button or `m_recentre`, and resets on every context change (AV-013). Classic Mode keeps the stock mouse. 10 unit tests — 2026-11-01.
- Options (F2) → Mouse tab: mouse mode, speed, centre speed (auto-centre mode), invert pitch, indicator and roll, saved to `tartarus.cfg`; the first Tartarus options page (F-067, F-069). The middle mouse button now only recentres the virtual joystick; F still fires flares — 2026-11-01.
### Added (Phase 1)
- Cheat menu (F-012, D-006): Shift+F12 opens a password screen over the game; 17 of the 19 SPEC §4 cheats (give all, god mode, infinite shields/energy/ammo/afterburner, invulnerability, keys, destroy robots, cloak, automap, level warp, statistics overlay, time scale, self-destruct, relock); "infinite" rows held at maximum every frame; sets `IsCheater` and shows a CHEATS tag; refused in Classic Mode and multiplayer (AV-001, AV-004). Console commands `cheat`, `cheatmenu`, `give`, `god`, `timescale`, `map`. 14 unit tests — 2026-11-01.
- Developer console (F-010): backtick toggles; ship controls suspended while open; scrollback, history (`console_history.txt`), tab completion, engine log mirror; commands `help`, `find`, `cvarlist`, `set`, `reset`, `toggle`, `echo`, `clear`, `exec`, `classic`, `screenshot`, `version`, `quit`; 11 unit tests — 2026-11-01.
- Cvar system (F-011): typed registry with `ARCHIVE`/`CHEAT`/`CLASSIC_LOCKED`/`LATCH`/`READONLY`, atomic `tartarus.cfg` persistence, console-grammar tokenizer; 16 unit tests — 2026-11-01.
- Classic Mode gate (F-002): `classic` cvar (default on), `IsClassic()`, session-only `-classic`/`-tartarus`; `Tartarus_Init()` hook in `InitD3Systems1()`; CMake option `TARTARUS_DEFAULT_CLASSIC` — 2026-11-01.
### Changed
- GitHub Actions builds Linux only (x64 and arm64 cross); Windows and macOS jobs disabled (D-014). The Windows compile error in the console is logged as BUG-009, deferred — 2026-11-01.
- Roadmap: new Phase 2 "Controls and display" (D-013); old Phases 2–6 renumbered 3–7 in all documents. F-060 (now including widescreen menus, priority Must), F-062 and F-063 moved into it — 2026-11-01.
### Added
- F-060 now also covers small-view window size, shape and position (rear view, Guide-Bot, missile cam); SPEC.md §14 — 2026-11-01.
- Features F-067 mouse flight modes (virtual joystick), F-068 advanced graphics options, F-069 options menu overhaul; SPEC.md §13 mouse-flight cvars; AV-013 stuck virtual-joystick offset — 2026-11-01.
- Steam data-directory probe (F-003): finds native, Flatpak and Snap Steam installs and extra library folders, so `-additionaldir` is optional; a missing `d3.hog` now stops with an error listing the searched directories instead of a black screen. 7 unit tests in `Descent3/tartarus/tests/` — 2026-11-01.
- Main-menu build string (F-004): "Tartarus 0.1.0-dev <revision>" over "based on Descent 3 Open Source <upstream hash>"; the upstream hash is kept in `Descent3/tartarus/UPSTREAM_REF`, updated by `tools/upstream-sync.sh --apply` — 2026-11-01.
### Fixed
- Right and middle mouse buttons were swapped on Linux: the middle button fired missiles. Right now fires secondaries and the middle button recentres the virtual joystick, as intended (BUG-007, upstream) — 2026-11-01.
- Mouse pointer far too fast on TelCom screens (briefings, ship select, objectives); it now moves as in the menus (BUG-006, upstream) — 2026-11-01.
- Screen resolution now restored by width x height instead of a list index that can point at a different size (BUG-004, upstream); `-windowed`/`-fullscreen` no longer overwrite the saved fullscreen choice (BUG-005, upstream) — 2026-11-01.
- Docs: removed `-setdir` from all run instructions; upstream dropped it in 2024 and the writable directory is SDL's preference path (BUG-001) — 2026-11-01.
- `tools/upstream-sync.sh`: trial-merges `upstream/main` in a scratch worktree and fails on conflicts outside `Descent3/tartarus/` and the ARCHITECTURE.md hook-point list (IMP-002, F-001, AV-002) — 2026-11-01.
- Upstream Descent 3 merged in at `593b880f` (D-001, F-001); `upstream` remote added. Upstream README/BUILD/CHANGELOG preserved as `docs/UPSTREAM_*.md`; `.gitignore` merged with Tartarus block, exempting upstream's tracked HogMaker assets and test fixture (AV-005) — 2026-11-01.
- Project documentation set (README, FEATURES F-001–F-081, ROADMAP Phases 0–6, ARCHITECTURE, DECISIONS D-001–D-012, SPEC, ATTACK_VECTORS AV-001–AV-012, BUILD, BUGS, IMPROVEMENTS, CLAUDE) — 2026-11-01, against upstream `fa61eec`.
