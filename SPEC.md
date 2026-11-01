# Spec

Authoritative technical reference for Tartarus. Values here are **starting points for tuning**; the console exists so they can be changed live (F-010/F-011). When a value is retuned, change it here in the same commit (Maintenance Rule 7). Engine references are to upstream commit `fa61eec`.

Engine units: Descent 3 world units (the Pyro-GX is roughly 4 units across), seconds, and the player energy/shield scale where `INITIAL_ENERGY = 100`, `MAX_ENERGY = 200`, `MAX_SHIELDS = 200` (`player_external.h`), afterburner `AFTERBURN_TIME = 5.0` s.

---

## 1. Modes

| Cvar | Type | Default | Flags | Meaning |
|---|---|---|---|---|
| `classic` | bool | 1 | ARCHIVE | Classic Mode. When 1, every `CLASSIC_LOCKED` cvar reads as its default and every Tartarus module early-outs. |

Command-line: `-classic` forces `classic 1` for the session; `-tartarus` forces 0.

Save-game sidecar chunk: `TART` magic, `uint16 version`, `uint8 classic`, then module records (`CLOK`, `CVAR`) each length-prefixed. Unknown records are skipped.

---

## 2. Cvar system

Flags: `ARCHIVE` (persisted to `tartarus.cfg`), `CHEAT` (requires matching unlock or Debug build), `CLASSIC_LOCKED` (forced to default when `classic 1`), `LATCH` (new value applies on next level load), `READONLY`.

Naming: lowercase, `snake_case`, prefixed by area:

| Prefix | Area |
|---|---|
| `cloak_`, `gw_`, `rp_`, `conv_`, `dash_`, `ping_`, `flak_`, `ric_`, `emp_`, `oc_`, `tether_`, `decoy_` | weapons & systems |
| `hud_` | HUD elements |
| `al_` | auto-level |
| `map_` | automap |
| `r_` | rendering/debug draw |
| `m_` | mouse |
| `g_` | general gameplay |
| `con_` | console itself |
| `phys_` | physics timing (fixed timestep, D-012) |

`tartarus.cfg` format: one `set <name> <value>` per line, `//` comments, written atomically (temp + rename).

---

## 3. Console

