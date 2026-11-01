# Features

## Target users

Linux players who own Descent 3 and want it to run natively and well, and pilots who want new *tactical* options (cloak, gravity, orientation aids) layered over the original game without losing the original. Secondary audience: modders who want a console, cvars and eventually a scripting layer on the D3 engine.

## Out of scope

- Redistributing any Descent 3 game data (HOGs, missions, movies, textures). Ever.
- A new renderer or asset pipeline in v1 (tracked as a candidate, F-066).
- Multiplayer compatibility with stock Descent 3 clients when Tartarus features are enabled. Classic Mode remains wire-compatible; Tartarus mode is Tartarus-only.
- Windows and macOS as *primary* targets. Upstream presets are not deliberately broken, but they are not tested here.
- The level editor (`editor/`, Windows/MFC only upstream). Not built, not touched.
- Rebalancing stock weapons in Classic Mode. Classic means classic.

## Features

Priority is MoSCoW. Status is one of Not started | In progress | Complete | Withdrawn. Phase references are to ROADMAP.md.

---

### A. Foundation

### F-001 Linux-native build tracking upstream
**Priority:** Must
**Acceptance:**
- `cmake --preset linux && cmake --build --preset linux --config Release` succeeds on the reference machine (BUILD.md) with no patches outside the repo
- Upstream `main` can be merged with conflicts confined to `Descent3/tartarus/` and the hook-point files listed in ARCHITECTURE.md
- `tools/upstream-sync.sh` fetches and merges upstream and reports the conflict set
**Status:** In progress (Phase 0): reference build passes and `tools/upstream-sync.sh` exists (2026-11-01); acceptance needs a real upstream merge with new commits
**Notes:** D-001.

### F-002 Classic Mode
**Priority:** Must
**Acceptance:**
- A single boolean `classic` (cvar + Options toggle + `-classic` command-line flag) disables every Tartarus gameplay, HUD and AI change; only performance/compatibility fixes remain active
- With `classic 1`, a scripted playthrough of the first two levels produces no HUD element, weapon, keybind or behaviour absent from upstream
- Save games record the mode; loading a Tartarus save in Classic Mode warns and refuses rather than silently dropping state
- Default is `classic 1` on first run
**Status:** Not started (Phase 1)
**Notes:** D-004, AV-001.

### F-003 Steam data directory detection
**Priority:** Should
**Acceptance:**
- On launch with no `-additionaldir`, the game probes the standard Steam library paths (`~/.steam/steam/steamapps/common/Descent 3`, `~/.local/share/Steam/...`, Flatpak Steam path) and any paths in `libraryfolders.vdf`
- If exactly one install is found it is used and logged; if several, the user is asked once and the choice persisted
- Missing required HOGs produce a readable error naming the file and the directory searched, not a crash
**Status:** In progress (Phase 0): probe, de-duplication and the missing-HOG error done and tested (2026-11-01). With several installs it uses the first and logs the rest; "ask once and persist" waits for Phase 1 (cvars/UI)

### F-004 Upstream credit and licence compliance
**Priority:** Must
**Acceptance:**
- README credits Outrage, Interplay and DescentDevelopers; `LICENSE` and `THIRD_PARTY.md` are carried forward unmodified
- Main menu build string shows "Tartarus <version> <revision>" over "based on Descent 3 Open Source <upstream hash>" (two lines, bottom right; one line does not fit a 640 px-wide menu)
- Every published binary release links to the exact source tag
**Status:** In progress (Phase 0): credits, unmodified `LICENSE`/`THIRD_PARTY.md` and the build string done (2026-11-01); release-tag linking applies from the first binary release
**Notes:** D-003, AV-012.

---

### B. Developer tooling

