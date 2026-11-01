# Roadmap

Phases are append-only; mark Complete with an ISO date. Feature IDs refer to FEATURES.md.

## Phase 0 — Fork and baseline
**Goal:** A clean fork that builds and plays stock Descent 3 natively on the reference Linux machine, with upstream credit in place and a repeatable upstream-merge path.
**Status:** Not started
**Features delivered:** F-001, F-003, F-004
**Deliverables:**
- [ ] Fork created from **https://github.com/DescentDevelopers/Descent3** (fork on GitHub, then locally):
  ```bash
  git clone --recurse-submodules https://github.com/Darian-Frey/tartarus
  cd tartarus
  git remote add upstream https://github.com/DescentDevelopers/Descent3
  git fetch upstream
  ```
- [ ] `tools/upstream-sync.sh` (merges `upstream/main`)
- [ ] Reference build on Linux Mint per BUILD.md; `BUILD.md` §Troubleshooting seeded with anything hit
- [ ] Steam data-dir detection (`Descent3/tartarus/datadir.cpp`)
- [ ] Main-menu build string with upstream hash; README credits; `THIRD_PARTY.md` carried forward
- [ ] Play levels 1–3 and the first Mercenary level; log any upstream-inherited issues in BUGS.md as `deferred`
- [ ] First CHANGELOG entry
**Acceptance:** `git merge upstream/main` on a fresh day yields zero conflicts outside `Descent3/tartarus/`, and the game completes level 1 from a Steam install without manual file copying.

## Phase 1 — Developer foundation
**Goal:** The tools every later phase depends on: console, cvars, Classic Mode gate, cheat menu, debug overlays.
**Status:** Not started
**Features delivered:** F-002, F-010, F-011, F-012, F-013
**Deliverables:**
- [ ] `cvar.cpp/.h` with flags, persistence to `tartarus.cfg`, `CLASSIC_LOCKED` enforcement
- [ ] `console.cpp/.h`: input capture, rendering via `grtext`, history, completion, plog sink
- [ ] `classic.cpp`: the single `IsClassic()` query and the Options toggle
- [ ] `cheatmenu.cpp`: password field, unlock table, per-frame "hold at max" loop, HUD "CHEATS" tag, `IsCheater` integration
- [ ] Debug overlays (`r_showaxes`, `r_showobjinfo`, `r_showcollide`)
- [ ] AV-001 Classic-regression checklist written and run once
**Acceptance:** Every cheat in SPEC.md §Cheat passwords works from both menu and console; `classic 1` hides all of it; `cvarlist` shows every SPEC constant.

## Phase 2 — Orientation and automap
**Goal:** Pilots can always tell which way is up, and the map re-anchors them.
**Status:** Not started
**Features delivered:** F-040, F-041, F-042, F-043, F-050, F-051
**Deliverables:**
- [ ] `orient_hud.cpp`: up-tick, attitude ball, gizmo; HUD item registration in `hud.cpp`
- [ ] Auto-level extensions in `physics/physics.cpp` behind cvars; hold and snap keys in controls config
- [ ] Automap: visited/seen colouring, filters, 2D mode, orientation glyph in `TelComAutoMap.cpp`
- [ ] Playtest notes on defaults (D-010) recorded in DECISIONS if defaults change
**Acceptance:** A new player test (someone unfamiliar with Descent) can complete level 2 without asking which way is up; the orientation aids cost < 0.2 ms/frame at 1080p.

## Phase 3 — Tactical weapons I
**Goal:** The two signature mechanics plus the free wins: cloak, gravity missile, repulsor, converter.
**Status:** Not started
**Features delivered:** F-020, F-021, F-022, F-030
**Deliverables:**
- [ ] Sidecar weapon definitions (`data/tartarus/weapons.txt`) loaded via `AllocWeapon()` (D-007)
- [ ] `cloak.cpp`: state machine, energy accounting, break hooks in `WeaponFire.cpp` and `physics/collide.cpp`, HUD gauge
- [ ] `gravwell.cpp`: new fireball index, pull loop with falloff and mass scaling, `GW_IMMUNE` robot flag, visuals
- [ ] Repulsor as a sign-flipped gravity well
- [ ] Converter keys and HUD feedback
- [ ] Tuning session via console cvars; final values written to SPEC.md
**Acceptance:** The "cloak → fire gravity missile round a corner → enemy dragged into view as cloak drops" sequence works reliably on at least three stock levels, and robot pull never clips a robot through a wall (AV-006 test).

## Phase 4 — Modernisation
**Goal:** The engine feels like a 2026 native Linux game.
**Status:** Not started
**Features delivered:** F-060, F-061, F-062, F-064 (F-063 if time allows)
**Deliverables:**
- [ ] Widescreen HUD anchoring and `r_fov`
- [ ] Raw mouse input
- [ ] Quicksave/quickload
- [ ] Fixed-timestep physics prototype behind `phys_fixedstep` cvar; recorded-input equivalence test; D-012 resolved to Accepted or Deprecated
**Acceptance:** Identical recorded-input runs at 60 and 240 Hz diverge by < 1 unit of position after 60 s; no HUD element is clipped at 21:9.

## Phase 5 — Tactical weapons II and AI
**Goal:** Round out the arsenal and make robots react to it.
**Status:** Not started
**Features delivered:** F-023 – F-028, F-031, F-032, F-044, F-045, F-070
**Deliverables:**
- [ ] Each weapon as its own sidecar entry plus a small behaviour module
- [ ] AI hooks: target-selection override (decoy), stunned state (EMP), gravity-escape goal, decloak-investigate
**Acceptance:** Each weapon has at least one level where it is the obviously right tool; AI changes are invisible in Classic Mode.

## Phase 6 — Platform
**Goal:** Turn the fork into something others can build on.
**Status:** Not started
**Features delivered:** F-065, F-071, F-080, F-081
**Deliverables:**
- [ ] Scripting layer design doc in `docs/`, then implementation
- [ ] Mission browser
- [ ] Demo free camera
- [ ] CONTRIBUTING.md and SECURITY.md added (Tier 3 trigger: external contributors)
**Acceptance:** A third party defines a new weapon without touching C++.