**Toggle key:** `` ` `` (SDL scancode GRAVE); rebindable via `con_key`. **Layout:** top 45 % of screen (`con_height 0.45`), slides over `con_anim_time 0.15` s. **Font:** the HUD font via `grtext`. **Scrollback:** 2000 lines (`con_lines`). **History:** 256 entries persisted to `console_history.txt`.

Grammar:

```
line      := command (';' command)*
command   := name arg*                 // name is a command or a cvar
arg       := word | '"' chars '"'
name alone, if cvar  → prints "name = value (default d) — description"
name value, if cvar  → sets (respecting CHEAT / CLASSIC_LOCKED / READONLY)
```

Built-in commands (Phase 1):

| Command | Effect |
|---|---|
| `help [name]` | list commands, or describe one |
| `find <substr>` | list commands/cvars containing substring |
| `cvarlist [prefix]` | dump cvars with values and flags |
| `exec <file>` | run a script from the writable base dir |
| `bind <key> "<line>"` / `unbind <key>` | console-script keybinds (layered over upstream controls) |
| `classic <0|1>` | mode switch (prompts if a level is loaded) |
| `map <mission.mn3> <level>` | CHEAT. Level warp |
| `give <weapon-name|all|energy|shields|ammo>` | CHEAT |
| `spawn <object-name> [count]` | CHEAT. Spawns ahead of the player |
| `god <0|1>`, `noclip <0|1>` | CHEAT |
| `timescale <f>` | CHEAT. 0.05–4.0 |
| `screenshot` | PNG to `screenshots/` in the writable base dir |
| `cheat <password>` | same unlock table as the cheat menu (§4) |
| `quit` | |

Input capture: while open, the console consumes all keyboard events except the toggle key and `Escape`; mouse look is frozen; the game continues to run unless `con_pause 1`.

---

## 4. Cheat menu and passwords

**Open:** pause menu → Cheats, or `F12` (`g_cheatmenu_key`). **Refused** when `classic 1` or `Game_mode & GM_MULTI`, with the message "Cheats are not available in Classic Mode / multiplayer".

**Unlock semantics.** Passwords are letters only, case-insensitive. Entering a valid password unlocks that row for the current pilot session and sets `IsCheater = true`. Unlocked rows are toggles (infinite-type) or buttons (one-shot). `IsCheater` persists until the next *new game*; `CATHARSIS` relocks all rows and schedules the flag to clear at the next new game.

**Hold-at-max loop.** For each active infinite toggle, once per frame after player think: `energy = MAX_ENERGY`, `shields = MAX_SHIELDS`, `weapon_ammo[i] = max` for the relevant class, `afterburn_time_left = AFTERBURN_TIME`.

| Password | Kind | Effect |
|---|---|---|
| `CORNUCOPIA` | button | Give all: every weapon (stock + Tartarus) at max ammo; all keys |
| `AMBROSIA` | toggle | God mode: invulnerable **and** infinite energy, shields, ammo, afterburner |
| `AEGIS` | toggle | Infinite shields (held at 200) |
| `PROMETHEUS` | toggle | Infinite energy (held at 200) |
| `HYDRA` | toggle | Infinite secondary ammo (missiles, mines, countermeasures) |
| `MAGAZINE` | toggle | Infinite primary ammo (Vauss, Napalm, Mass Driver, Omega charge) |
| `ZEPHYR` | toggle | Infinite afterburner |
| `TITANHIDE` | toggle | Invulnerable only (shields still display damage flashes) |
| `SKELETONKEY` | button | All keys for the current level |
| `THANATOS` | button | Kill all robots in the level (guide-bot spared, as upstream) |
| `SHADE` | toggle | Cloak on/off (stock invisibility, no energy cost) |
| `WRAITH` | toggle | Noclip |
| `ARIADNE` | button | Full automap for the current level |
| `FERRYMAN` | button | Level warp (opens mission/level picker) |
| `ARGUS` | toggle | External/third-person camera |
| `HOURGLASS` | toggle | Frame-time and render statistics overlay |
| `TORPOR` | slider | Time scale 0.1–1.0 (slow motion) |
| `STYX` | button | Suicide (restart from last save) |
| `CATHARSIS` | button | Relock all cheats; clear `IsCheater` at next new game |

HUD tag: the string `CHEATS` in the top-right HUD corner, 50 % alpha, whenever `IsCheater` is true in Tartarus mode.

Debug builds (`CMAKE_BUILD_TYPE=Debug`) treat every row as pre-unlocked but still set `IsCheater` on use.

---

## 5. Weapon sidecar format (`data/tartarus/weapons.txt`)

Loaded after `manage/` table pages; each block allocates a slot with `AllocWeapon()`; names must not collide with `FindWeaponName()` results (AV-010). Assets are referenced by the names already present in the HOGs.

```
weapon "GravMissile" {
  class        secondary          // primary | secondary
  slot         7                  // player weapon list index to attach to (Tartarus range 5–9 secondary)
  model        "frag.oof"         // existing HOG model (placeholder until GPL art exists)
  tint         0.55 0.25 0.95
  fire_delay   1.2
  speed        70
  lifetime     6.0
  mass         2.0
  damage       15                 // direct impact only
  ammo_pickup  3
  ammo_max     12
  fire_sound   "WpnMicroMissileFire"
  impact_sound "WpnBlackSharkImpact"
  flags        TWF_GRAVWELL       // Tartarus flag; triggers gravwell.cpp on wall impact
  // module-specific keys are passed through as a key/value list:
  gw_radius    60
  gw_time      1.5
  gw_force     420
}
```

**Tartarus weapon flags** (stored in a side table keyed by weapon index, *not* in the upstream `weapon.flags` word):

| Flag | Module |
|---|---|
| `TWF_GRAVWELL` | gravwell.cpp |
| `TWF_FLAK` | flak.cpp |
| `TWF_RICOCHET` | ricochet.cpp |
| `TWF_EMP` | emp.cpp |
| `TWF_DECOY` | decoy.cpp |
| `TWF_TETHER` | tether.cpp |

---

## 6. Cloak (F-020)

State machine: `READY → ACTIVE → COOLDOWN → READY`.

| Cvar | Default | Notes |
|---|---|---|
| `cloak_cost` | 30 | energy charged on activation |
| `cloak_drain` | 8 | energy/s while ACTIVE |
| `cloak_min_energy` | 40 | activation refused below this |
| `cloak_max_time` | 8.0 | s; auto-drop |
| `cloak_cooldown` | 3.0 | s after a voluntary drop or timeout |
| `cloak_cooldown_forced` | 6.0 | s after fire/bump/damage break |
| `cloak_break_damage` | 10 | shield damage in one hit that breaks cloak; 0 = never |
| `cloak_break_on_flare` | 0 | 1 = flares also break cloak |
| `cloak_toggle` | 1 | 1 = press to toggle; 0 = hold to maintain |
| `cloak_bump_alerts` | 1 | bumped robot is alerted to the player |

Rules:
- Activation: `energy ≥ cloak_min_energy` and state `READY`; then `energy -= cloak_cost`; call `MakeObjectInvisible(Player_object, cloak_max_time, 0.25f, true)`.
- Each frame ACTIVE: `energy -= cloak_drain * Frametime`; if `energy ≤ 0` → drop (voluntary cooldown).
- `Cloak_Break(reason)` from `FireWeaponFromPlayer` (and `FireFlareFromPlayer` if `cloak_break_on_flare`), from the robot/player collision case in `physics/collide.cpp`, and from the damage path when `damage ≥ cloak_break_damage`. Forced breaks use `cloak_cooldown_forced`.
- Decloak visual: 0.25 s fade-in plus a brief flash sprite so a decloak is readable to the player (and later to AI, F-070).
- HUD gauge (`hud_cloak 1`): ring segment next to the afterburner gauge; blue = READY, white with sweep = ACTIVE (remaining fraction), red = COOLDOWN.

---

## 7. Gravity missile and repulsor (F-021, F-022)

Spawned on wall impact as fireball index `TARTARUS_GRAVWELL_INDEX` (allocated above the stock fireball table). Frame function `DoGravWellFrame(obj)`:

```
for each candidate object o within gw_radius of obj->pos
    (robots, player if gw_self, weapons if gw_pull_weapons; never doors/powerups):
  d      = max(0, |o.pos − obj.pos| − o.size)
  if d > gw_radius or o has GW_IMMUNE: continue
  fall   = 1 − d / gw_radius                       // linear falloff
  tnorm  = obj.lifeleft / gw_time                  // fades as the well expires
  F      = gw_force * fall * tnorm
  F     *= clamp(gw_mass_ref / o.mass, gw_mass_min_scale, 1.0)   // heavy robots budge, light ones fly
  dir    = normalize(obj.pos − o.pos)
  phys_apply_force(o, dir * F * Frametime * 60)    // force scaled to a nominal 60 Hz tick