### F-010 Developer console
**Priority:** Must
**Acceptance:**
- Backtick toggles a drop-down console over the game view; game input is suppressed while it is open and restored exactly on close
- Supports commands and cvars (F-011), tab completion, up/down history (persisted across runs), `help`, `find <substring>`, paging of long output
- All engine log output (plog) is mirrored into the console scrollback
- Commands available at launch: `classic`, `help`, `find`, `exec <file>`, `bind`, `unbind`, `quit`, `map <mission> <level>`, `give <weapon|all>`, `spawn <object>`, `noclip`, `god`, `timescale <f>`, `screenshot`, `cvarlist`, `cheat <password>` (see F-012)
- Cheat-class commands are refused unless the matching cheat is unlocked (F-012) or the build is a Debug build
**Status:** Not started (Phase 1)
**Notes:** D-005. SPEC.md §Console grammar.

### F-011 Cvar system
**Priority:** Must
**Acceptance:**
- Typed cvars (bool/int/float/string) with default, min/max, description and flags `ARCHIVE` (persist), `CHEAT` (needs unlock), `CLASSIC_LOCKED` (forced to default in Classic Mode), `LATCH` (applies on level load)
- Persisted cvars round-trip through `tartarus.cfg` in the writable base directory
- Every tunable named in SPEC.md is a cvar
**Status:** Not started (Phase 1)
**Notes:** D-005.

### F-012 Password-gated cheat menu
**Priority:** Must
**Acceptance:**
- An in-game Cheats screen (from the pause menu or `F12`) shows a password field and a list of cheat toggles; each toggle is greyed out until its password has been entered once in the current pilot's session
- Passwords are case-insensitive, letters only, and listed in SPEC.md §Cheat passwords; the same passwords work via `cheat <password>` in the console
- Unlocking any cheat sets the existing `IsCheater` flag (disables mission-progress recording and score tables exactly as upstream cheats do); the HUD shows a small persistent "CHEATS" tag
- Cheats are refused in multiplayer games and when `classic 1`, with a message
- Infinite-resource cheats (energy, shields, primary ammo, secondary ammo, afterburner) are *toggles* that hold the value at maximum each frame, not one-shot refills
- `CATHARSIS` relocks everything and clears the flag for the next new game (not the current one)
**Status:** Not started (Phase 1)
**Notes:** D-006. Upstream's `GameCheat.cpp` typed-sequence cheats stay functional in Classic Mode.

### F-013 Debug overlays
**Priority:** Should
**Acceptance:**
- `r_showaxes 1` draws world X/Y/Z at the player; `r_showobjinfo 1` labels nearby objects with type/id/handle; `r_showcollide 1` draws collision spheres
- Zero cost when off (no per-frame work)
**Status:** Not started (Phase 1)

---

### C. Tactical weapons and systems

### F-020 Player-triggered cloak
**Priority:** Must
**Acceptance:**
- Bound key engages cloak if energy ≥ `cloak_min_energy`; charges `cloak_cost` up front and drains `cloak_drain` per second; auto-drops at `cloak_max_time`
- Cloak drops immediately on any primary/secondary fire (flares exempt by default, `cloak_break_on_flare`), on contact with a robot, and on damage ≥ `cloak_break_damage`
- A forced drop starts a cooldown of `cloak_cooldown_forced`; a voluntary drop `cloak_cooldown`
- Robots lose target lock while cloaked, using the existing invisibility path (`MakeObjectInvisible`); a bump decloaks *and* alerts the bumped robot
- HUD gauge shows state (ready / active with time left / cooldown)
**Status:** Not started (Phase 4)
**Notes:** SPEC.md §Cloak. AV-007.

### F-021 Gravity missile
**Priority:** Must
**Acceptance:**
- Dumb-fire secondary; on wall impact spawns a gravity well of radius `gw_radius`, duration `gw_time`, peak force `gw_force`, with linear falloff
- Robots within the radius are pulled toward the impact point through physics forces (no teleporting); mass scaling means light robots arrive, heavy ones budge, bosses flagged `GW_IMMUNE` ignore it
- Works without line of sight from the player: a robot round a corner within radius is drawn into the corridor
- Player is affected when within `gw_self_radius` (default on)
- Low direct damage; scarce pickup; distinct impact and pull visuals and audio
- `give gravmissile` works from the console
**Status:** Not started (Phase 4)
**Notes:** D-007, D-009. Reuses the engine's `GRAVITY_FIELD_INDEX` fireball machinery (`fireball.cpp::DoGravityFieldEffect`). SPEC.md §Gravity missile. AV-006.

