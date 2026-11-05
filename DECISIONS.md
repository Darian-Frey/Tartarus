# Decisions

Append-only log of significant design decisions.
Each entry: D-NNN, with Decided and Recorded dates (ISO 8601; equal for normal entries, divergent for retroactive ones), status, context, alternatives, decision, consequences, and reversal conditions.
Status vocabulary: Proposed | Accepted | Superseded by D-NNN | Deprecated.

### D-001 Fork upstream DescentDevelopers/Descent3 and track it as a remote
**Decided:** 2026-11-01
**Recorded:** 2026-11-01
**Status:** Accepted
**Authors:** Shane Hartley (with Claude design discussion 2026-11-01)
**Related:** F-001, F-004, AV-002, D-014

**Context.** Descent 3 has never run well for the owner on Linux (Steam Windows build under Proton). The engine source is GPL-3.0 and already builds natively on Linux via CMake/SDL3 with CI. The question was whether to start from the 2024 source drop, from upstream's current `main`, or from scratch.

**Options.**
- **A. Fresh reimplementation.** Rejected: years of work before parity; the point is to add features, not rewrite a 1999 engine.
- **B. Fork from the original April 2024 source drop.** Rejected: loses 64-bit, SDL3, CMake, std::filesystem and dozens of bug fixes already done upstream.
- **C. Fork upstream `main`, keep `upstream` as a remote, merge regularly, confine changes to a subdirectory plus marked hook points.** Chosen.

**Decision.** Option C. Upstream is https://github.com/DescentDevelopers/Descent3 (added as git remote `upstream`); it remains the source of truth for the engine and Tartarus is a thin, mergeable layer.

**Consequences.**
- Linux port effort is essentially zero; effort goes to features.
- Must keep hook points few and marked (ARCHITECTURE.md §Hook points) or merges become painful.
- Upstream's SDL3 migration (1.6.0-dev) is inherited; breaking changes there arrive on merge.

**Reversal conditions.** Revisit if upstream merges start costing more than a day a month, or upstream changes licence/governance in a way that blocks the fork.

### D-002 Working name "Tartarus"
**Decided:** 2026-11-01
**Recorded:** 2026-11-01
**Status:** Proposed
**Authors:** Claude (suggestion); awaiting Shane Hartley's confirmation
**Related:** README.md

**Context.** The owner names projects after Greek/Latin primordial figures. The game is about descending into mines; Tartarus is the primordial abyss beneath the underworld in Hesiod — a literal "descent" destination. Names already in use elsewhere in the owner's portfolio (Erebus, Charon, Nyx, Daedalus) were excluded.

**Options.**
- **A. Tartarus.** Primordial, thematically exact, unused. Chosen as working title.
- **B. Katabasis.** The Greek term for the descent into the underworld; evocative but a concept, not a primordial figure, and harder to say.
- **C. Keep "Descent 3 Linux fork".** Rejected: generic, and may read as an official continuation.

**Decision.** Use Tartarus as the working title in all documents until confirmed or replaced.

**Consequences.**
- Repo, cfg file (`tartarus.cfg`), source directory and cvar prefixes all use the name; a rename before Phase 1 is cheap, after it is a search-and-replace plus config migration.

**Reversal conditions.** Owner picks a different name, or a trademark/collision search turns up a conflicting game project.

### D-003 Licence: GPL-3.0 inherited; credit upstream prominently
**Decided:** 2026-11-01
**Recorded:** 2026-11-01
**Status:** Accepted
**Authors:** Shane Hartley
**Related:** F-004, AV-012, AV-005

**Context.** Upstream is GPL-3.0; a derivative must be too. The owner also wants explicit credit to the original creators and maintainers.

**Options.**
- **A. GPL-3.0, carry `LICENSE` and `THIRD_PARTY.md` unmodified, credit in README and main menu.** Chosen (the only compliant option, plus the credit the owner wanted).
- **B. Dual-licence Tartarus-only files.** Rejected: pointless complexity; everything links into one GPL binary.

