> **Status:** Active
> **Provenance:** Shane Hartley (project owner, design); Claude (initial documentation scaffolding, 2026-11-01)
> **Last reviewed:** 2026-11-04
> **Why this status:** In development. Upstream is merged and the game plays from a Steam install; Phases 0 and 1 of ROADMAP.md are closed apart from listed deferrals, and Phase 2 (controls and display) is under way.

# Tartarus — a tactical Descent 3 for Linux

*Working title; see D-002.*

Tartarus is a Linux-first fork of the open-source Descent 3 engine ([DescentDevelopers/Descent3](https://github.com/DescentDevelopers/Descent3), GPL-3.0). It keeps the original game intact behind a **Classic Mode** switch and layers on a set of *tactical* additions that exploit Descent's six-degrees-of-freedom tunnels rather than simply adding bigger guns: a player-triggered cloak, a gravity missile that drags enemies round corners, orientation aids for pilots who lose track of "up", an overhauled automap, a proper developer console, and a password-gated cheat menu. It is for people who own Descent 3 (Steam or otherwise) and want it to run natively and well on Linux, and for anyone who wants a modern, scriptable sandbox on top of the 1999 engine.

Tartarus is an engine fork only. It does **not** include any Descent 3 game data; you need your own copy of the game.

## The deck

Tartarus replaces the stock menus with a cassette-futurism "deck": amber phosphor on a gunmetal casing, drawn at your screen's resolution. Classic Mode keeps the original menus.

![The main menu on the deck](docs/images/deck-main.png)

![The Graphics page of the options deck](docs/images/deck-options.png)

## Orientation aids

Losing track of which way is up is a classic Descent problem. In cockpit view a pale green flight HUD on the canopy shows the horizon, a pitch ladder and where the ship is actually going. Optional aids can be switched on in the deck's Gameplay page: a wedge on the reticle that points to world up, an axis gizmo and this attitude ball, which shows pitch and roll against the horizon. Classic Mode has none of them.

<img src="docs/images/attitude-ball.png" alt="The attitude ball, rolled with the nose above the horizon line" width="200">

## Credits and licence

Descent 3 was created by Outrage Entertainment and published by Interplay (1999). The source was released under GPL-3.0 on 16 April 2024 and is maintained by the DescentDevelopers community; Tartarus would not exist without their 64-bit, SDL and CMake modernisation work. Upstream's `THIRD_PARTY.md` is carried forward unchanged. See [LICENSE](LICENSE) (GPL-3.0, inherited — see D-003).

## Quick start

```bash
# 1. Clone (submodules carry third-party code)
git clone --recurse-submodules https://github.com/Darian-Frey/tartarus
cd tartarus

# 2. Build (Linux preset; see BUILD.md for dependencies)
cmake --preset linux
cmake --build --preset linux --config Release

# 3. Run. A Steam install of Descent 3 is found automatically; otherwise
#    add -additionaldir "<path to your Descent 3 install>" (BUILD.md §4)
./builds/linux/build/Release/Descent3
```

The first launch runs in Classic Mode. Open the console with the backtick key (`` ` ``) and type `classic 0` to enable Tartarus features, or toggle it in Options → Tartarus.

## Build requirements

Linux x86-64 (primary; Linux Mint 22 / Ubuntu 24.04 is the reference machine), CMake ≥ 3.20, Ninja, GCC ≥ 13 or Clang ≥ 17, SDL3 ≥ 3.2.6, zlib, glm, plog, gtest, cpp-httplib, libxcrypt. Dependencies can come from the distro or from vcpkg exactly as upstream. Full detail in [BUILD.md](BUILD.md).

## Project structure

```
tartarus/
├── Descent3/            Game module (upstream layout preserved)
│   └── tartarus/        All Tartarus additions live here — see ARCHITECTURE.md
├── physics/ renderer/ … Upstream engine modules (modified only where listed in ARCHITECTURE.md §Hook points)
├── data/tartarus/       Sidecar data shipped with the fork (weapon definitions, HUD layouts; no Outrage assets)
├── docs/                Design notes that are too long for the standard documents
├── tools/               Scripts (cross-ref checker, upstream-sync helper)
└── *.md                 Standard documents (below)
```

## Documentation map

| Document | What it answers |
|----------|-----------------|
| [FEATURES.md](FEATURES.md) | What Tartarus does, with priorities and acceptance criteria (F-NNN) |
| [ROADMAP.md](ROADMAP.md) | Which phase delivers which features |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Where the new code sits and how it hooks into the upstream engine |
| [DECISIONS.md](DECISIONS.md) | Why it is built this way (D-NNN) |
| [SPEC.md](SPEC.md) | Constants, formulas, console grammar, cheat passwords, data formats |
| [ATTACK_VECTORS.md](ATTACK_VECTORS.md) | How it could break, and how each failure is detected (AV-NNN) |
| [BUGS.md](BUGS.md) / [IMPROVEMENTS.md](IMPROVEMENTS.md) | Realised defects / candidate refactors (BUG-NNN, IMP-NNN) |
| [BUILD.md](BUILD.md) | Toolchain, dependencies, build and run commands |
| [CHANGELOG.md](CHANGELOG.md) | What changed, when |
| [CLAUDE.md](CLAUDE.md) | Handoff for AI-assisted sessions |

## License

GPL-3.0 — see [LICENSE](LICENSE). Game data is the property of its rights holders and is never distributed with this project.