### F-022 Repulsor pulse
**Priority:** Should
**Acceptance:**
- Bound key emits a radial shove of radius `rp_radius`, force `rp_force`, cost `rp_cost` energy, cooldown `rp_cooldown`
- Pushes robots, weapons (missiles deflect) and the player (wall-launch is possible)
**Status:** Not started (Phase 4)
**Notes:** Same force code as F-021 with the sign flipped.

### F-023 Flak / proximity burst missile
**Priority:** Could
**Acceptance:**
- Detonates when a robot comes within `flak_trigger_radius` of the projectile, or on impact
**Status:** Not started (Phase 6)

### F-024 Ricochet slug
**Priority:** Could
**Acceptance:**
- Bounces up to `ric_bounces` times; damage multiplied by `ric_gain` per bounce; uses existing bounce flags
**Status:** Not started (Phase 6)

### F-025 Decoy drone
**Priority:** Could
**Acceptance:**
- Deploys a stationary hologram of the player ship for `decoy_time`; robots within range retarget it in preference to the player
**Status:** Not started (Phase 6)
**Notes:** Needs an AI target-selection hook (F-070).

### F-026 EMP mine
**Priority:** Could
**Acceptance:**
- Proximity mine that applies a `stunned` AI state for `emp_time` instead of damage
**Status:** Not started (Phase 6)

### F-027 Overcharge
**Priority:** Could
**Acceptance:**
- Holding primary fire on an energy weapon for > `oc_hold` seconds dumps up to `oc_max_energy` into a single shot scaled by `oc_gain`; HUD charge bar
**Status:** Not started (Phase 6)

### F-028 Tether / grapple
**Priority:** Could
**Acceptance:**
- Fires a line; on wall hit applies a spring force toward the anchor until released or `tether_max_len` exceeded
**Status:** Not started (Phase 6)

### F-030 Energy ↔ shield converter
**Priority:** Should
**Acceptance:**
- Two bound keys convert at `conv_rate` units/second with `conv_ratio` loss, never above max or below `conv_floor`
**Status:** Not started (Phase 4)
**Notes:** Descent 2 had this; D3 dropped it.

### F-031 Dash
**Priority:** Could
**Acceptance:**
- Directional impulse of `dash_impulse` on double-tap of a slide key, cooldown `dash_cooldown`, energy cost `dash_cost`
**Status:** Not started (Phase 6)

### F-032 Scanner ping
**Priority:** Could
**Acceptance:**
- Bound key marks robots within `ping_radius` on HUD and automap for `ping_time`, through walls, cost `ping_cost`
**Status:** Not started (Phase 6)

---

### D. Orientation awareness

### F-040 Reticle up-tick
**Priority:** Must
**Acceptance:**
- A short tick on the reticle ring always points toward world-up projected onto the view plane; fades out when the ship is within `hud_uptick_fade_deg` of upright
- `hud_uptick` cvar; on by default in Tartarus mode
**Status:** Not started (Phase 3)
**Notes:** D-010.

### F-041 Attitude indicator
**Priority:** Must
**Acceptance:**
- Aircraft-style pitch/roll ball relative to the level's up vector (terrain gravity direction outdoors; world +Y indoors), positioned per HUD layout
- `hud_attitude` cvar; off by default
**Status:** Not started (Phase 3)

### F-042 Orientation gizmo
**Priority:** Should
**Acceptance:**
- Corner 3-axis gizmo showing world axes relative to ship; `hud_gizmo` cvar
**Status:** Not started (Phase 3)

### F-043 Enhanced auto-level and snap-to-up
**Priority:** Should
**Acceptance:**
- `al_strength` (0–1) and `al_delay` cvars extend upstream's autolevel slider; `al_hold` key rolls to upright while held; `al_snap` key single-tap rolls to nearest upright over `al_snap_time`
- Never fights active roll input: any roll input cancels an in-progress snap
**Status:** Not started (Phase 3)
**Notes:** Builds on `physics/physics.cpp` autolevel code. AV-011.

