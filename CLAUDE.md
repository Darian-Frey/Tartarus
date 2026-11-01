# CLAUDE.md

## Project
Tartarus (working title, D-002) is a Linux-first fork of the GPL-3.0 Descent 3 engine (DescentDevelopers/Descent3) that keeps stock gameplay behind a Classic Mode flag and adds tactical features: developer console + cvars, password-gated cheat menu, player cloak, gravity missile, repulsor, orientation aids, automap overhaul, and modernisation (FOV, raw mouse, quicksave, fixed timestep). All additions live in `Descent3/tartarus/` with a short list of marked hook points.

## Current state
- Upstream **https://github.com/DescentDevelopers/Descent3** is the `upstream` remote and was merged into `main` on 2026-11-01 at `593b880f` (`--allow-unrelated-histories`; repo is not a GitHub fork). Upstream's own README/BUILD/CHANGELOG live in `docs/UPSTREAM_*.md`; ours are at the root. `.gitignore` = upstream's file unchanged + a Tartarus block at the end. First Tartarus code: `Descent3/tartarus/` has datadir (F-003) and version (F-004), wired in via `tartarus.cmake`; 6 `// TARTARUS:` markers in `Descent3/CMakeLists.txt`, `init.cpp`, `mmItem.cpp`. Reference build and the 7 unit tests pass; the game reaches the main menu from the Steam install with no flags. Documents: README, FEATURES, ROADMAP, ARCHITECTURE, DECISIONS (D-001–D-012), SPEC, ATTACK_VECTORS (AV-001–AV-012), BUILD, BUGS, IMPROVEMENTS, CHANGELOG.
- Upstream reference for all file/symbol names in the docs: commit `fa61eec` (2026-11-01); the merged `593b880f` only adds netgames/netcon logging on top. Upstream is on SDL3 (1.6.0-dev); its own `BUILD.md`/`USAGE.md` are authoritative for build mechanics.
- Verified upstream facts the design relies on: `GameCheat.cpp` typed-sequence cheats and `IsCheater`; `MakeObjectInvisible()` in `object_lighting.h`; `GRAVITY_FIELD_INDEX` / `DoGravityFieldEffect` in `fireball.cpp`; `phys_apply_force()` in `physics/physics.cpp`; autolevel block in `physics/physics.cpp` (`MAX_AUTOLEVEL_TILT_ANGLE`); `AllocWeapon()`/`FindWeaponName()` in `weapon.cpp`, `MAX_WEAPONS 200`; `HUD_ITEM_*` in `hud.h`; `TelComAutoMap.cpp`; `MAX_ENERGY/MAX_SHIELDS 200`.

## Active task
**Phase 0 — Fork and baseline** (ROADMAP.md). Acceptance: upstream merges cleanly outside `Descent3/tartarus/`; game completes level 1 from a Steam install without copying files.
1. ~~Fork; `upstream` remote; `tools/upstream-sync.sh`~~ (done 2026-11-01).
2. ~~Reference build~~ (done; vcpkg in `~/vcpkg`, pass `VCPKG_ROOT=$HOME/vcpkg` if not in the shell).
3. ~~F-003 probe, F-004 build string~~ (done; F-003 multi-install prompt deferred to Phase 1).
4. **Remaining:** owner plays levels 1–3 and the first Mercenary level; log inherited issues in BUGS.md as `deferred`.
Then Phase 1 (console/cvars/classic gate/cheat menu) — D-005 says tooling before any gameplay feature.

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
cmake --preset linux -DBUILD_TESTING=ON && cmake --build --preset linux --config Debug && ctest --test-dir builds/linux/build --output-on-failure
./builds/linux/build/Release/Descent3 -windowed -logfile   # Steam data found automatically (F-003); log → ./Descent3.log
```

## Local environment
Machine-specific facts (the owner's Descent 3 install path, Steam library layout, GPU quirks) live in **`CLAUDE.local.md`**, which is gitignored and never committed. Read it at session start if present. The repo documents only *which* files are needed (BUILD.md §4a), never where they are on anyone's machine.

## Conventions
- British English in docs and UI strings. ISO 8601 dates.
- C++20 as upstream; follow upstream `.clang-format`. New files: `snake_case.cpp`, functions `PascalCase` to match the surrounding engine, cvars `snake_case` with area prefix (SPEC §2).
- Commit messages: multi-paragraph; reference IDs (`F-021`, `D-009`, `AV-006`, `BUG-003`); first line ≤ 72 chars. One Subtle Chaos anomaly per commit body is the owner's house style — leave it to him unless asked.
- Stable IDs are append-only; status flags, never deletion.
- Maintenance Rule 8: when you notice a bug or a refactor opportunity while doing something else, **log it** in BUGS.md / IMPROVEMENTS.md; do not silently fix or apply.
- Docs change in the same commit as the code they describe. Refresh this file at the end of every significant session (state, not history).

## Pitfalls
- `-setdir`/`-useexedir` no longer exist upstream (BUG-001); the writable dir is SDL's pref path `~/.local/share/Outrage Entertainment/Descent 3/`. The current working directory is also a read-only base dir in portable builds.
- ctest names tests by gtest suite (`SteamTree.*`), not by executable; filter with `-R` on suite names.
- See ATTACK_VECTORS.md for the canonical list.
- The console key intercept must run *before* `DemoCheats(key)` in `GameLoop.cpp` or typed cheats eat console input and vice versa (AV-008).
- The stock gravity field pins objects with `PF_DESTINATION_POS` — don't copy that line (D-009).
- `MakeObjectInvisible` has a `no_hud_message` parameter; pass `true` for the cloak or every activation prints the powerup message.
- Weapon table is binary and editor-only on Windows; new weapons go through the sidecar (D-007). Check `MAX_WEAPONS` and `FindWeaponName` collisions (AV-010).
- The Linux Steam depot ships `.ogv` movies, which upstream cannot decode; cutscenes are simply skipped (owner's choice, 2026-11-01). Don't add an OGV decoder or fetch the Windows depot without asking.
- Cross-references between docs are maintained by hand until `tools/check_xrefs.py` exists (Phase 1); assume some may be stale.

## Out of scope — do not change without asking
- Anything under `editor/`, `legacy/`, `win32/`, `dd_grwin32/`.
- Upstream `LICENSE`, `THIRD_PARTY.md`.
- Stock weapon values in Classic Mode.
- Multiplayer protocol (D-008).
- Renaming the project (D-002 is Proposed; owner decides).
- Adding dependencies (licence review per D-003 first).
