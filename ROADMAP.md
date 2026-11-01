# Roadmap

Phases are append-only; mark Complete with an ISO date. Feature IDs refer to FEATURES.md.

## Phase 0 — Fork and baseline
**Goal:** A clean fork that builds and plays stock Descent 3 natively on the reference Linux machine, with upstream credit in place and a repeatable upstream-merge path.
**Status:** Acceptance met 2026-11-01; one deliverable open (levels 2–3 and the first Mercenary level)
**Features delivered:** F-001, F-003, F-004
**Deliverables:**
- [x] Fork created from **https://github.com/DescentDevelopers/Descent3** (fork on GitHub, then locally):
  ```bash
  git clone --recurse-submodules https://github.com/Darian-Frey/tartarus
  cd tartarus
  git remote add upstream https://github.com/DescentDevelopers/Descent3
  git fetch upstream
  ```
  Done 2026-11-01 by merging `upstream/main` (`593b880f`) into the existing docs repo with `--allow-unrelated-histories`; upstream's README/BUILD/CHANGELOG moved to `docs/UPSTREAM_*.md`.
- [x] `tools/upstream-sync.sh` (merges `upstream/main`) — 2026-11-01, IMP-002
- [x] Reference build on Linux Mint per BUILD.md; `BUILD.md` §Troubleshooting seeded with anything hit — 2026-11-01 (vcpkg needs `autoconf-archive`; skip `libjack-dev`)
- [x] Steam data-dir detection (`Descent3/tartarus/datadir.cpp`) — 2026-11-01; multi-install prompt deferred to Phase 1 (F-003 notes)
- [x] Main-menu build string with upstream hash; README credits; `THIRD_PARTY.md` carried forward — 2026-11-01
- [ ] Play levels 1–3 and the first Mercenary level; log any upstream-inherited issues in BUGS.md as `deferred`
- [x] First CHANGELOG entry
**Acceptance:** `git merge upstream/main` on a fresh day yields zero conflicts outside `Descent3/tartarus/`, and the game completes level 1 from a Steam install without manual file copying.
**Acceptance status (2026-11-01):** met. The game starts and loads level 1 from the Steam install with no flags; the owner first could not finish level 1 with the stock mouse model, then completed it with the F-067 virtual-joystick mouse (D-013).

## Phase 1 — Developer foundation
**Goal:** The tools every later phase depends on: console, cvars, Classic Mode gate, cheat menu, debug overlays.
**Status:** In progress
**Features delivered:** F-002, F-010, F-011, F-012, F-013
**Deliverables:**
- [x] `cvar.cpp/.h` with flags, persistence to `tartarus.cfg`, `CLASSIC_LOCKED` enforcement — 2026-11-01
- [ ] `console.cpp/.h`: input capture, rendering via `grtext`, history, completion, plog sink (all done and verified in level 1, 2026-11-01; cheat commands done 2026-11-01; `bind`/`unbind`, `noclip`, `spawn` pending)
- [ ] `classic.cpp`: the single `IsClassic()` query and the Options toggle (query, cvar and `-classic`/`-tartarus` done 2026-11-01; toggle comes with the console now and F-069 in Phase 2)
- [ ] `cheats.cpp` + `cheat_table.cpp`: password field, unlock table, per-frame "hold at max" loop, HUD "CHEATS" tag, `IsCheater` integration (done and verified in level 1, 2026-11-01; WRAITH, ARGUS, countermeasure hold and the AV-004 LAN check pending)
- [ ] Debug overlays (`r_showaxes`, `r_showobjinfo`, `r_showcollide`)
- [ ] AV-001 Classic-regression checklist written and run once
**Acceptance:** Every cheat in SPEC.md §Cheat passwords works from both menu and console; `classic 1` hides all of it; `cvarlist` shows every SPEC constant.