### F-044 Environmental up cues
**Priority:** Could
**Acceptance:**
- Optional drifting dust particles falling along world-down in indoor rooms; optional subtle brightness gradient toward up
**Status:** Not started (Phase 6)

### F-045 Heading tape
**Priority:** Could
**Acceptance:**
- Yaw compass strip, outdoors only by default
**Status:** Not started (Phase 6)

---

### E. Automap

### F-050 Automap overhaul
**Priority:** Should
**Acceptance:**
- Visited vs seen-but-unvisited rooms distinguished by colour; terrain can be hidden; objective, key, door and powerup markers filterable
- 2D top-down projection mode with depth shading, toggled from within the map
- Opening the map always starts camera-up = world-up, with a clear ship-with-up-arrow glyph (F-051)
**Status:** Not started (Phase 3)
**Notes:** `Descent3/TelComAutoMap.cpp`.

### F-051 Automap orientation anchoring
**Priority:** Should
**Acceptance:**
- Player glyph shows facing and up; closing the map briefly shows the up-tick enlarged for `map_reanchor_time`
**Status:** Not started (Phase 3)

---

### F. Quality of life and modernisation

### F-060 Widescreen HUD, menus and FOV
**Priority:** Must
**Acceptance:**
- HUD elements anchor to screen edges at any aspect ratio; `r_fov` cvar 60–120 applies to the 3D view without stretching the HUD
- The 3D view uses the correct aspect at 16:9, 16:10 and 21:9 without needing upstream's `-aspect` flag
- Main menu, options and in-game menus fill the screen or are cleanly centred at any aspect ratio (upstream draws them 4:3 with bars, seen 2026-11-01)
- Small-view windows (rear view Shift+F1/Shift+F2, Guide-Bot and missile cameras) have adjustable size, shape and position per window (SPEC.md §14); the defaults reproduce the upstream layout, and windows stay fully on screen at every aspect ratio
**Status:** Not started (Phase 2)
**Notes:** Upstream FOV slider is limited to 72–90° (`Render_FOV_setting`, `config.cpp`). Upstream small views (`SmallViews.cpp::DrawSmallViews`) are square, `Game_window_h / 4` wide, spaced along the bottom of the 3D view; the cockpit view uses fixed cockpit monitors and is left as upstream. Small-view layout requested 2026-11-01. D-013.

### F-061 Fixed-timestep physics with uncapped render rate
**Priority:** Should
**Acceptance:**
- Physics/AI tick at a fixed rate (SPEC.md); rendering interpolates; game feel at 60/144/240 Hz is identical in a recorded-input test
- Classic Mode may keep upstream timing if the change proves behaviour-altering (see D-012)
**Status:** Not started (Phase 5)
**Notes:** D-012 (Proposed). Highest-risk item in the plan.

### F-062 Raw mouse input
**Priority:** Should
**Acceptance:**
- SDL relative mode, no OS acceleration, `m_sensitivity` and `m_accel 0` by default
**Status:** Not started (Phase 2)
**Notes:** Upstream already uses `SDL_SetWindowRelativeMouseMode`; this item is about removing acceleration/smoothing and exposing one sensitivity. Pairs with F-067.

### F-063 Gamepad with gyro aim
**Priority:** Could
**Acceptance:**
- SDL3 gamepad API including gyro where present; bindable in the existing controls UI
**Status:** Not started (Phase 2)

### F-064 Quicksave anywhere
**Priority:** Should
**Acceptance:**
- `F5`/`F9` quicksave/quickload using the existing save path; `g_checkpoints 0` disables mid-level autosaves for purists
**Status:** Not started (Phase 5)

### F-065 Demo free camera
**Priority:** Could
**Acceptance:**
- During demo playback, a detached camera with fly controls and pause/scrub
**Status:** Not started (Phase 7)

### F-066 Modern renderer path
**Priority:** Won't (v1)
**Acceptance:** n/a — candidate only
**Status:** Not started