**Decision.** Option A.

**Consequences.**
- Any binary release must link to the exact source tag.
- No proprietary or non-GPL-compatible dependencies (e.g. some Lua bindings' licences need checking before F-080).

**Reversal conditions.** None foreseeable; GPL inheritance is not optional.

### D-004 Classic Mode is a runtime flag, not a branch or second binary
**Decided:** 2026-11-01
**Recorded:** 2026-11-01
**Status:** Accepted
**Authors:** Shane Hartley (with Claude design discussion 2026-11-01)
**Related:** F-002, AV-001, AV-003

**Context.** The owner wants "just classic Descent 3, optimised" available alongside the new features.

**Options.**
- **A. Separate `classic` git branch.** Rejected: two codebases to merge upstream into; features and fixes drift.
- **B. Compile-time `#ifdef`.** Rejected: two binaries to ship and test; can't switch in-game.
- **C. Single runtime flag `classic` consulted by every Tartarus module, plus `CLASSIC_LOCKED` cvars.** Chosen.

**Decision.** Option C, default on for first run.

**Consequences.**
- Every Tartarus code path needs the gate; a missed gate is a bug class (AV-001) with its own checklist.
- Performance and compatibility fixes (F-060-class items that don't change gameplay) remain active in Classic; the line between "optimisation" and "change" is adjudicated per feature in FEATURES notes.
- Saves carry the mode stamp.

**Reversal conditions.** If the gate count exceeds ~50 sites or Classic regressions keep escaping, revisit a compile-time split.

### D-005 Developer console and cvar system are built first
**Decided:** 2026-11-01
**Recorded:** 2026-11-01
**Status:** Accepted
**Authors:** Shane Hartley (with Claude design discussion 2026-11-01)
**Related:** F-010, F-011, ROADMAP Phase 1, D-013

**Context.** Every gameplay feature needs live tuning (cloak costs, gravity radius, autolevel strength). Upstream has plog logging and a typed-sequence cheat system but no console or cvar registry.

**Options.**
- **A. Tune by recompiling.** Rejected: slow, and leaves no record of what values were tried.
- **B. Build console + cvars before any gameplay feature.** Chosen.

**Decision.** Option B. Phase 1 is tooling; Phase 3 onward consumes it.

**Consequences.**
- Delays the first visible gameplay change by one phase.
- Cheat menu and debug overlays ride on the same infrastructure for free.

**Reversal conditions.** None; sunk once built.

### D-006 Cheats: password-gated menu, plaintext passwords, upstream `IsCheater` semantics
**Decided:** 2026-11-01
**Recorded:** 2026-11-01
**Status:** Accepted
**Authors:** Shane Hartley (with Claude design discussion 2026-11-01)
**Related:** F-012, SPEC.md §Cheat passwords, AV-004

**Context.** Upstream cheats are typed letter sequences matched against `jcrypt`-obfuscated strings in `GameCheat.cpp`. The owner wants a visible menu where a password unlocks each option (give all, god mode, infinite energy/missiles/health, etc.).

**Options.**
- **A. Extend the upstream typed-sequence system.** Rejected: invisible UI, obfuscation is meaningless in a GPL repo, and infinite toggles need persistent state the old system lacks.
- **B. Menu with a password field; passwords as plain constants in a table; unlocked state per pilot session; same passwords accepted by the console `cheat` command.** Chosen.
- **C. Hash the passwords.** Rejected: theatre — the source is public.

**Decision.** Option B. Unlocking sets upstream `IsCheater` so progress/score handling is unchanged. Cheats refused in multiplayer and Classic Mode. Upstream sequence cheats remain available in Classic Mode untouched.

**Consequences.**
- Passwords are a design asset, not a secret; choose memorable themed words (SPEC.md).
- "Infinite" cheats hold values at max each frame rather than refilling once.
- `IsCheater` is already respected by `pilot.cpp` mission progress and `Player.cpp` scoring, so no new plumbing there.

**Reversal conditions.** If a future multiplayer protocol needs server-authoritative cheat control, add it as a new decision rather than changing this one.

### D-007 New weapons defined in a fork-owned sidecar file, not by editing `table.gam`
**Decided:** 2026-11-01
**Recorded:** 2026-11-01
**Status:** Accepted
**Authors:** Shane Hartley (with Claude design discussion 2026-11-01)
**Related:** F-021 – F-028, ARCHITECTURE.md §weapons_sidecar, AV-010, AV-005

**Context.** Weapon stats live in the binary table file loaded by `manage/`. The authoring tool is the MFC level editor, which upstream describes as Windows-only, 32-bit and unusable. Shipping a modified `table.gam` would also mean redistributing Outrage data.

**Options.**
- **A. Patch `table.gam` with a custom tool and ship it.** Rejected: redistributes game data; opaque diffs; editor unavailable on Linux.
- **B. Hard-code new weapons in C++.** Workable but every tweak is a rebuild and it blocks F-080 scripting later.
- **C. Text sidecar `data/tartarus/weapons.txt` parsed after table load, allocating slots via `AllocWeapon()` and referencing existing HOG assets (models, sounds, bitmaps) by name.** Chosen.

**Decision.** Option C. Behaviour not expressible by existing weapon flags is implemented in C++ modules selected by Tartarus-only flag bits.

**Consequences.**
- No new art required for v1 — weapons reuse stock models/effects with tinting/scaling; custom art can come later as GPL assets.
- `MAX_WEAPONS` is 200; stock uses well under that, but the sidecar must check the ceiling.
- Lays the groundwork for F-080 (the sidecar grammar becomes the scripting data model).

**Reversal conditions.** If upstream ships a working cross-platform table editor and a sanctioned mod-overlay mechanism for tables, prefer that.

### D-008 Single-player first; Tartarus features disabled in multiplayer until a protocol exists
**Decided:** 2026-11-01
**Recorded:** 2026-11-01
**Status:** Accepted
**Authors:** Shane Hartley
**Related:** F-002, F-012, F-020, F-021

**Context.** New weapons and player state are not understood by stock clients; cloak and gravity need netcode to replicate.

**Options.**
- **A. Design netcode alongside each feature.** Rejected: doubles the cost of every feature for an audience that doesn't exist yet.
- **B. Gate everything Tartarus on `!(Game_mode & GM_MULTI)`; Classic Mode remains wire-compatible.** Chosen.

**Decision.** Option B.

**Consequences.**
- Multiplayer Tartarus is a future phase with its own decision.
- Keeps AV-004 (cheat leakage into multiplayer) simple to enforce.

**Reversal conditions.** A community of Tartarus players wanting co-op/deathmatch with the new weapons.

### D-009 Gravity missile reuses the engine's gravity-field fireball machinery with a new index
**Decided:** 2026-11-01
**Recorded:** 2026-11-01
**Status:** Accepted
**Authors:** Shane Hartley (with Claude design discussion 2026-11-01)
**Related:** F-021, F-022, AV-006, SPEC.md §Gravity missile

**Context.** `fireball.cpp` already implements a timed pulling field (`GRAVITY_FIELD_INDEX`, `DoGravityFieldEffect`) used by a stock weapon via `WF_GRAVITY_FIELD`. It applies `phys_apply_force` with distance and mass scaling, then pins objects with `PF_DESTINATION_POS` and ends in a blast ring.

**Options.**
- **A. Set `WF_GRAVITY_FIELD` on the new missile and tune `gravity_size/time`.** Rejected: inherits the centre-pinning and the terminal explosion, both wrong for a utility weapon, and couples tuning to the stock weapon.
- **B. New fireball index with its own frame function modelled on the stock one, minus pinning and minus the terminal blast, with linear falloff and a `GW_IMMUNE` robot flag.** Chosen.
- **C. Write a bespoke attractor object type.** Rejected: more code for the same result.

**Decision.** Option B.

**Consequences.**
- Repulsor is the same function with the sign flipped and no spawned object.
- Wall-clipping risk is lower than the stock field but still needs the AV-006 test.

**Reversal conditions.** If the fireball system's object pool or lifetime handling proves limiting (e.g. need for several concurrent wells with different behaviours), move to Option C.

### D-010 Orientation-aid defaults: reticle up-tick on, everything else off
**Decided:** 2026-11-01
**Recorded:** 2026-11-01
**Status:** Superseded by D-017 (2026-11-05)
**Authors:** Shane Hartley (with Claude design discussion 2026-11-01)
**Related:** F-040 – F-043, AV-011, D-017

**Context.** Losing track of "up" is a known Descent frustration, but veterans prize the raw 6DOF feel.

**Options.**
- **A. All aids on by default.** Rejected: clutter; alienates veterans.
- **B. Only the least intrusive aid (reticle tick) on; attitude ball, gizmo, hold/snap, stronger autolevel opt-in.** Chosen.
- **C. All off.** Rejected: new players never discover them.

**Decision.** Option B. Classic Mode: all off (upstream autolevel slider unchanged).

**Consequences.**
- Phase 3 playtest may move the attitude ball to default-on; that would be a new decision.

**Reversal conditions.** Playtest evidence that the tick alone does not fix disorientation for new players.

### D-011 Adopt the Development Documentation Standard in full, including in-repo BUGS.md and IMPROVEMENTS.md
**Decided:** 2026-11-01
**Recorded:** 2026-11-01
**Status:** Accepted
**Authors:** Shane Hartley
**Related:** all standard documents

**Context.** Solo project with AI-assisted sessions and expected multi-week gaps; the owner's standard (project-scaffold) targets exactly this. Tier 1 plus Tier 2 all apply; no exemptions claimed. Tier 3: `CONTRIBUTING.md`/`SECURITY.md` deferred to Phase 7 when external contributors are invited; `CLAIMS.md` not applicable (no research claims); `BENCHMARKS.md` to be added when F-061 lands (performance becomes a stated requirement).

**Options.**
- **A. Full adoption.** Chosen.
- **B. Use GitHub Issues instead of BUGS.md.** Rejected: bug history should survive forge changes and be readable by AI sessions in-repo.

**Decision.** Option A.

**Consequences.**
- Maintenance Rule 8 applies from the first coding session: log bugs and improvements, don't silently fix.
- `tools/check_xrefs.py` is a Phase 1 deliverable so cross-references don't decay.

**Reversal conditions.** None.

### D-012 Fixed-timestep physics
**Decided:** —
**Recorded:** 2026-11-01
**Status:** Proposed
**Authors:** Claude (proposal); decision deferred to Phase 5 prototype
**Related:** F-061, ROADMAP Phase 5

**Context.** Upstream integrates with the variable `Frametime`. Modern high-refresh displays expose frame-rate-dependent behaviour. A fixed tick with interpolated rendering is the standard fix but may measurably change the feel of a 27-year-old game, which Classic Mode promises not to do.

**Options.**
- **A. Leave timing alone; cap render rate in Classic.** Safe; leaves high-Hz players with stutter.
- **B. Fixed tick everywhere.** Best determinism; risks altering Classic feel.
- **C. Fixed tick behind a cvar, default on in Tartarus mode and off in Classic, promoted to Classic only if a recorded-input equivalence test shows no behavioural difference.** Current proposal.

**Decision.** Deferred. Prototype Option C in Phase 5 and seal then.

**Consequences.**
- Nothing until the prototype exists.

**Reversal conditions.** n/a until Accepted.

### D-013 Add a "Controls & display" phase as Phase 2
**Decided:** 2026-11-01
**Recorded:** 2026-11-01
**Status:** Accepted
**Authors:** Shane Hartley (with Claude design discussion 2026-11-01)
**Related:** F-060, F-062, F-063, F-067, F-068, F-069, D-005, ROADMAP.md, AV-013, D-016

**Context.** In the Phase 0 playthrough the owner could not finish level 1: with the stock mouse model (mouse movement = rotation) sustained turns need repeated sweeps. The owner also wants widescreen, higher-resolution and further graphics options, and a menu able to hold them. These were Phase 4 (modernisation) or not planned at all, behind orientation aids and the new weapons.

**Options.**
- **A. Keep the order.** Rejected: the game is not comfortably playable for the owner until much later, and every playtest in between is distorted by the controls.
- **B. Do controls and display now, before Phase 1.** Rejected: dead zone, curve and graphics settings need live tuning and persistence, which Phase 1's console and cvars provide (D-005).
- **C. Insert a new Phase 2 "Controls & display" after Phase 1; old Phases 2–6 become 3–7.** Chosen.

**Decision.** Option C. Phase 2 delivers F-060 (now including widescreen menus), F-062, F-063, and new F-067 (mouse flight modes), F-068 (advanced graphics options), F-069 (options menu overhaul). Fixed-timestep physics (F-061) and quicksave (F-064) stay in the modernisation phase, now Phase 5. Phase numbers in all documents were renumbered on this date (CHANGELOG entries are left as written).

**Consequences.**
- Phase 0's "complete level 1" check stays open until F-067 lands, or the owner completes it with keyboard or gamepad.
- Display options are performance/compatibility and stay available in Classic Mode; mouse flight modes change feel and are `CLASSIC_LOCKED`.
- Orientation aids and tactical weapons move back one phase.

**Reversal conditions.** If Phase 1 slips badly, a minimal virtual-joystick mode with hard-coded defaults can be pulled ahead of it as a stop-gap.

### D-014 Continuous integration builds Linux only
**Decided:** 2026-11-01
**Recorded:** 2026-11-01
**Status:** Accepted
**Authors:** Shane Hartley
**Related:** D-001, BUG-009, ARCHITECTURE.md §Hook points, D-015

**Context.** Upstream's GitHub Actions matrix (`.github/workflows/build.yml`) builds Windows, macOS, Linux x64 and Linux arm64 (cross), Debug and Release, on every push. Tartarus is Linux-first and is only built and played on Linux. The Windows jobs started failing on Tartarus code (BUG-009); nobody can test fixes for Windows or macOS here.

**Options.**
- **A. Keep the full matrix and fix Windows blind.** Rejected: fixes can't be tested locally, and every push would keep paying for platforms nobody runs.
- **B. Build Linux only (x64 and the arm64 cross build).** Chosen.

**Decision.** Option B. The Windows and macOS matrix entries are commented out (not deleted, so upstream merges stay readable) with a `# TARTARUS:` marker. Platform-specific failures on Windows/macOS are logged as `deferred`, not fixed.

**Consequences.**
- CI is faster and stays green for the platform that matters.
- Tartarus code may drift from compiling on Windows/macOS; anyone reviving those builds starts with BUG-009 and a CI run.

**Reversal conditions.** If a contributor commits to testing on Windows or macOS, restore the matrix entries and fix what fails.

### D-015 Build the game in CI on demand, one Linux job
**Decided:** 2026-11-02
**Recorded:** 2026-11-02
**Status:** Accepted
**Authors:** Shane Hartley
**Related:** D-014, AV-001, IMP-001

**Context.** After D-014, every push to `main` still ran upstream's Continuous Delivery workflow as four jobs (Linux x64 and the arm64 cross build, Debug and Release), each building every vcpkg dependency and the game: about 20 minutes and roughly 80 runner-minutes per push. Every Tartarus commit is already built in both configurations and unit-tested on the owner's machine first, so the CI build mostly repeats that; its value is a clean-machine check (uncommitted files, undeclared dependencies).

**Options.**
- **A. Keep building on every push.** Rejected: slow, and expensive if the repository is private.
- **B. Turn CI builds off.** Rejected: loses the occasional clean-machine check.
- **C. Build on demand (and on pull requests), as one Linux x64 Release job with tests; keep the fast "Tartarus audits" workflow on every push.** Chosen.

**Decision.** Option C, plus a weekly automatic build as a safety net (owner, 2026-11-02). `cd.yml` runs from the Actions tab ("Run workflow") and every Monday at 04:17 UTC instead of on push; `build.yml`'s matrix is Linux x64 Release only (arm64 cross and Debug commented out with `# TARTARUS:` markers). `ci.yml` (pull requests) is unchanged and uses the same single job.

**Consequences.**
- A normal push runs only the audits (under a minute).
- Clean-machine breakage is found by the weekly run at the latest; run it by hand before tagging a release and after changing dependencies or the build files.

**Reversal conditions.** If breakage that only CI would catch slips through the weekly run, restore the push trigger.

### D-016 Tartarus menus are a new drawn layer, in a cassette-futurism style; Classic Mode keeps the stock menus
**Decided:** 2026-11-03
**Recorded:** 2026-11-03
**Status:** Accepted
**Authors:** Shane Hartley (with Claude design discussion and a clickable mock-up, 2026-11-03)
**Related:** F-069, F-060, D-013, IMP-005, IMP-006, AV-001

**Context.** F-069 was specified as built on upstream `newui`. That system is fixed-size 640x480 bitmap artwork with an option list of eight buttons: it cannot give the look the owner wants (IMP-006), its windows shrink at high resolutions (IMP-005), and there is no room for the settings added in Phase 2.

**Options.**
- **A. Extend `newui`.** Rejected: the limits above remain.
- **B. A new Tartarus menu layer drawn from rectangles, lines and scaled text, with pages generated from cvar metadata.** Chosen.

**Decision.** Option B. Look: amber phosphor on gunmetal casing, tape-deck page keys with an orange LED on the active page, scanline strength adjustable inside the menu (`ui_scanlines`); other phosphors and casings stay available as themes, including a plain one for readability. Tartarus mode gets the new deck on F2; **Classic Mode keeps the stock menus** ("Classic is classic"), and multiplayer keeps them for now because the deck pauses the game. Stock options stay reachable from the deck until each dialog is replaced.

**Consequences.**
- Mouse, keyboard and later gamepad handling are ours to build.
- Text uses the game's fonts until an open-licensed font passes review (D-003).
- Screens move over one at a time: in-level options first, then options from the main menu (which needs its own loop), then the main menu, pilot and save/load screens.

**Reversal conditions.** If the layer's upkeep outweighs its value, the stock menus are still there; `Deck_HandleKey()` is the only place that takes F2.

### D-017 Orientation defaults: the flight HUD on the canopy, every other aid off
**Decided:** 2026-11-05
**Recorded:** 2026-11-05
**Status:** Accepted
**Authors:** Shane Hartley (with Claude design discussion 2026-11-05)
**Related:** F-040, F-041, F-042, F-046, D-010

**Context.** The owner wanted more than the up-tick, ball and gizmo, using an aircraft head-up display as the reference, drawn so it looks as if it sits on the ship's canopy. D-010 kept only the up-tick on by default.

**Options.**
- **A. Keep D-010.** Rejected: the owner wants the flight HUD to be the main aid.
- **B. Flight HUD on by default in cockpit view; up-tick, ball and gizmo off; one master switch for them all.** Chosen.
- **C. Everything off until switched on.** Rejected: new players would never find it, the reason D-010 rejected its own option C.

**Decision.** Option B, in Tartarus mode. It appears when the canopy is closed (cockpit view) and is drawn before the cockpit model, so the frame covers it and it sits on the glass. The full-screen HUD view shows it only if `hud_flight_fullscreen` is set. Pale green for now; other colours may follow if it works well. The right-hand tape, when it comes, is range to the wall ahead. Classic Mode: all off. Stronger auto-level stays opt-in, as in D-010 (`al_strength` 0).

**Consequences.**
- `hud_uptick` now defaults to off. A value saved in `tartarus.cfg` still applies.
- Pilots who fly in the full-screen HUD view see no aid by default.

**Reversal conditions.** Playtest evidence that the flight HUD clutters the view, or that new players in the full-screen view lose track of up.
