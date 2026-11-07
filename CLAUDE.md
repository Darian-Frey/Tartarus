# CLAUDE.md

## Project
Tartarus (working title, D-002) is a Linux-first fork of the GPL-3.0 Descent 3 engine (DescentDevelopers/Descent3) that keeps stock gameplay behind a Classic Mode flag and adds tactical features: developer console + cvars, password-gated cheat menu, player cloak, gravity missile, repulsor, orientation aids, automap overhaul, and modernisation (FOV, raw mouse, quicksave, fixed timestep). All additions live in `Descent3/tartarus/` with a short list of marked hook points.

## Current state
- Upstream **https://github.com/DescentDevelopers/Descent3** is the `upstream` remote and was merged into `main` on 2026-11-01 at `593b880f` (`--allow-unrelated-histories`; repo is not a GitHub fork). Upstream's own README/BUILD/CHANGELOG live in `docs/UPSTREAM_*.md`; ours are at the root. `.gitignore` = upstream's file unchanged + a Tartarus block at the end. `Descent3/tartarus/` (wired in via `tartarus.cmake`) has: datadir (F-003), version (F-004), cvars (F-011), Classic gate (F-002), console (F-010), display fixes (BUG-004/005), TelCom pointer speed (BUG-006), mouse buttons (BUG-007), cheat menu (F-012, 17/19 rows), virtual-joystick mouse (F-067), raw mouse (F-062, partly), graphics options (F-068), options deck (F-069, D-016), deck main menu and other deck screens. 128 `// TARTARUS:` markers in `Descent3/CMakeLists.txt`, `init.cpp`, `mmItem.cpp`, `gamesave.cpp`, `newui.cpp`, `help.cpp`, `buddymenu.cpp`, `pilot.cpp`, `ctlconfig.cpp`, `hud.cpp`, `config.cpp`, `GameLoop.cpp`, `TelCom.cpp`, `TelComGoals.cpp`, `Controls.cpp`, `ddio/sdlcontroller.cpp`, `ddio/lnxmouse.cpp`, `renderer/HardwareOpenGL.cpp`, `renderer/dyna_gl.h`, `renderer/ShaderProgram.h`, `lib/renderer.h`, `gamesequence.cpp`, `gamecinematics.cpp`, `TelComAutoMap.cpp`, `SmallViews.cpp`, `game.cpp`, `menu.cpp`, `.github/workflows/build.yml`, `.github/workflows/cd.yml`; GameLoop hooks call the dispatchers in `tartarus.cpp` (`HandleKey`, `Frame`, `TimeScale`, `RenderOverlays`), so new modules plug in there, not via new hooks. All 172 ctest tests pass (upstream's + ours). The game reaches level 1 from the Steam install with no flags. Documents: README, FEATURES, ROADMAP, ARCHITECTURE, DECISIONS (D-001–D-017), SPEC, ATTACK_VECTORS (AV-001–AV-013), BUILD, BUGS, IMPROVEMENTS, CHANGELOG.
- Upstream reference for all file/symbol names in the docs: commit `fa61eec` (2026-11-01); the merged `593b880f` only adds netgames/netcon logging on top. Upstream is on SDL3 (1.6.0-dev); its own `BUILD.md`/`USAGE.md` are authoritative for build mechanics.
- Verified upstream facts the design relies on: `GameCheat.cpp` typed-sequence cheats and `IsCheater`; `MakeObjectInvisible()` in `object_lighting.h`; `GRAVITY_FIELD_INDEX` / `DoGravityFieldEffect` in `fireball.cpp`; `phys_apply_force()` in `physics/physics.cpp`; autolevel block in `physics/physics.cpp` (`MAX_AUTOLEVEL_TILT_ANGLE`); `AllocWeapon()`/`FindWeaponName()` in `weapon.cpp`, `MAX_WEAPONS 200`; `HUD_ITEM_*` in `hud.h`; `TelComAutoMap.cpp`; `MAX_ENERGY/MAX_SHIELDS 200`.

