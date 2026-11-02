# Attack Vectors

Project-specific failure modes the project must be resilient against.
Grouped by category. Each vector lists detection method and severity.
Severity: Critical (must hold) | Major (regression on release blocks) | Minor (track only).

## Fidelity

### AV-001 Tartarus behaviour leaking into Classic Mode
**Severity:** Critical
**Description.** A new HUD element, keybind, weapon, AI change or physics tweak is active while `classic 1`. Breaks the core promise and, for multiplayer, wire compatibility with stock clients.
**Detection.** `tools/classic_audit.py` (CI, "Tartarus audits" workflow) fails unless every upstream file with a `TARTARUS:` marker and every `tartarus::` function or `Tartarus_*` global used there is listed, with its Classic behaviour, in the hook table of `docs/CLASSIC_CHECKLIST.md`; so a new hook cannot land without a Classic review. The same file holds the manual checklist (run before each release and after hook changes) and a log of runs. (A grep for missing `IsClassic()` early-outs was the original idea; rejected as too easy to fool.)
**Related decisions.** D-004, D-015 (the audit runs in CI on every push).
**History.** Identified at design time 2026-11-01. Audit and checklist added and first run passed 2026-11-02 (two small fixes on the way, see the checklist's run log).

### AV-002 Upstream merge drift
**Severity:** Major
**Description.** Hook points multiply or upstream refactors the files they live in (e.g. `GameLoop.cpp`, `WeaponFire.cpp`), making merges expensive until the fork silently stops tracking upstream and loses fixes.
**Detection.** Monthly `tools/upstream-sync.sh` run; it reports conflicts and fails if any conflict is outside `Descent3/tartarus/` or a file in ARCHITECTURE.md §Hook points. Grep count of `// TARTARUS:` markers is logged in CHANGELOG per release.
**Related decisions.** D-001.
**History.** —

### AV-003 Save/demo format breakage
**Severity:** Major
**Description.** Adding fields to upstream structs (`player`, `object`, `weapon`) changes save-game and demo layouts; existing saves fail to load or load corrupt.
**Detection.** Invariant 2 (side tables, not struct edits) enforced by review; `tests/save_roundtrip_test` loads a fixture save from upstream and from Tartarus and compares player position/energy/shields. Not implemented until Phase 1.
**Related decisions.** D-004 (mode stamp in saves).
**History.** —

## Integrity

### AV-004 Cheats leaking into multiplayer or scores
**Severity:** Major
**Description.** A cheat toggle survives into a multiplayer game, or an unlock path forgets to set `IsCheater`, letting cheated progress count.
**Detection.** Unit test over the unlock table asserting every row sets `IsCheater` (`CheatTable.EveryRowButCatharsisMarksCheater`, the relock row being the one exception); manual: start a LAN game after unlocking and confirm all toggles are off and the menu refuses. Code review of every `CHEAT`-flagged cvar's set path.
**Related decisions.** D-006, D-008.
**History.** —

### AV-005 Game data committed to the repository
**Severity:** Critical
**Description.** A HOG, mission, movie, extracted texture or table file lands in git, infringing Outrage/Interplay rights and poisoning every clone.
**Detection.** Pre-commit hook rejecting `CLAUDE.local.md`, `*.hog`, `*.mn3`, `*.d3l`, `*.gam`, `*.mve`, `*.ogv`, `*.oof`, `*.ogf`, `*.pld` and any file > 5 MB outside `third_party/`; `.gitignore` for the same; CI runs the same check. Exempt: upstream's own tracked, GPL-released files in `scripts/data/demohog/`, `scripts/data/fullhog/` (HogMaker inputs for `d3-linux.hog`) and `cfile/tests/TestDir/` (test fixture); the hook must allow these paths, and merges from `upstream` that touch only them.
**Related decisions.** D-003, D-007.
**History.** —

### AV-012 GPL compliance of binary releases
**Severity:** Major
**Description.** A published binary without a matching source tag, or a dependency with an incompatible licence (candidate scripting libraries, audio codecs).
**Detection.** Release checklist item; `THIRD_PARTY.md` reviewed on every new dependency.
**Related decisions.** D-003.
**History.** —

## Gameplay correctness

### AV-006 Gravity-well physics faults
**Severity:** Major
**Description.** Pulled objects clip through walls, jitter at the centre, accumulate NaN velocities, or stack unbounded force at high frame rates; stock `PF_DESTINATION_POS` pinning behaviour was the main risk and is excluded by design.
**Detection.** `tests/gravwell_test` (headless physics step with a wall between robot and well: robot must end on its own side); console `r_showcollide 1` during tuning; force clamp `F ≤ gw_force` asserted in Debug builds; NaN guard on every `phys_apply_force` from Tartarus code.
**Related decisions.** D-009.
**Related features.** F-021, F-022.
**History.** —

### AV-007 Cloak / AI inconsistency
**Severity:** Major
**Description.** Robots never regain target after decloak (invisibility flag not cleared on a forced break), or never lose it (flag set but AI path not using it), or a cloak persists across death/level load.
**Detection.** State-machine unit test for every transition clearing/setting the invisibility flag; manual: decloak in front of a robot and confirm it fires within 1 s; level transition with cloak active returns to READY.
**Related features.** F-020, F-070.
**History.** —

### AV-010 Weapon slot collisions
**Severity:** Major
**Description.** Sidecar weapons collide with stock names in `FindWeaponName()`, exceed `MAX_WEAPONS` (200), or claim a player weapon slot already used by a stock weapon, corrupting the weapon lists.
**Detection.** Loader refuses duplicates and logs; `tests/sidecar_test` loads the shipped file against the stock name list; `cvarlist`/`give` reflect the loaded set.
**Related decisions.** D-007.
**History.** —

### AV-011 Auto-level fighting the player
**Severity:** Minor
**Description.** Snap/hold/strength settings override active roll input or oscillate near upright, producing a nauseating or sluggish feel.
**Detection.** Rule "any roll input cancels" asserted in code; playtest item in Phase 3 acceptance; `al_strength 0` must be bit-identical to upstream behaviour (recorded-input comparison).
**Related decisions.** D-010.
**History.** —

## Engineering

### AV-008 Console capturing or leaking input
**Severity:** Major
**Description.** Console open: game still receives keys (ship fires while typing) or mouse look continues; console closed: a stuck key state leaves the ship thrusting or the backtick character appears in chat/pilot-name fields.
**Detection.** Manual checklist; automated SDL event injection test in Phase 1 if cheap.
**Related features.** F-010.
**History.** —

### AV-013 Virtual-joystick offset stuck or drifting
**Severity:** Major
**Description.** In virtual-joystick mode (F-067) the ship turns whenever the stored mouse offset is outside the dead zone. If the offset survives a context change (console or menu opened, alt-tab, death, level load, pause), the ship keeps spinning with no input, or the indicator and actual rate disagree.
**Detection.** Unit test of the offset model (clamp, curve, decay, every reset trigger; `VirtualJoystick.*`, `VjCurve.*`); manual checklist: offset to full rate, then open console / menu / automap / alt-tab / die / load a level, and confirm the ship is still and the indicator centred on return.
**Related decisions.** D-013.
**Related features.** F-067.
**History.** Identified at design time 2026-11-01.

### AV-009 Per-frame cost of new HUD/overlay code
**Severity:** Minor
**Description.** Orientation aids, console scrollback rendering or debug overlays add measurable frame time even when disabled (allocations, string formatting).
**Detection.** `HOURGLASS` stats overlay before/after; budget in SPEC.md §9 (≤ 0.2 ms); Debug-build assertion that disabled items perform no work (counter must be zero).
**Related features.** F-013, F-040 – F-042.
**History.** —