### F-067 Mouse flight modes (virtual joystick)
**Priority:** Must
**Acceptance:**
- `m_mode` selects Classic (upstream: mouse movement = rotation), Virtual joystick (mouse offset from centre = rotation *rate*; the ship keeps turning until the mouse returns to centre), or Virtual joystick with auto-centring (the offset decays toward centre at `m_vj_centre_rate`)
- Virtual-joystick tuning per SPEC.md §13: dead zone, full-rate radius, response curve, per-axis invert, optional mouse-X-as-roll
- An on-screen indicator shows the current offset from centre (dot plus line from the reticle) and the dead-zone ring; `m_vj_indicator` toggles it
- A recentre key zeroes the offset instantly; the offset is also zeroed whenever the console, a menu or the automap opens, on focus loss, and on death/level load (AV-013)
- In Classic Mode `m_mode` is forced to Classic (`CLASSIC_LOCKED`)
- The owner can complete level 1 with the mouse in Virtual joystick mode (closes the Phase 0 acceptance check)
**Status:** Not started (Phase 2)
**Notes:** Requested 2026-11-01 after the Phase 0 playthrough: sustained turns with the stock mouse needed repeated sweeps. Implemented as an analogue axis source in the control layer (the same path a joystick uses), so the flight model is untouched. D-013, AV-013.

### F-068 Advanced graphics options
**Priority:** Should
**Acceptance:**
- Display mode: fullscreen, borderless fullscreen, windowed; any resolution SDL reports, plus a custom width/height
- VSync on/off/adaptive; frame-rate cap (off, 60, 120, 144, 240, custom), replacing upstream's fixed 60 fps default cap
- Render scale (resolution of the 3D view independent of the UI), texture filtering (bilinear/trilinear/anisotropic level), MSAA where the GL context supports it, gamma
- Every option is a cvar (`r_` prefix), shown on the Graphics page of F-069, applied without restarting the game where the renderer allows, otherwise flagged "applies on restart"
- Performance/compatibility only: available in Classic Mode (D-004)
**Status:** Not started (Phase 2)
**Notes:** Exact option list confirmed against the OpenGL renderer (`renderer/HardwareOpenGL.cpp`) at the start of Phase 2. D-013.

### F-069 Options menu overhaul
**Priority:** Should
**Acceptance:**
- One Options screen with pages: Graphics (F-068, F-060), Controls (F-067, F-062, F-063, upstream bindings), Audio (upstream), Gameplay, Tartarus (Classic Mode toggle, F-002)
- Every page is generated from cvar metadata (type, range, description) so new settings appear without new UI code
- Works with mouse, keyboard and gamepad; readable at 1080p and above
- Upstream's existing option dialogs remain reachable until each is replaced
**Status:** Not started (Phase 2)
**Notes:** Built on upstream `newui`. Also hosts the Phase 1 cheat menu entry. D-013.

---

### G. AI

### F-070 AI reactions to Tartarus tools
**Priority:** Should
**Acceptance:**
- Robots caught in a gravity well attempt to thrust away if they can; a decloak flash within sight triggers an investigate behaviour; stunned state exists for F-026
**Status:** Not started (Phase 6)

### F-071 Mirror difficulty tier
**Priority:** Could
**Acceptance:**
- An optional difficulty above Insane where selected robots use cloak and gravity weapons
**Status:** Not started (Phase 7)

---

### H. Modding platform

### F-080 Scripting layer
**Priority:** Could
**Acceptance:**
- Lua (or equivalent) bindings for weapon definitions, console commands and simple object behaviours; new weapons definable without recompiling
**Status:** Not started (Phase 7)

### F-081 Mission browser
**Priority:** Could
**Acceptance:**
- In-game list of installed `.mn3` missions with level counts and descriptions, replacing the stock file dialog
**Status:** Not started (Phase 7)

## Candidate features (uncommitted)

- Loadout limits (choose fewer secondaries) — turns pickups into decisions; may conflict with level design
- Replay-based time-trial mode using F-061 determinism
- Vulkan renderer (F-066) once SDL3 GPU API stabilises upstream
- Accessibility: colour-blind palettes for F-050 markers, screen-shake scale
- Flatpak packaging that prompts for the Steam data directory
