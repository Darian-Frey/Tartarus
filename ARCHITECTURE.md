# Architecture

Descriptive only; rationale lives in DECISIONS.md. File and symbol names below were checked against upstream (https://github.com/DescentDevelopers/Descent3) commit `fa61eec` (2026-11-01); upstream moves, so re-verify with `grep` before relying on a line number.

## System overview

```
┌──────────────────────────────────────────────────────────────────────┐
│ Upstream Descent 3 engine (unchanged layout)                         │
│  Descent3/   game loop, player, weapons, HUD, AI, automap, cheats    │
│  physics/    movement, collision (collide.cpp), autolevel            │
│  renderer/   OpenGL; grtext/ 2D text; ui/ newui/ menus               │
│  manage/     table file (table.gam) page loaders                     │
│  ddio/ cfile/ sndlib/ libmve/ music/ networking/ …                   │
└──────────────┬───────────────────────────────────────────────────────┘
               │ hook points (≈12 small edits, listed below)
┌──────────────▼───────────────────────────────────────────────────────┐
│ Descent3/tartarus/   (all additions; compiled into the Descent3 exe) │
│  classic.cpp     IsClassic() — the one gate every module checks      │
│  cvar.cpp        typed registry, persistence, flags                  │
│  console.cpp     drop-down console, commands, plog sink              │
│  cmdparse.cpp    console/config line tokenizer (SPEC §3 grammar)     │
│  console_core.cpp  scrollback, editing, history, completion          │
│  tartarus.cpp    Tartarus_Init(): cvar policy, cfg load, modules     │
│  cheats.cpp      cheat menu, hold loop (+ cheat_table.cpp)           │
│  datadir.cpp     Steam/data-dir discovery (+ datadir_steam.cpp)      │
│  version.cpp     build string, upstream hash (F-004)                 │
│  display.cpp     resolution by size, session-only -windowed          │
│  weapons_sidecar.cpp  loads data/tartarus/weapons.txt → Weapons[]    │
│  cloak.cpp       player cloak state machine + HUD gauge              │
│  gravwell.cpp    gravity-missile well, repulsor                      │
│  converter.cpp   energy↔shield                                       │
│  mouse_flight.cpp  virtual-joystick mouse modes (F-067)              │
│  orient_hud.cpp  up-tick, attitude ball, gizmo                       │
│  autolevel_ext.cpp  hold/snap/strength on top of physics autolevel   │
│  automap_ext.cpp overlay logic used by TelComAutoMap.cpp             │
│  debugdraw.cpp   r_show* overlays                                    │
└──────────────────────────────────────────────────────────────────────┘
               │ reads
┌──────────────▼───────────────────────────────────────────────────────┐
│ data/tartarus/   weapons.txt, hud_layout.txt (fork-owned, GPL)       │
│ <Steam dir>/     d3.hog, missions/, movies/ (user-owned, never ours) │
└──────────────────────────────────────────────────────────────────────┘
```

## Module responsibilities

**classic.cpp.** Owns the `classic` cvar and `IsClassic()` (a cached bool, safe per frame). Every Tartarus module's per-frame entry point begins `if (IsClassic()) return;`. `-classic` / `-tartarus` set a session-only override that is never written to `tartarus.cfg` and is dropped as soon as the user changes `classic`. `CLASSIC_LOCKED` cvars are enforced by the registry on the read side (they *read* as their default while Classic is on, so the user's value survives the round trip). Stamping the mode into save games arrives with the save sidecar (AV-003).