## Phase 2 — Controls and display
**Goal:** The game flies comfortably with a mouse and looks right on a modern widescreen display, with one options menu that holds all of it.
**Status:** In progress
**Features delivered:** F-060, F-062, F-063, F-067, F-068, F-069
**Deliverables:**
- [x] Mouse flight modes (`Descent3/tartarus/mouse_flight.cpp`): virtual-joystick offset model as an analogue axis source, indicator, recentre key, AV-013 resets and unit tests — 2026-11-01; the owner completed level 1 with it
- [x] Options (F2, in game and from the main menu) → **Mouse** tab: mode, speed, centre speed, invert pitch, indicator, roll; the first Tartarus page of the options menu (F-067, F-069; requested by the owner) — 2026-11-01
- [ ] Raw mouse: no acceleration or smoothing, one sensitivity (F-062)
- [ ] Widescreen: correct 3D aspect without `-aspect` (already true upstream, checked 2026-11-01), `r_fov` 60–120 (done 2026-11-01), HUD anchoring (fine at 16:9/16:10), menus centred with blurred bar fill (done 2026-11-01; full-width menu canvas moved to F-069) (F-060)
- [ ] Small-view windows (rear view, Guide-Bot, missile cam): per-window size, aspect and position cvars, defaults identical to upstream (F-060, SPEC.md §14)
- [ ] Graphics options investigated against `renderer/HardwareOpenGL.cpp`, then implemented as `r_` cvars (F-068)
- [ ] Options menu with Graphics / Controls / Audio / Gameplay / Tartarus pages generated from cvar metadata (F-069)
- [ ] SDL3 gamepad with gyro if time allows (F-063)
- [ ] Tuning session for SPEC.md §13 defaults; values written back to SPEC
**Acceptance:** The owner completes level 1 with the mouse in virtual-joystick mode (which also closes Phase 0); no HUD element or menu is clipped or stretched at 16:9, 16:10 and 21:9; every new option is reachable from the Options menu and the console; Classic Mode still flies exactly like upstream.

## Phase 3 — Orientation and automap
**Goal:** Pilots can always tell which way is up, and the map re-anchors them.
**Status:** Not started
**Features delivered:** F-040, F-041, F-042, F-043, F-050, F-051
**Deliverables:**
- [ ] `orient_hud.cpp`: up-tick, attitude ball, gizmo; HUD item registration in `hud.cpp`
- [ ] Auto-level extensions in `physics/physics.cpp` behind cvars; hold and snap keys in controls config
- [ ] Automap: visited/seen colouring, filters, 2D mode, orientation glyph in `TelComAutoMap.cpp`
- [ ] Playtest notes on defaults (D-010) recorded in DECISIONS if defaults change
**Acceptance:** A new player test (someone unfamiliar with Descent) can complete level 2 without asking which way is up; the orientation aids cost < 0.2 ms/frame at 1080p.

## Phase 4 — Tactical weapons I
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

## Phase 5 — Modernisation
**Goal:** The engine feels like a 2026 native Linux game.
**Status:** Not started
**Features delivered:** F-061, F-064 (F-060, F-062, F-063 moved to Phase 2 by D-013)
**Deliverables:**
- [ ] Quicksave/quickload
- [ ] Fixed-timestep physics prototype behind `phys_fixedstep` cvar; recorded-input equivalence test; D-012 resolved to Accepted or Deprecated
**Acceptance:** Identical recorded-input runs at 60 and 240 Hz diverge by < 1 unit of position after 60 s.

## Phase 6 — Tactical weapons II and AI
**Goal:** Round out the arsenal and make robots react to it.
**Status:** Not started
**Features delivered:** F-023 – F-028, F-031, F-032, F-044, F-045, F-070
**Deliverables:**
- [ ] Each weapon as its own sidecar entry plus a small behaviour module
- [ ] AI hooks: target-selection override (decoy), stunned state (EMP), gravity-escape goal, decloak-investigate
**Acceptance:** Each weapon has at least one level where it is the obviously right tool; AI changes are invisible in Classic Mode.

## Phase 7 — Platform
**Goal:** Turn the fork into something others can build on.
**Status:** Not started
**Features delivered:** F-065, F-071, F-080, F-081
**Deliverables:**
- [ ] Scripting layer design doc in `docs/`, then implementation
- [ ] Mission browser
- [ ] Demo free camera
- [ ] CONTRIBUTING.md and SECURITY.md added (Tier 3 trigger: external contributors)
**Acceptance:** A third party defines a new weapon without touching C++.