```

No `PF_DESTINATION_POS` pinning (D-009); walls stop objects via normal collision (AV-006).

| Cvar | Default | Notes |
|---|---|---|
| `gw_radius` | 60 | world units; ≈ 2.5× a Frag splash |
| `gw_time` | 1.5 | s |
| `gw_force` | 420 | peak |
| `gw_mass_ref` | 4.0 | mass at which scale = 1 |
| `gw_mass_min_scale` | 0.15 | floor for very heavy robots |
| `gw_self` | 1 | pull the player too |
| `gw_self_radius` | 40 | |
| `gw_pull_weapons` | 1 | missiles curve in (fun, and a counter) |
| `gw_stun_on_arrive` | 0.6 | s of AI stun when a robot reaches < 1 radius of centre (needs F-070; 0 before then) |

`GW_IMMUNE`: per-robot-type flag set from `data/tartarus/robot_overrides.txt` (bosses and any robot with `mass > 40` by default).

Visuals: scaled `LIGHTNING_BOLT_INDEX` strands from centre to each pulled object (as the stock field does), a dark inward-moving particle ring, screen shake ∝ proximity. Audio: reuse a pitched-down Black Shark loop (placeholder).

**Repulsor** (`rp_`): no object spawned; one-shot application of the loop above with `dir` negated, centred on the player, radius `rp_radius 35`, impulse `rp_force 600`, cost `rp_cost 20` energy, cooldown `rp_cooldown 4.0`.

---

## 8. Energy ↔ shield converter (F-030)

Keys held: `conv_to_shields`, `conv_to_energy`. Per frame: `amt = conv_rate * Frametime`; source `-= amt`; destination `+= amt * conv_ratio`; never below `conv_floor` on the source or above the max on the destination.

| Cvar | Default |
|---|---|
| `conv_rate` | 25 /s |
| `conv_ratio` | 0.5 |
| `conv_floor` | 20 |

---

## 9. Orientation aids (F-040 – F-043)

**Up vector:** indoors, world +Y; outdoors, the negative of the terrain gravity direction used by the physics module. Blend over 0.5 s when crossing a room/terrain boundary.

**Reticle up-tick** (`hud_uptick 1`): project `up` into the view plane; angle θ = atan2(up_view.x, up_view.y). Draw a tick of length `hud_uptick_len 14` px on the reticle ring at θ. Alpha = `smoothstep(hud_uptick_fade_deg, hud_uptick_fade_deg*3, |roll|)` with `hud_uptick_fade_deg 8`, so it disappears when upright.

**Attitude ball** (`hud_attitude 0`): pitch = asin(forward · up), roll = atan2(right · up, up_ship · up). 64 px ball, horizon line, 10° pitch ladder, position from the HUD layout file.

**Gizmo** (`hud_gizmo 0`): 48 px, three axes in ship space, world +Y in white and brighter.

**Auto-level extensions** (`al_`): `al_strength 0.0–1.0` (default 0.3 in Tartarus, maps onto the upstream slider range), `al_delay 0.4` s after last roll input before leveling resumes, `al_hold` key: roll rate toward upright `al_hold_rate 180` °/s, `al_snap` key: roll to nearest of the four uprights over `al_snap_time 0.35` s with ease-out; any roll input cancels.

Budget: all orientation HUD items together ≤ 0.2 ms/frame at 1080p (ROADMAP Phase 3 acceptance).

---

## 10. Automap (F-050, F-051)

| Cvar | Default | Meaning |
|---|---|---|
| `map_visited_colour` | `0.2 0.8 1.0` | rooms the player has entered |
| `map_seen_colour` | `0.4 0.4 0.5` | rooms revealed but not entered |
| `map_show_terrain` | 1 | |
| `map_filter` | `all` | comma list: `goals,keys,doors,powerups,robots` |
| `map_2d` | 0 | top-down projection with depth shading |
| `map_reanchor_time` | 1.0 | s the enlarged up-tick shows after closing the map |

Visited set: room index added on entry; persisted in the save sidecar `AMAP` record as a bitset.

---

## 11. Modernisation constants (Phases 2 and 5)

`r_fov 90` (60–120, affects 3D view only), `m_sensitivity 1.0`, `m_accel 0`, `g_quicksave_key F5`, `g_quickload_key F9`, `g_checkpoints 1`. Fixed-timestep proposal (D-012): `phys_fixedstep 0|1`, `phys_hz 120`, render interpolation between the last two physics states.

---

## 12. Keybind defaults (Tartarus mode)

| Action | Default |
|---|---|
| Console | `` ` `` |
| Cheat menu | F12 |
| Cloak | C |
| Repulsor | V |
| Convert → shields / → energy | [ / ] |
| Auto-level hold | Left Alt |
| Snap to up | Z |
| Quicksave / quickload | F5 / F9 |
| Mouse recentre (F-067) | Middle mouse button |