## Active task
**Phase 2 — Controls and display** (ROADMAP.md), started 2026-11-01. F-067 virtual-joystick mouse done; the owner completed level 1 with it, which met Phase 0's acceptance (levels 2–3 and Mercenary playthrough still open there). F2 → Tartarus tab (first F-069 page; one tab because upstream's option list holds eight buttons) has mouse mode, speed, centre speed, invert, indicator, roll and rear-view mirror presets. F-067 tuning 2026-11-07: full-turn travel is `m_vj_travel` in mouse counts (was `m_vj_radius`, a share of window height, so feel changed with window size); defaults are now the owner's settings (auto-centre, centre speed 2.0, speed 0.75, inverted pitch) as the old ones flew badly. Curve, dead zone and travel fine at defaults (owner); F-067 done. F-060 essentially done: FOV 60–120, blurred menu bar fill, small-view layout cvars; 3D aspect, cockpit and HUD fine at 16:9/16:10 and at 21:9 (checked 2026-11-07 in a 1920x810 window, F-060 done); widescreen menu canvas deferred to F-069 (owner). F-062 done 2026-11-07: `m_raw`/`m_sensitivity`/`m_accel` and `where`; the owner keeps the stock mouse as it is, so `m_raw` stays off (the 18° vs 7° result at 60 vs 120 fps with `m_raw` was never explained); the virtual joystick is read every frame. F-068 graphics options done: VSync (BUG-010 fixed), `r_maxfps`, trilinear/anisotropic, 24-bit depth, MSAA, `r_renderscale`, `r_gamma`, `r_restart`, all on the deck's Graphics page. F-069 options deck done in levels (F2) and from the main menu (D-016): cassette-futurism, amber on gunmetal; Classic Mode and multiplayer keep the stock menu. Menu rebuild in progress (owner, 2026-11-04; gamepad moved later): main menu, Save, Load, New Game, level warp, Pilots, bindings, weapon order, Keys, Guide-Bot and message boxes done through `Deck_RunList()` (`deck_save.cpp`, `deck_screens.cpp`, `deck_controls.cpp`); the deck's Controls page has a Key bindings row. The owner has not yet opened the Joystick and mouse or Weapon order pages. Full Classic run passed 2026-11-06, covering everything to `414a55d5`. Still stock: ship config, Adjust settings dialogs, post-level results, multiplayer, TelCom. Stock screens must keep the 640x480 frame: only deck screens switch to native (`Display_NativeMenu()`).
**Phase 3** (ROADMAP) started 2026-11-04 with Phase 2 still open for mouse tuning: F-040 up-tick done (`orient_core.cpp`, `orient_hud.cpp`), accepted by the owner; F-041 ball and F-042 gizmo done, with opacity settings at the owner's request (all three about 0.05 ms a frame). F-046 flight HUD on the canopy (D-017, replaces D-010; on by default in cockpit view, other aids off, `hud_aids` master switch): stage 1 accepted by the owner (horizon bar, ladder, smoothed flight path marker; hook in `hud.cpp` before `RenderCockpit()`); stage 2 accepted by the owner (roll arc, heading tape F-045, batching via `rend_DrawTriangles2D`, about 0.08 ms). Stage 3 accepted by the owner in combat (speed tape, wall range by one ray every 4 frames, climb rate). Measured at 1080p 2026-11-06: about 0.13 ms average, worst single frame in 120 up to 0.29 ms; text is over half, at about 1 µs a character, so the only saving is fewer labels (a static vertex array in `addVertexData()` was tried and changed nothing). F-043 hold (X) and snap (Z) done, accepted by the owner: `level_assist.cpp`, bank input in the controls stage, so `physics.cpp` needed no change; `al_strength` defaults to 0. Stage 4 status words done. F-050 automap: laser scanner prototype done, the owner is happy with 200-unit range (`scan_core.cpp`, `map_scan.cpp`, owner's idea; 200-unit range). Wireframe look done, accepted by the owner (`rend_DrawLines3D`). `.amap` saving done; full-resolution map screen and ship marker done, unscanned surfaces blank, accepted by the owner (`Deck_RunView()`, `MapScan_RunMap()`, `TCAMDeck*`). Level name and marker text restored; compass and closing cue done (F-051). Symbols and filters done (no robots; powerups off by default; only where scanned). 2D mode moved to IMP-010 (owner). Automap done, apart from checking keys, goals and locked doors on a later level. F-046 leftovers: moving with the cockpit shake, a default key for `hud_aids`. Acceptance needs a new-player test.
F-047 reticle styles done, approved by the owner (`hud_reticle`, hook in `RenderReticle()`), inserted before F-043 at the owner's request; Gunsight is the default unless the owner picks another.
**Phase 1** closed 2026-11-02 (tidy-up: `con_pause`, `bind`/`unbind`, `cheat use`, `check_xrefs.py`, Classic audit + checklist). Deferred, listed in ROADMAP Phase 1: F-013 overlays (before Phase 4), WRAITH/ARGUS, HYDRA countermeasures, AV-004 LAN check, `con_key`, line wrapping, `noclip`/`spawn`/`map` arguments, Options Classic toggle (F-069).

**Owner confirmed 2026-11-01:** BUG-004, BUG-005 and BUG-006 fixes; the F-012 cheat menu on Shift+F12. Commit and push only when the owner says so.

**Open with the owner:** BUG-011 (Alt+F2 is taken by Cinnamon; proposed Save/Load entries in the deck plus console commands), IMP-009 (quieter logs).

**Open question for the owner:** should Tartarus use its own SDL pref dir (e.g. `~/.local/share/Tartarus/`) instead of sharing `Outrage Entertainment/Descent 3/`? Asked twice 2026-11-01, unanswered; would become the next decision and touch `D3_PREF_ORG/APP`.

## Invariants
- `IsClassic()` early-out in every Tartarus per-frame path (AV-001).
- No edits to upstream struct layouts; side tables + save sidecar chunk only (AV-003).
- Every edit outside `Descent3/tartarus/` carries `// TARTARUS:` and appears in ARCHITECTURE.md §Hook points.
- Movement via `phys_apply_force`, never by writing positions (AV-006).
- No game data in the repo, ever (AV-005); no machine-specific paths either (`CLAUDE.local.md` is gitignored).
- Every SPEC constant is a cvar.
- Cheats set `IsCheater`, show the HUD tag, refuse in multiplayer and Classic.