**cvar.cpp / cmdparse.cpp.** `CvarRegistry`: a flat registry of named, typed values (bool/int/float/string, min/max, description) with flags `ARCHIVE`, `CHEAT`, `CLASSIC_LOCKED`, `LATCH`, `READONLY`. API: `Cvars().RegisterBool/Int/Float/String`, `Find`, `List(prefix)`, `Set(name, value, source)`, `ApplyLatched()`, change callbacks; reads are cached numbers, so per-frame use is free. Persisted to `tartarus.cfg` in the writable base directory (SDL's preference path; `-setdir` no longer exists upstream): only non-default `ARCHIVE` values, written atomically after each user change; values for modules not yet registered are kept and applied when they register. Engine dependencies (classic flag, cheat policy, log, path) are injected through `CvarPolicy`, so both files are pure C++ and unit-tested. Does not depend on the console.

**console_core.cpp / console.cpp.** `ConsoleCore` (pure C++, unit-tested) holds the scrollback (thread-safe `Print`, drained by `Pump` on the game thread), line editing, history (256), tab completion over commands and cvars, command registry with a `CMD_CHEAT` flag, dispatch (`name` describes a cvar, `name value` sets it) and `exec`. `console.cpp` is the engine glue: `Console_HandleKey()` is the first thing `ProcessKeys()` does with every key (AV-008); opening calls upstream `SuspendControls()` so the ship gets no input, closing calls `ResumeControls()`, which flushes the key and mouse queues; `Console_Render()` draws after the HUD in `GameRenderFrame()` with `rend_FillRect` + `grtext` (BRIEFING_FONT, ASCII only) and re-suspends controls if a game event resumed them while open. A plog appender mirrors engine logging. Owns no game state.

**cheat_table.cpp / cheats.cpp.** `cheat_table.cpp` (pure C++, unit-tested, AV-004) holds the SPEC §4 password table and `CheatState`: per-pilot-session unlocks, active toggles, time scale, and `Holds()`, which folds god mode into the individual holds. `cheats.cpp` is the engine glue: an overlay menu drawn like the console (`rend_FillRect` + `grtext`, controls suspended while open; not `newui`, so it can sit over the running game), the effects (one-shot ones reuse what `GameCheat.cpp` does), and `Cheats_Frame()`, which holds energy/shields/ammo/afterburner at max, re-applies invulnerability and cloak, and undoes exactly what it applied when a hold ends. Sets upstream `IsCheater` and zeroes the score on unlock and use; refuses when `IsClassic()` (and switches everything off on the way in), in multiplayer (state forgotten, player object untouched) or outside a level. Draws the CHEATS tag itself, so no HUD-item hook is needed. Upstream's typed cheats are untouched.

**raw_mouse.cpp.** Raw mouse (F-062): `m_raw`, `m_sensitivity`, `m_accel`. Sets `Tartarus_mouse_every_frame` each frame (stock modes with `m_raw 1`, and always for the virtual joystick), scales the counts the stock mouse paths receive (fraction carried between batches; `raw_mouse_core.cpp`, unit-tested) and switches SDL's relative-mode system scale for `m_accel`. Early-outs in Classic Mode and multiplayer.

**graphics.cpp.** Graphics options (F-068): registers the `r_vsync*`, `r_maxfps`, `r_trilinear`, `r_anisotropy`, `r_depthbits` and `r_msaa` cvars, sets the frame cap (`Min_allowed_frametime`, unless `-framecap`), and installs `Tartarus_before_flip`, which copies the settings (VSync from the stock checkbox) into the renderer's `Tartarus_*` globals every frame, menus included. The renderer applies the swap interval when it changes, re-filters cached textures when filtering settings change, and builds the offscreen frame with the chosen depth and MSAA, resolving it before anything reads it. `r_gamma` mirrors the brightness setting; `r_restart` rebuilds the frame (screen mode plus a forced renderer re-init, as the Video tab does) at the start of the next frame. Render scale lives in `display.cpp`.

**small_view_layout.cpp / small_views.cpp.** Small-view windows in the HUD layout (F-060, SPEC §14). `small_view_layout.cpp` (pure, unit-tested against a verbatim copy of upstream's arithmetic) computes a window's rectangle from size, aspect, position and the "bigger" scale, clamped inside the 3D view, and returns upstream's exact rectangle at the defaults. `small_views.cpp` registers the `hud_sv_*` cvars and replaces the rectangle in `DrawSmallViews()`; cockpit monitors are untouched.

**orient_core.cpp / orient_hud.cpp.** Orientation aids and the flight HUD (F-040 – F-042, F-046, D-017). `orient_core.cpp` (pure, unit-tested) computes the up-tick, ball and gizmo geometry from world up along the ship's axes; `orient_hud.cpp` draws them from `RenderOverlays()`, and the flight HUD from a hook just before the cockpit model, so the frame covers it. Shapes are single `rend_DrawPolygon2D` calls with a small white bitmap, because the renderer has no flat 2D polygon and lines have no width.

**deck_core.cpp / deck.cpp.** The options deck (F-069, D-016). `deck_core.cpp` (pure, unit-tested) builds pages from cvar metadata plus a curated table, and computes each row's text and next value. `deck.cpp` draws it as an overlay with `rend_FillRect`, `rend_DrawLine` and scaled `grtext` in a 1280x720 layout, takes F2 in `HandleKey()` (no upstream hook for the in-level deck), runs its own loop over the menu scene from the main menu (`Deck_RunFromMenu()`, one hook in `menu.cpp`), handles keys and the mouse, and applies changes through the cvars; pausing comes from `UpdatePause()`. Classic Mode and multiplayer never reach it. `Deck_RunList()` is the reusable list screen for the other menus, with `Deck_Confirm()`, `Deck_Message()` and `Deck_RunText()`; `MainDeck_Run()` is the main menu, `deck_save.cpp` the Save and Load screens, `deck_controls.cpp` the bindings and weapon order, and `deck_screens.cpp` the rest; each stock screen asks there first and runs itself on `kDeckStock`. While one runs, `Display_NativeMenu()` switches the menu frame to the chosen resolution (about 35 ms) and back, because stock screens draw at fixed 640x480 coordinates.

**binds_core.cpp / binds.cpp.** Console key bindings (F-010). `binds_core.cpp` (pure, unit-tested) parses key names with `ctrl+`/`alt+`/`shift+`, keeps the table and writes `binds.cfg`; the engine key codes are passed in by `binds.cpp`, which registers `bind`/`unbind`, runs `binds.cfg` at start-up and, third in `HandleKey()` after the console and cheat menu, runs a bound line in a level.

**Tools.** `tools/check_xrefs.py` (IMP-001) checks the documents' stable-ID cross-references; `tools/classic_audit.py` (AV-001) checks that every hook is listed in `docs/CLASSIC_CHECKLIST.md`. `tools/run_probe.sh` with `tools/gamedrive.py` runs scripted, silent game tests in a throwaway copy of the player's profile (needs python-xlib and ImageMagick). Both run in the "Tartarus audits" GitHub workflow.

**tartarus.cpp.** `Tartarus_Init()` plus the per-frame dispatchers the GameLoop hooks call (`HandleKey`, `Frame`, `TimeScale`, `RenderOverlays`), so a new module never needs a new hook in the loop.

**datadir.cpp / datadir_steam.cpp.** `DataDir_Probe()` runs in `InitIOSystems()` after the user, config and platform base directories are added, and acts only if none of them provides `d3.hog` (so `-additionaldir` always wins). It probes the native, Flatpak and Snap Steam roots and every library in their `libraryfolders.vdf`, keeps directories holding `d3.hog`, de-duplicates by canonical path, and adds the first with `cf_AddBaseDirectory()`, the mechanism `-additionaldir` uses. `DataDir_RequireHog()` stops with an error naming the file and every directory searched when `d3.hog` fails to open. `datadir_steam.cpp` is pure standard C++ and unit-tested (`Descent3/tartarus/tests/`). Runs in Classic Mode too: it is a compatibility fix.

**display.cpp / display_prefs.cpp.** Display settings that survive restarts (compatibility fix, active in Classic). Saves the resolution as `RS_width`/`RS_height` as well as upstream's list index, and restores it by size once `ConfigureDisplayResolutions()` has built the display's mode list (BUG-004). Records the saved fullscreen preference in `Tartarus_Init()`, before `rend_InitWindowMode()` applies `-windowed`/`-fullscreen`, and rewrites `RS_fullscreen` at save so those flags are session-only (BUG-005). Decision rules are pure and unit-tested.

**version.cpp.** Build identification. `tartarus.cmake` regenerates `lib/tartarus_version.h` every build from `TARTARUS_VERSION`, `git describe --match "tartarus-v*"` (so upstream's `v1.x` tags never label our build) and the committed `Descent3/tartarus/UPSTREAM_REF`, which `tools/upstream-sync.sh --apply` updates. `DrawMenuBuildString()` replaces the upstream main-menu version line.

**weapons_sidecar.cpp.** Parses `data/tartarus/weapons.txt` after the table file has loaded (`manage/` page loading complete), allocates slots with `AllocWeapon()`, fills the `weapon` struct fields (model, fire delay, damage, flags, sounds — referencing existing HOG assets by name), and registers each new weapon with the player ship's weapon lists. Weapon *behaviour* that no existing flag covers is implemented in the specific module (e.g. `gravwell.cpp`) and selected by a Tartarus-only flag bit range (SPEC.md §Weapon flags).

**cloak.cpp.** State machine (`READY → ACTIVE → COOLDOWN → READY`) on a per-player struct kept *alongside* `Players[]` (not inside it, to avoid changing save layout — AV-003). Calls `MakeObjectInvisible()` / clears the invisibility flags on transition. Exposes `Cloak_Break(reason)` for the two hooks. Draws its gauge through a HUD item registered in `hud.cpp`.

**gravwell.cpp.** Registers a new fireball index (`TARTARUS_GRAVWELL_INDEX`) and a `DoGravWellFrame(object*)` dispatched from `fireball.cpp::DoExplosionFrame`. Implements the pull: iterate room/terrain-neighbourhood objects, compute linear-falloff force, scale by mass, skip `GW_IMMUNE`, `phys_apply_force()`. Deliberately does **not** set `PF_DESTINATION_POS` the way the stock field does (that pins objects to the centre and is the main wall-clip risk). Repulsor is the same function with negative force and no spawned object.

**converter.cpp.** Two held keys; per-frame transfer with ratio loss; clamps.

**mouse_flight.cpp.** Virtual-joystick mouse modes (F-067, SPEC.md §13). Accumulates raw mouse deltas into a clamped 2-D offset, applies dead zone, curve and optional auto-centring, and supplies the result as an analogue pitch/heading (or roll) rate in −1…1, the range a joystick axis produces, so the flight model is untouched. Resets the offset on every context change (AV-013) and draws the offset indicator as a HUD item. The pure model is `mouse_flight_core.cpp` (unit-tested). Mouse counts arrive through a filter pointer called from `sdlgameController::mouse_geteval()`; while the mode is active the filter keeps the counts and hands upstream zero, so neither mouselook nor the mouse axis turns the ship. `MouseFlight_Apply()` runs in `DoMovement()` right after the controller movement and adds the output where a joystick axis would go. The indicator is drawn in `RenderOverlays()` with line segments (the GL renderer's circle functions are empty stubs). Early-outs to upstream behaviour when `m_mode 0`, `IsClassic()` or multiplayer.

**orient_hud.cpp.** Computes the level up vector (`Gravity_direction`-equivalent outdoors, world +Y indoors), projects it into view space each frame, and renders the up-tick on the reticle, the attitude ball and the gizmo as HUD items.

**autolevel_ext.cpp.** Wraps the autolevel block in `physics/physics.cpp` (around `MAX_AUTOLEVEL_TILT_ANGLE`) with strength/delay cvars and adds hold/snap behaviours that inject roll toward upright, cancelled by any player roll input.

**automap_ext.cpp.** Visited/seen room sets, marker filters, 2D projection, orientation glyph; `TelComAutoMap.cpp` calls into it at its draw and input points.

**debugdraw.cpp.** Immediate-mode line/label helpers and the three `r_show*` cvars.

## Hook points in upstream code

Kept deliberately few and each marked `// TARTARUS:` so upstream merges are greppable.

| Upstream file | Hook | Purpose |
|---|---|---|
| `Descent3/GameLoop.cpp` | `tartarus::HandleKey()` first in the `ProcessKeys()` loop (console, options deck, cheat menu, key bindings); `tartarus::RenderOverlays()` after the HUD in `GameRenderFrame()`; `tartarus::Frame()` after `DoPlayerFrame()` in `GameFrame()`; `Frametime *= tartarus::TimeScale()` after `CalcFrameTime()`; `tartarus::PausedFrame()` in the paused branch of `GameFrame()` (keys while the console/cheat menu holds the pause, IMP-004) | console, cheats, later cloak/converter ticks |
| `Descent3/init.cpp` | `DataDir_Probe()` at upstream's Steam TODO in `InitIOSystems()`; `DataDir_RequireHog()` after `cf_OpenLibrary("d3.hog")`; `Tartarus_Init()` right after `InitIOSystems()` in `InitD3Systems1()`; `Display_SaveSettings()` after the `RS_fullscreen` write in `SaveGameSettings()` | startup, display settings |
| `Descent3/init.cpp::LoadGameSettings` (RS_fov clamp), `Descent3/config.cpp` video sheet (FOV slider range and apply) | `kFovAbsMin/Max`, `Display_FovMin/Max()`, `Display_SetFov()` | FOV range (F-060) |
| `renderer/HardwareOpenGL.cpp::rend_Flip` (+ `TartarusDrawBarFill`, `Tartarus_bar_fill_*` globals), `renderer/dyna_gl.h` (`glFramebufferTexture2D`) | blurred fill instead of the black clear when the frame is narrower than the window | menu bars (F-060) |
| `Descent3/gamesequence.cpp` (level start), `Descent3/gamecinematics.cpp::Cinematic_Frame` | `SkipBriefing()`, `SkipCinematics()` (command-line `-skipbriefing`, `-skipcinematics`) | test runs |
| `renderer/HardwareOpenGL.cpp` (F-068 block, `rend_Flip`, FBO creation and teardown, `opengl_MakeFilterTypeCurrent`, `rend_GetPixel`, `rend_Screenshot`), `renderer/dyna_gl.h` (`glRenderbufferStorageMultisample`) | `Tartarus_before_flip` callback and `Tartarus_*` settings: swap interval, trilinear, anisotropy, depth bits, MSAA with a resolve target | graphics options (F-068), BUG-010 |
| `Descent3/game.cpp::SetScreenMode` (SM_GAME) | `Display_ScaleRenderSize()` on the frame size, `Display_AdjustGameWindow()` before `InitGameScreen()` | render scale (F-068) |
| `renderer/ShaderProgram.h::AddVertexData`, `renderer/HardwareOpenGL.cpp` (`addVertexData`) | vertices written with `glBufferSubData`; only the used vertices are sent | garbage walls after explosions (BUG-012) |
| `Descent3/config.cpp::ConfigureDisplayResolutions` | `Display_RestoreResolution()` replaces the index-based restore | display settings (BUG-004) |
| `Descent3/TelCom.cpp` (`TelComShow`, `TelComHandleAllEvents`), `Descent3/TelComGoals.cpp` | menu-scale virtual mouse space and position reads (`kUiMouseScale`) | TelCom pointer speed (BUG-006) |
| `Descent3/Controls.cpp::DoMovement` | `MouseFlight_Apply()` after `DoControllerMovement()` | virtual-joystick mouse (F-067) |
| `Descent3/Controls.cpp` (flare button read) | flare packet zeroed when `MouseFlight_ReservesButton()` | middle button recentres only (F-067) |
| `Descent3/config.cpp::OptionsMenu` | `tartarus::TartarusOptionsSheet` setup/finish beside upstream's sheets | Options → Tartarus tab: mouse flight and rear-view mirror (F-067, F-060, F-069) |
| `ddio/lnxmouse.cpp` (`sdlMouseButtonDownFilter`, `sdlMouseButtonUpFilter`) | swap SDL's middle (2) and right (3) button numbers | mouse buttons (BUG-007) |
| `ddio/sdlcontroller.cpp::mouse_geteval` | `Tartarus_mouse_filter` pointer (defined there, null = upstream) called on each batch of mouse counts; `Tartarus_mouse_every_frame` bypasses the 20 Hz sampling gate | virtual-joystick mouse (F-067), raw mouse (F-062) |
| `Descent3/mmItem.cpp::mmInterface::CopyrightText` | `DrawMenuBuildString()` early return | build string (F-004) |
| `Descent3/WeaponFire.cpp::FireWeaponFromPlayer`, `FireFlareFromPlayer` | `Cloak_Break(CLOAK_BREAK_FIRE)`; overcharge later | cloak |
| `physics/collide.cpp` (`collide_generic_and_player` via `DO_COLLISION(OBJ_ROBOT, OBJ_PLAYER, …)`) | `Cloak_Break(CLOAK_BREAK_BUMP)` + alert | cloak |
| `Descent3/fireball.cpp::DoExplosionFrame` | dispatch `TARTARUS_GRAVWELL_INDEX` | gravity missile |
| `Descent3/WeaponFire.cpp` (weapon-vs-wall impact, where `WF_GRAVITY_FIELD` is handled ~line 3078) | spawn grav well for Tartarus flag | gravity missile |
| `Descent3/hud.cpp::RenderHUDItems` + `HUD_ITEM_*` | register cloak gauge, orientation items | HUD |
| `Descent3/gauges.cpp` | cockpit-mode variants if cockpit view is kept | HUD |
| `Descent3/SmallViews.cpp::DrawSmallViews` (HUD/dormant-cockpit branch) | `SmallViews_Adjust()` after upstream computes the rectangle: size, aspect and position from `hud_sv_*`; no-op in Classic Mode | small views (F-060) |
| `physics/physics.cpp` (autolevel block) | strength/delay/hold/snap | orientation |
| `Descent3/TelComAutoMap.cpp` | draw/input calls into `automap_ext` | automap |
| `Descent3/gamesave.cpp` / `Descent3/loadstate.cpp` | mode stamp + Tartarus sidecar chunk | saves |
| `Descent3/menu.cpp` (`IDV_OPTIONS`) | `Deck_RunFromMenu()` before the stock `OptionsMenu()` | options deck from the main menu (F-069, D-016) |
| `Descent3/menu.cpp` (main menu loop) | `MainDeck_Run()` before `main_menu.DoUI()`, rows mapped to the `IDV_` ids | deck main menu (F-069, D-016) |
| `Descent3/mmItem.cpp::MenuScene` | `Deck_DrawMenuArt()` before `DrawLargeBitmap()` | menu art scaled to fill the frame (D-016) |
| `Descent3/game.cpp::SetScreenMode` | `Display_MenuSize()` after the fixed menu size | native menu frame while a deck screen runs (D-016) |
| `Descent3/newui.cpp` | `Deck_MessageBox()` first in `DoMessageBox()`, `Deck_EditBox()` first in `DoEditDialog()` | deck message and text-entry boxes (F-069, D-016) |
| `Descent3/ctlconfig.cpp::CtlConfig` | `ControlsDeck_Run()` after `LoadControlConfig()` | deck bindings and weapon order (F-069, D-016) |
| `Descent3/gamesequence.cpp::RunGameMenu` | `Deck_RunPendingControls()` before `OptionsMenu()` | Key bindings row of the in-level deck (F-069, D-016) |
| `Descent3/hud.cpp::RenderHUDFrame` | `OrientHud_RenderCanopy()` before `RenderCockpit()` in cockpit view | flight HUD behind the cockpit frame (F-046, D-017) |
| `Descent3/help.cpp::HelpDisplay` | `HelpDeck_Run()` | deck Keys screen (F-069, D-016) |
| `Descent3/buddymenu.cpp::BuddyDisplay` | `BuddyDeck_Pick()` after the menu data arrives | deck Guide-Bot menu (F-069, D-016) |
| `Descent3/pilot.cpp::PilotSelect` | `PilotDeck_Run()` | deck Pilots screen (F-069, D-016) |
| `Descent3/menu.cpp` (`MenuNewGame`, `DisplayLevelWarpDlg`) | `MissionDeck_Pick()`, `LevelDeck_Pick()` before each stock UI loop | deck New Game and level warp (F-069, D-016) |
| `Descent3/gamesave.cpp` | `SaveDeck_Run()` in `SaveGameDialog()`, `SaveDeck_PickLoad()` and `SaveDeck_SlotPath()` in `LoadGameDialog()` | deck Save and Load screens (F-069, D-016) |
| `.github/workflows/build.yml` | Windows, macOS, arm64-cross and Debug matrix entries commented out | Linux x64 Release only (D-014, D-015) |
| `.github/workflows/cd.yml` | push trigger replaced by `workflow_dispatch` and a weekly `schedule` | builds on demand (D-015) |
| `Descent3/CMakeLists.txt` | `include(tartarus/tartarus.cmake)` before `add_executable(Descent3)`: sources, version header, tests | build |

## Data flow for the signature sequence

```
key: cloak ─► cloak.cpp (READY→ACTIVE) ─► MakeObjectInvisible(Player_object)
key: fire  ─► FireWeaponFromPlayer ─► Cloak_Break(FIRE) ─► clear invisibility
           └► weapon object (gravmissile) ─► wall impact ─► CreateFireball(TARTARUS_GRAVWELL_INDEX)
                                  each frame ─► DoGravWellFrame ─► phys_apply_force(robot)
                                  robot physics ─► slides round corner ─► visible
```

## Key invariants

1. **Classic is classic.** No Tartarus module executes gameplay-affecting code when `IsClassic()` is true. Checked by AV-001.
2. **No upstream struct layout changes.** New per-player / per-object state lives in Tartarus-owned side tables keyed by object handle or player number. Save files gain a separately versioned sidecar chunk. (AV-003)
3. **Hook points are marked and minimal.** Every edit outside `Descent3/tartarus/` carries a `// TARTARUS:` comment; the table above is the complete list.
4. **Forces, not teleports.** Nothing in Tartarus writes an object's position directly; movement goes through `phys_apply_force` and the existing integrator. (AV-006)
5. **Cheats are loud.** Any unlocked cheat sets `IsCheater`, shows the HUD tag, and is refused in multiplayer.
6. **No game data in the repo.** `data/tartarus/` contains only fork-authored text and GPL-compatible assets.
7. **Every tunable is a cvar.** If a number is in SPEC.md it is reachable from the console.

## Cross-cutting concerns

- **Logging:** plog (upstream). Tartarus modules log under a `[tartarus]` prefix; the console mirrors the plog stream.
- **Threading:** none added. Everything runs on the game thread inside the existing loop.
- **Error handling:** data-file parse errors are logged and the offending entry skipped; a missing sidecar file is not an error (the game is then upstream + console).
- **Configuration:** `tartarus.cfg` (cvars) and `console_history.txt` in the writable base dir; upstream's own config untouched.
- **Multiplayer:** Tartarus features are disabled when `Game_mode & GM_MULTI` until a later phase defines a protocol (D-008).
