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
**Status:** In progress (Phase 1): `classic` cvar, `IsClassic()`, `-classic`/`-tartarus` and read-side `CLASSIC_LOCKED` done (2026-11-01); Options toggle (F-069), save-game mode stamp and the AV-001 audit pending
**Notes:** D-004, AV-001. See also D-008.

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
**Notes:** D-003, AV-012. See also D-001.

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
**Status:** In progress (Phase 1): toggle, input capture (controls suspended, queues flushed on close), scrollback, history persisted to `console_history.txt`, tab completion, plog mirror, `help`/`find`/`cvarlist`/`set`/`reset`/`toggle`/`echo`/`clear`/`exec`/`classic`/`screenshot`/`version`/`quit` done and verified in level 1 (2026-11-01); cheat commands `cheat`, `cheatmenu`, `give`, `god`, `timescale`, `map` (picker only) done with F-012 (2026-11-01); `con_pause` (IMP-004), `bind`/`unbind` (saved to `binds.cfg`), `cheat use` and `where` done (2026-11-02); deferred: `noclip`, `spawn`, `map <mission> <level>` arguments, `con_key`, line wrapping (IMP-003)
**Notes:** D-005. SPEC.md §Console grammar. See also AV-008.

### F-011 Cvar system
**Priority:** Must
**Acceptance:**
- Typed cvars (bool/int/float/string) with default, min/max, description and flags `ARCHIVE` (persist), `CHEAT` (needs unlock), `CLASSIC_LOCKED` (forced to default in Classic Mode), `LATCH` (applies on level load)
- Persisted cvars round-trip through `tartarus.cfg` in the writable base directory
- Every tunable named in SPEC.md is a cvar
**Status:** In progress (Phase 1): registry, all five flags, `tartarus.cfg` persistence and 16 unit tests done (2026-11-01); "every SPEC tunable is a cvar" is met module by module as each lands
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
**Status:** In progress (Phase 1): unlock table, password entry, toggles/buttons/slider, hold-at-max loop, `IsCheater` + score reset, CHEATS tag, Classic/multiplayer refusal and `cheat`/`give`/`god`/`timescale`/`map` console commands done and verified in level 1 (2026-11-01); 17 of 19 rows work, from the menu and from the console (`cheat use <password> [value]`, 2026-11-02). Deferred (ROADMAP Phase 1): WRAITH (noclip: upstream has none, and moving through walls without breaking room tracking needs its own design), ARGUS (upstream's chase camera is half-built), countermeasures (mines, chaff) in HYDRA's hold, and the LAN check for AV-004. Opens with **Shift+F12**, not F12: stock F12 drops a marker, and upstream has no pause menu to add an entry to (pause is a message box); owner confirmed 2026-11-01 after testing.
**Notes:** D-006. Upstream's `GameCheat.cpp` typed-sequence cheats stay functional in Classic Mode. The game pauses while the menu is open (`con_pause`, IMP-004). See also D-008.

### F-013 Debug overlays
**Priority:** Should
**Acceptance:**
- `r_showaxes 1` draws world X/Y/Z at the player; `r_showobjinfo 1` labels nearby objects with type/id/handle; `r_showcollide 1` draws collision spheres
- Zero cost when off (no per-frame work)
**Status:** Deferred (2026-11-02, owner): to just before the Phase 4 weapons work, where it is needed
**Notes:** See also AV-009.

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
**Notes:** SPEC.md §Cloak. AV-007. See also D-008.

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
**Notes:** D-007, D-009. Reuses the engine's `GRAVITY_FIELD_INDEX` fireball machinery (`fireball.cpp::DoGravityFieldEffect`). SPEC.md §Gravity missile. AV-006. See also D-008.

### F-022 Repulsor pulse
**Priority:** Should
**Acceptance:**
- Bound key emits a radial shove of radius `rp_radius`, force `rp_force`, cost `rp_cost` energy, cooldown `rp_cooldown`
- Pushes robots, weapons (missiles deflect) and the player (wall-launch is possible)
**Status:** Not started (Phase 4)
**Notes:** Same force code as F-021 with the sign flipped. See also AV-006, D-009.

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
**Notes:** See also D-007.

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
- `hud_uptick` cvar; off by default since D-017 (was on, D-010)
**Status:** Done (Phase 3), 2026-11-04, accepted by the owner: a wedge at the ring radius, `orient_core.cpp` (unit-tested) and `orient_hud.cpp`, on the deck's Gameplay page. No upstream hook: it draws from `RenderOverlays()`. All three aids together average 0.05 ms a frame (AV-009).
**Notes:** D-010. See also AV-009.

### F-041 Attitude indicator
**Priority:** Must
**Acceptance:**
- Aircraft-style pitch/roll ball relative to the level's up vector (terrain gravity direction outdoors; world +Y indoors), positioned per HUD layout
- `hud_attitude` cvar; off by default (D-017)
**Status:** Done (Phase 3), 2026-11-04: in `orient_hud.cpp`, positioned by cvars rather than a layout file, on the deck's Gameplay page.

### F-042 Orientation gizmo
**Priority:** Should
**Acceptance:**
- Corner 3-axis gizmo showing world axes relative to ship; `hud_gizmo` cvar; off by default (D-017)
**Status:** Done (Phase 3), 2026-11-04: in `orient_hud.cpp`, mid-left by default rather than a corner, where the rear-view presets sit.
**Notes:** See also AV-009.

### F-043 Enhanced auto-level and snap-to-up
**Priority:** Should
**Acceptance:**
- `al_strength` (0–1) and `al_delay` cvars extend upstream's autolevel slider; `al_hold` key rolls to upright while held; `al_snap` key single-tap rolls to nearest upright over `al_snap_time`
- Never fights active roll input: any roll input cancels an in-progress snap
**Status:** Done (Phase 3), 2026-11-05, accepted by the owner: `level_assist.cpp`, as bank input in the controls stage, so `physics/physics.cpp` needed no change. In a scripted test a snap took a 17° roll to within 3° in 0.35 s, hold settled at 0.4° with no overshoot, a tap while rolling did nothing, and `-classic` ignored the keys. Keys are console settings (`al_hold_key`, `al_snap_key`), not in the stock bindings screen, which cannot take new functions without changing pilot files (AV-003).
**Notes:** Works alongside `physics/physics.cpp` autolevel code rather than changing it. AV-011. See also D-010.

### F-044 Environmental up cues
**Priority:** Could
**Acceptance:**
- Optional drifting dust particles falling along world-down in indoor rooms; optional subtle brightness gradient toward up
**Status:** Not started (Phase 6)

### F-045 Heading tape
**Priority:** Could
**Acceptance:**
- Yaw compass strip, outdoors only by default
**Status:** Done 2026-11-05 as part of F-046, moved up from Phase 6 by the owner; it shows indoors too

### F-046 Flight HUD on the canopy
**Priority:** Should
**Acceptance:**
- In cockpit view, pale green symbols on the canopy glass, behind the cockpit frame: horizon and pitch ladder on the real world at any FOV, flight path marker, roll arc, heading tape (F-045), speed tape, range to the wall ahead, climb rate and status words
- `hud_aids` master switch turns off every orientation aid; each element has its own switch; on by default with the canopy closed, optional in the full-screen HUD view (D-017)
- All orientation aids within the AV-009 budget
**Status:** Done (Phase 3), apart from moving with the cockpit shake and a default key for `hud_aids`. Stage 1 done 2026-11-05: `hud_aids`, horizon, pitch ladder (dashed below the horizon) and flight path marker (crossed when moving backwards, dimmed at the edge when off screen), on the deck's Gameplay page. Stage 2 done 2026-11-05: roll arc and heading tape (F-045), with all shapes batched by colour, about 0.08 ms a frame. Stage 3 done 2026-11-05: speed tape, range to the wall ahead, climb rate, about 0.12 ms a frame on average with peaks of about 0.25 ms. Stage 4 done 2026-11-05: status words either side of the heading tape, shown only while active.
**Notes:** D-017. See also AV-009. One hook in `RenderHUDFrame()`.

### F-047 Reticle styles
**Priority:** Should
**Acceptance:**
- In Tartarus mode the targeting reticle is a choice of four drawn styles or stock, on the deck's Gameplay page; Classic Mode always has the stock reticle
- Every style shows which primary guns and missile launchers exist and which are ready to fire, from upstream's slot masks
**Status:** Done pending the owner's choice of default (Phase 3), 2026-11-05: Gunsight (default), Brackets, Chevrons, Cross and Stock; `hud_reticle`, `hud_reticle_size`, `hud_reticle_alpha`. Zoom and guided missile reticles stay stock.
**Notes:** Requested by the owner while working on F-046. One hook in `RenderReticle()`.

---

### E. Automap

### F-050 Automap overhaul
**Priority:** Should
**Acceptance:**
- Scanned, seen and unknown surfaces distinguished (was visited and seen rooms; owner, 2026-11-05); terrain can be hidden; objective, key, door and powerup markers filterable
- 2D top-down mode: moved to IMP-010 as a nice-to-have (owner, 2026-11-05)
- Opening the map always starts camera-up = world-up, with a clear ship-with-up-arrow glyph (F-051)
**Status:** Done (Phase 3), 2026-11-05, with 2D mode set aside (IMP-010). Laser scanner prototype done 2026-11-05 (`scan_core.cpp`, `map_scan.cpp`), owner's idea replacing the visited colouring: the map shows surfaces as the scanner reaches them, 200-unit range. Wireframe look done 2026-11-05: pale green on black, unscanned surfaces blank, new surfaces flash when the map opens. Saving done 2026-11-05: a `.amap` file beside each save, leaving the save format alone. Full-resolution map screen done 2026-11-05: the map runs in the deck casing, outside TelCom, with an amber ship marker in place of the stock model. Symbols and filters done 2026-11-05: keys, doors, goals and powerups where scanned, each switchable, with thin markers and wire terrain. Still to check on a later level: keys, goals and locked doors on the map.
**Notes:** `Descent3/TelComAutoMap.cpp`.

### F-051 Automap orientation anchoring
**Priority:** Should
**Acceptance:**
- Player glyph shows facing and up; closing the map briefly makes the roll arc and heading tape stand out for `map_reanchor_time` (was the up-tick, off by default since D-017)
**Status:** Done (Phase 3), 2026-11-05: an amber wedge with a raised spine for the ship, and the closing cue; a compass tape on the map as well

---

### F. Quality of life and modernisation

### F-060 Widescreen HUD, menus and FOV
**Priority:** Must
**Acceptance:**
- HUD elements anchor to screen edges at any aspect ratio; `r_fov` cvar 60–120 applies to the 3D view without stretching the HUD
- The 3D view uses the correct aspect at 16:9, 16:10 and 21:9 without needing upstream's `-aspect` flag
- Main menu, options and in-game menus fill the screen or are cleanly centred at any aspect ratio (upstream draws them 4:3 with bars, seen 2026-11-01)
- Small-view windows (rear view Shift+F1/Shift+F2, Guide-Bot and missile cameras) have adjustable size, shape and position per window (SPEC.md §14); the defaults reproduce the upstream layout, and windows stay fully on screen at every aspect ratio
**Status:** In progress (Phase 2). Survey 2026-11-01 at 4:3, 16:9 and 16:10: the 3D view is already correct upstream (the projection keeps the vertical FOV and widens the horizontal one, "Hor+"; `-aspect` only affects TelCom), and the cockpit, HUD and in-game options menus look right at all three (21:9 not testable on the reference display). Done: FOV range 60–120° in Tartarus mode (Classic keeps 72–90) on the Video slider and as `r_fov` (2026-11-01). Menus: upstream draws every menu screen on a fixed 640x480 canvas that the renderer scales into the window with bars; the bars are now filled with a blurred, darkened copy of the screen (`r_bar_fill`, owner's choice (b), 2026-11-01). A true widescreen menu canvas (art filling the screen, menus centred; option (a)) is deferred to F-069, which rebuilds those screens anyway. Small-view windows (rear view, Guide-Bot, missile camera): size, aspect and position per window via `hud_sv_*` (SPEC §14), defaults unit-tested pixel-identical to upstream at eight resolutions, verified in level 1 (2026-11-01). Remaining: a 21:9 check (no 21:9 mode on the reference display); the left window has Stock / Top left / Top right rear-view mirror presets in Options (F2) → Tartarus (owner's request, 2026-11-02); other small-view settings are console-only until F-069.
**Notes:** See D-016 for the menus. Upstream FOV slider is limited to 72–90° (`Render_FOV_setting`, `config.cpp`). Upstream small views (`SmallViews.cpp::DrawSmallViews`) are square, `Game_window_h / 4` wide, spaced along the bottom of the 3D view; the cockpit view uses fixed cockpit monitors and is left as upstream. Small-view layout requested 2026-11-01. D-013.

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
**Status:** In progress (Phase 2), 2026-11-02. Findings: SDL3's relative mode is already unaccelerated (`SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE` defaults to 0), so there is no OS acceleration to remove; upstream's "smoothing" is 20 Hz sampling (`MOUSE_POLLING_TIME`): the controller reads the mouse at most every 50 ms and reuses the reading on the frames between, so movement arrives late and in steps. Done: `m_raw` (read every frame), `m_sensitivity`, `m_accel`; the virtual joystick (F-067) is now always read every frame. Measured with the `where` command (120 injected counts, `m_mode 0`): upstream 5.6–7.2° at 60 and 120 fps; `m_raw 1` 1.9° at both (frame-rate independent, but ~3.6x less per count); `m_raw 1` with `m_sensitivity 3.5` gave 18.1° at 60 fps but 6.9° at 120, which is not yet explained (input-chain nonlinearity, or XTest delivery). So `m_raw` defaults to off (owner's choice, 2026-11-02) and the stock mouse is unchanged; pending: tune `m_sensitivity` for `m_raw 1` by feel or a better measurement, then decide the default.
**Notes:** Upstream already uses `SDL_SetWindowRelativeMouseMode`; this item is about removing acceleration/smoothing and exposing one sensitivity. Pairs with F-067. See also D-013.

### F-063 Gamepad with gyro aim
**Priority:** Could
**Acceptance:**
- SDL3 gamepad API including gyro where present; bindable in the existing controls UI
**Status:** Not started (Phase 2)
**Notes:** See also D-013.

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
**Status:** In progress (Phase 2): modes 0/1/2, all SPEC §13 cvars, indicator, recentre (mouse button and `m_recentre`), AV-013 resets and 10 unit tests done; verified by scripted runs in level 1 (2026-11-01): a nudge keeps the ship turning, returning to centre stops it, opening the console stops it. Options (F2) → Tartarus tab (named "Mouse" until 2026-11-02) with mode, speed (`m_vj_sensitivity`), centre speed (`m_vj_centre_rate`, for the auto-centring mode), invert pitch, indicator and roll (2026-11-01, owner's request). Recentre stays on the middle mouse button (owner's choice); while the virtual joystick is active a middle-button flare binding is ignored, and F still fires flares. The owner completed level 1 in virtual-joystick mode (2026-11-01), closing the Phase 0 check; right-button missiles confirmed (BUG-007). Pending: a tuning pass on the defaults.
**Notes:** Requested 2026-11-01 after the Phase 0 playthrough: sustained turns with the stock mouse needed repeated sweeps. Implemented as an analogue axis source in the control layer (the same path a joystick uses), so the flight model is untouched. D-013, AV-013.

### F-068 Advanced graphics options
**Priority:** Should
**Acceptance:**
- Display mode: fullscreen, borderless fullscreen, windowed; any resolution SDL reports, plus a custom width/height
- VSync on/off/adaptive; frame-rate cap (off, 60, 120, 144, 240, custom), replacing upstream's fixed 60 fps default cap
- Render scale (resolution of the 3D view independent of the UI), texture filtering (bilinear/trilinear/anisotropic level), MSAA where the GL context supports it, gamma
- Every option is a cvar (`r_` prefix), shown on the Graphics page of F-069, applied without restarting the game where the renderer allows, otherwise flagged "applies on restart"
- Performance/compatibility only: available in Classic Mode (D-004)
**Status:** In progress (Phase 2), 2026-11-02. Survey: fullscreen is already borderless (SDL3 desktop fullscreen; the Video tab's resolution is the render resolution, scaled to the screen), the monitor is chosen with upstream's `-display N`, and the VSync checkbox was never applied (BUG-010). Done, as `r_` cvars (console; Video tab for VSync): VSync on/off/adaptive (`r_vsync`, `r_vsync_adaptive`), frame cap `r_maxfps` (60 as upstream, 0 = up to 1000; whole-millisecond steps, IMP-007), `r_trilinear` (default on), `r_anisotropy` 1–16 (default 4), `r_depthbits` 16/24 (default 24), MSAA `r_msaa` 0/2/4/8 (default off; resolved before scaling, screenshots and `rend_GetPixel` read the resolved frame). Also `r_renderscale` 0.5–2.0 (the game frame renders at that multiple of the resolution and is scaled to the window; menus keep 640x480), `r_gamma` (mirrors the brightness dialog) and the `r_restart` command, which rebuilds the frame so render scale, MSAA and depth bits apply at once. Verified in level 1. Known: HUD text changes size with render scale (the game picks HUD fonts from a few fixed sizes by frame width; see IMP-005). The Graphics page came with F-069 (2026-11-03). Retro CRT filter noted in IMP-006.
**Notes:** Exact option list confirmed against the OpenGL renderer (`renderer/HardwareOpenGL.cpp`) at the start of Phase 2. D-013.

### F-069 Options menu overhaul
**Priority:** Should
**Acceptance:**
- One Options screen with pages: Graphics (F-068, F-060), Controls (F-067, F-062, F-063, upstream bindings), Audio (upstream), Gameplay, Tartarus (Classic Mode toggle, F-002)
- Every page is generated from cvar metadata (type, range, description) so new settings appear without new UI code
- Works with mouse, keyboard and gamepad; readable at 1080p and above
- Upstream's existing option dialogs remain reachable until each is replaced
**Status:** In progress (Phase 2). Step 1 done 2026-11-03: the options deck (`deck.cpp`, D-016) opens on F2 in a single-player level in Tartarus mode, with Graphics, Controls, Audio, Gameplay and Tartarus pages generated from cvar metadata, keyboard and mouse input, live changes, the game paused while open, and a Stock options entry. Classic Mode and multiplayer keep the stock Options menu. Step 2 done 2026-11-03: main menu Options opens the deck too, in its own loop over the menu, and saves the stock-owned settings on exit. Resolution and fullscreen are on the Graphics page (`r_resolution`, `r_fullscreen`), applied when the deck closes, in a level or from the main menu. The owner confirmed the resolution survives a restart (2026-11-03). Step 3 in progress 2026-11-04: the remaining menus move to the deck, using one reusable list screen (`Deck_RunList()`) with Yes/No, message and text-entry prompts. Done: main menu, Save and Load (`deck_save.cpp`, with the save's screenshot, BUG-013), every message and text-entry box, Keys, the Guide-Bot menu, New Game, level warp and Pilots (`deck_screens.cpp`), and the keyboard, joystick and mouse bindings and weapon order (`deck_controls.cpp`), also reached from a Key bindings row on the Controls page. Deck screens render at the chosen resolution with the art scaled to fill; stock screens not yet moved keep the 640x480 frame, because TelCom, multiplayer and the controls screen draw at fixed coordinates. Still stock: the pilot's ship, picture and copy-controls screens, the Adjust settings dialogs, the post-level results and all of multiplayer; TelCom stays as it is. Gamepad later (owner, 2026-11-04). The stock menu's Tartarus tab (`options_tartarus.cpp`) stays reachable through Stock options.
**Notes:** A new Tartarus menu layer, not `newui` (D-016, replacing the original plan). D-013. See SPEC.md §15. Includes the widescreen menu canvas deferred from F-060 (owner, 2026-11-01): menus drawn on a canvas as wide as the window, art scaled to fill, every 640x480-assuming screen re-centred.

---

### G. AI

### F-070 AI reactions to Tartarus tools
**Priority:** Should
**Acceptance:**
- Robots caught in a gravity well attempt to thrust away if they can; a decloak flash within sight triggers an investigate behaviour; stunned state exists for F-026
**Status:** Not started (Phase 6)
**Notes:** See also AV-007.

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