## Build & test
```bash
cmake --preset linux && cmake --build --preset linux --config Release
cmake --preset linux -DBUILD_TESTING=ON && cmake --build --preset linux --config Debug && ctest --test-dir builds/linux -C Debug --output-on-failure
./builds/linux/build/Release/Descent3 -windowed -logfile   # Steam data found automatically (F-003); log → ./Descent3.log
# Scripted/automated test runs: isolated profile (XDG_DATA_HOME=<scratch>, volumes 0) and
#   -windowed -nosound -nomusic -pilot <name> -mission d3 -loadlevel 1 -skipbriefing -skipcinematics
```

## Local environment
Machine-specific facts (the owner's Descent 3 install path, Steam library layout, GPU quirks) live in **`CLAUDE.local.md`**, which is gitignored and never committed. Read it at session start if present. The repo documents only *which* files are needed (BUILD.md §4a), never where they are on anyone's machine.

## Conventions
- British English in docs and UI strings. ISO 8601 dates.
- C++17 as upstream (`CMAKE_CXX_STANDARD 17`: no `std::span`); follow upstream `.clang-format`. New files: `snake_case.cpp`, functions `PascalCase` to match the surrounding engine, cvars `snake_case` with area prefix (SPEC §2).
- Commit messages: multi-paragraph; reference IDs (`F-021`, `D-009`, `AV-006`, `BUG-003`); first line ≤ 72 chars. One Subtle Chaos anomaly per commit body is the owner's house style — leave it to him unless asked.
- Stable IDs are append-only; status flags, never deletion.
- Maintenance Rule 8: when you notice a bug or a refactor opportunity while doing something else, **log it** in BUGS.md / IMPROVEMENTS.md; do not silently fix or apply.
- Docs change in the same commit as the code they describe. Refresh this file at the end of every significant session (state, not history).

## Pitfalls
- Scripted test runs: `tools/run_probe.sh <dir> <probe.py> [args]` with `tools/gamedrive.py` (silent throwaway profile; the scratchpad is cleared between sessions). Warn the owner before long batches (each launch ~30 s, window grabs the mouse); prefer 1–4 targeted runs. Opening the console centres the virtual joystick (AV-013), so measure before opening it.
- CI (GitHub Actions): every push runs only the "Tartarus audits" workflow; the game build (one Linux x64 Release job) runs on demand from the Actions tab, weekly (Mondays 04:17 UTC) and on pull requests (D-014, D-015). Windows/macOS breakage is logged as deferred, not fixed. Suggest a manual build run before releases and after dependency or build-file changes.
- Upstream registry values are hex (`dword:48` = 72). Resolution is restored by `RS_width`/`RS_height` (BUG-004); `-windowed`/`-fullscreen` are session-only (BUG-005). To test display settings without touching the owner's, run with `XDG_DATA_HOME=<scratch>` (SDL's pref path follows it).
- `-setdir`/`-useexedir` no longer exist upstream (BUG-001); the writable dir is SDL's pref path `~/.local/share/Outrage Entertainment/Descent 3/`. The current working directory is also a read-only base dir in portable builds.
- ctest names tests by gtest suite (`SteamTree.*`), not by executable; filter with `-R` on suite names.
- See ATTACK_VECTORS.md for the canonical list.
- The console key intercept must run *before* `DemoCheats(key)` in `GameLoop.cpp` or typed cheats eat console input and vice versa (AV-008).
- The stock gravity field pins objects with `PF_DESTINATION_POS` — don't copy that line (D-009).
- `MakeObjectInvisible` has a `no_hud_message` parameter; pass `true` for the cloak or every activation prints the powerup message.
- Weapon table is binary and editor-only on Windows; new weapons go through the sidecar (D-007). Check `MAX_WEAPONS` and `FindWeaponName` collisions (AV-010).
- The Linux Steam depot ships `.ogv` movies, which upstream cannot decode; cutscenes are simply skipped (owner's choice, 2026-11-01). Don't add an OGV decoder or fetch the Windows depot without asking.
- Run `python3 tools/classic_audit.py` after adding or changing a hook: every hook and the `tartarus::` / `Tartarus_*` names it uses must be in `docs/CLASSIC_CHECKLIST.md` with their Classic behaviour (CI fails otherwise).
- Run `python3 tools/check_xrefs.py` after editing docs (CI runs it with `--strict`): every mentioned ID must be defined, and `**Related**` links must be two-way. Placeholder IDs for future entries fail it; write "the next decision" instead.

## Out of scope — do not change without asking
- Anything under `editor/`, `legacy/`, `win32/`, `dd_grwin32/`.
- Upstream `LICENSE`, `THIRD_PARTY.md`.
- Stock weapon values in Classic Mode.
- Multiplayer protocol (D-008).
- Renaming the project (D-002 is Proposed; owner decides).
- Adding dependencies (licence review per D-003 first).
