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
│  cheatmenu.cpp   password unlock table, per-frame hold loop          │
│  datadir.cpp     Steam/data-dir discovery                            │
│  weapons_sidecar.cpp  loads data/tartarus/weapons.txt → Weapons[]    │
│  cloak.cpp       player cloak state machine + HUD gauge              │
│  gravwell.cpp    gravity-missile well, repulsor                      │
│  converter.cpp   energy↔shield                                       │
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

**classic.cpp.** Owns the `classic` cvar and `IsClassic()`. Every Tartarus module's per-frame entry point begins `if (IsClassic()) return;`. Also forces `CLASSIC_LOCKED` cvars to default when the mode flips, and stamps the mode into save-game headers.

**cvar.cpp.** A flat registry of named, typed values with flags (`ARCHIVE`, `CHEAT`, `CLASSIC_LOCKED`, `LATCH`). Loaded from and saved to `tartarus.cfg` in the writable base directory (the `-setdir` path). Exposes `Cvar_Get/Set/Register` and change callbacks. Does not depend on the console.

**console.cpp.** Input capture (hooks the key path in `GameLoop.cpp` ahead of `DemoCheats()`), a scrollback buffer, command dispatch, tab completion over commands + cvars, history persisted to `console_history.txt`, and a plog appender that mirrors engine logging. Rendering uses `grtext` and the 2D primitives already used by `hud.cpp`. Owns no game state.

**cheatmenu.cpp.** A `newui` dialog with a text field and toggle list. Maintains an `unlocked` bitmask per pilot session, the password table (SPEC.md), and a per-frame `CheatHoldLoop()` called from the player think path that pins energy/shields/ammo/afterburner at max for active infinite toggles. Sets upstream `IsCheater` on first unlock; refuses when `IsClassic()` or `Game_mode & GM_MULTI`.

**datadir.cpp.** Called early in `init.cpp` if no `-additionaldir` was given. Probes Steam library paths and `libraryfolders.vdf`, validates presence of `d3.hog`, injects the directory as a read-only base directory through the same mechanism `-additionaldir` uses.

**weapons_sidecar.cpp.** Parses `data/tartarus/weapons.txt` after the table file has loaded (`manage/` page loading complete), allocates slots with `AllocWeapon()`, fills the `weapon` struct fields (model, fire delay, damage, flags, sounds — referencing existing HOG assets by name), and registers each new weapon with the player ship's weapon lists. Weapon *behaviour* that no existing flag covers is implemented in the specific module (e.g. `gravwell.cpp`) and selected by a Tartarus-only flag bit range (SPEC.md §Weapon flags).

**cloak.cpp.** State machine (`READY → ACTIVE → COOLDOWN → READY`) on a per-player struct kept *alongside* `Players[]` (not inside it, to avoid changing save layout — AV-003). Calls `MakeObjectInvisible()` / clears the invisibility flags on transition. Exposes `Cloak_Break(reason)` for the two hooks. Draws its gauge through a HUD item registered in `hud.cpp`.

**gravwell.cpp.** Registers a new fireball index (`TARTARUS_GRAVWELL_INDEX`) and a `DoGravWellFrame(object*)` dispatched from `fireball.cpp::DoExplosionFrame`. Implements the pull: iterate room/terrain-neighbourhood objects, compute linear-falloff force, scale by mass, skip `GW_IMMUNE`, `phys_apply_force()`. Deliberately does **not** set `PF_DESTINATION_POS` the way the stock field does (that pins objects to the centre and is the main wall-clip risk). Repulsor is the same function with negative force and no spawned object.

**converter.cpp.** Two held keys; per-frame transfer with ratio loss; clamps.

**orient_hud.cpp.** Computes the level up vector (`Gravity_direction`-equivalent outdoors, world +Y indoors), projects it into view space each frame, and renders the up-tick on the reticle, the attitude ball and the gizmo as HUD items.

**autolevel_ext.cpp.** Wraps the autolevel block in `physics/physics.cpp` (around `MAX_AUTOLEVEL_TILT_ANGLE`) with strength/delay cvars and adds hold/snap behaviours that inject roll toward upright, cancelled by any player roll input.

**automap_ext.cpp.** Visited/seen room sets, marker filters, 2D projection, orientation glyph; `TelComAutoMap.cpp` calls into it at its draw and input points.

**debugdraw.cpp.** Immediate-mode line/label helpers and the three `r_show*` cvars.

## Hook points in upstream code

Kept deliberately few and each marked `// TARTARUS:` so upstream merges are greppable.

| Upstream file | Hook | Purpose |
|---|---|---|
| `Descent3/GameLoop.cpp` (key handling near `DemoCheats(key)`) | console key intercept; `CheatHoldLoop()`; Tartarus per-frame update | console, cheats, cloak/converter ticks |
| `Descent3/init.cpp` | `Tartarus_Init()` after cvar/arg parsing; `DataDir_Probe()` | startup |
| `Descent3/WeaponFire.cpp::FireWeaponFromPlayer`, `FireFlareFromPlayer` | `Cloak_Break(CLOAK_BREAK_FIRE)`; overcharge later | cloak |
| `physics/collide.cpp` (`collide_generic_and_player` via `DO_COLLISION(OBJ_ROBOT, OBJ_PLAYER, …)`) | `Cloak_Break(CLOAK_BREAK_BUMP)` + alert | cloak |
| `Descent3/fireball.cpp::DoExplosionFrame` | dispatch `TARTARUS_GRAVWELL_INDEX` | gravity missile |
| `Descent3/WeaponFire.cpp` (weapon-vs-wall impact, where `WF_GRAVITY_FIELD` is handled ~line 3078) | spawn grav well for Tartarus flag | gravity missile |
| `Descent3/hud.cpp::RenderHUDItems` + `HUD_ITEM_*` | register cloak gauge, orientation items, CHEATS tag | HUD |
| `Descent3/gauges.cpp` | cockpit-mode variants if cockpit view is kept | HUD |
| `physics/physics.cpp` (autolevel block) | strength/delay/hold/snap | orientation |
| `Descent3/TelComAutoMap.cpp` | draw/input calls into `automap_ext` | automap |
| `Descent3/gamesave.cpp` / `Descent3/loadstate.cpp` | mode stamp + Tartarus sidecar chunk | saves |
| `Descent3/menu.cpp` / `Descent3/newui.cpp` options | Tartarus options page, cheat menu entry | UI |
| `Descent3/CMakeLists.txt` | add `tartarus/` sources | build |

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