All bindable in the upstream controls UI; none active in Classic Mode (the console remains available in Classic for non-cheat commands).

---

## 13. Mouse flight modes (F-067)

| Cvar | Default | Notes |
|---|---|---|
| `m_mode` | 1 in Tartarus, 0 in Classic | 0 Classic (upstream), 1 virtual joystick, 2 virtual joystick with auto-centring. `CLASSIC_LOCKED` |
| `m_vj_deadzone` | 0.04 | fraction of the full-rate radius with zero output |
| `m_vj_radius` | 0.20 | full-rate radius as a fraction of screen height |
| `m_vj_curve` | 1.8 | response exponent: `out = sign(x) · ((|x| − dz)/(1 − dz))^curve`, clamped to 1 |
| `m_vj_centre_rate` | 0.0 (mode 2: 1.5) | per second; offset decays as `offset *= exp(−rate · Frametime)` |
| `m_vj_sensitivity` | 1.0 | scales raw mouse counts into offset before clamping |
| `m_vj_roll_x` | 0 | 1 = mouse X drives roll instead of yaw |
| `m_vj_invert_y` | 0 | |
| `m_vj_indicator` | 1 | draw offset dot, line to reticle and dead-zone ring |

Offset model: each frame `offset += mouse_delta · m_vj_sensitivity / (m_vj_radius · screen_h)` per axis, the offset vector is clamped to length 1, and the curve output feeds the pitch and heading axes as an analogue rate in −1…1 (the same range a joystick axis produces). Offset resets to zero on: recentre key, console/menu/automap open, window focus loss, death, level load (AV-013).
