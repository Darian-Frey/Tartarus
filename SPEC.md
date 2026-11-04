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
| `ui_` | Tartarus menus |
| `s_` | audio |
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
| `bind <key> "<line>"` / `unbind <key\|all>` | console-script keybinds, saved to `binds.cfg` in the writable base dir and run at start-up. Key names: `a`–`z`, `0`–`9`, `f1`–`f12`, `kp0`–`kp9`, `kpplus`, `kpenter`, `space`, `up`, `pageup`, … with `ctrl+`, `alt+`, `shift+`. A bound key runs its line in a level when the console and cheat menu are closed, and its stock key action is skipped (held flight controls keep working). Backtick and Escape cannot be bound. `bind` alone lists, `bind <key>` shows one |
| `classic <0|1>` | mode switch (prompts if a level is loaded) |
| `map <mission.mn3> <level>` | CHEAT (FERRYMAN). Level warp; for now opens upstream's level picker, arguments pending |
| `give <weapon-name|all|energy|shields|ammo|keys>` | CHEAT (CORNUCOPIA). Weapon names as the HUD shows them, spaces optional (`give massdriver`) |
| `spawn <object-name> [count]` | CHEAT. Spawns ahead of the player |
| `god <0|1>`, `noclip <0|1>` | CHEAT (AMBROSIA; WRAITH). `noclip` pending |
| `timescale <f>` | CHEAT (TORPOR). 0.05–4.0 |
| `screenshot` | closes the console and calls upstream `DoScreenshot()` (upstream's location and format) |
| `cheat <password>` | same unlock table as the cheat menu (§4); no argument lists the rows and their state |
| `cheat use <password> [value]` | uses an unlocked row as the menu would: fires a button, flips a toggle, sets the TORPOR time scale (0.1–1.0) |
| `cheatmenu` | opens the cheat menu |
| `quit` | leave the game; shows the normal exit confirmation |
| `set <cvar> <value>`, `reset <cvar>`, `toggle <bool cvar>` | explicit forms of cvar assignment |
| `echo <text>`, `clear`, `version` | |
| `where` | ship room, position and heading/pitch/bank in degrees; also written to the log (for measuring) |
| `r_restart` | rebuild the game frame (in a level) so `r_renderscale`, `r_msaa` and `r_depthbits` apply now |

Input capture: while open, the console consumes every key (the toggle key and `Escape` close it); all ship controls are suspended and the key/mouse queues are flushed on close; with `con_pause 1` (default) the game pauses while the console is open in single-player (simulation, time, sound and music, via upstream `PauseGame()`), and keeps running with `con_pause 0` or in multiplayer. Editing keys: Left/Right, Home/End, Backspace/Delete, Up/Down history, Page Up/Down scroll, Tab complete, Ctrl+U clear line, Ctrl+L clear scrollback. Implemented cvars so far: `con_height`, `con_anim_time`, `con_lines`, `con_pause`; `con_key` (rebinding) pending.

---

## 4. Cheat menu and passwords

**Open:** `Shift+F12`, or `cheatmenu` in the console. (Stock F12 drops a marker; upstream has no pause menu, only a "paused" message box, so there is no menu entry.) Rebinding waits for `bind` (F-010). **Refused** when `classic 1` or `Game_mode & GM_MULTI`, or outside a level, with the message "Cheats are not available in Classic Mode" / "… in multiplayer".

**Menu keys.** Letters type the password; Enter submits it (and selects the unlocked row). With the password field empty: Up/Down select, Enter uses the row (toggle flips, button fires, slider resets to 1.0), Left/Right move the slider by 0.1. Escape or Shift+F12 closes. Ship controls are suspended while open, and the game pauses as for the console (`con_pause`).

**Unlock semantics.** Passwords are letters only, case-insensitive. Entering a valid password unlocks that row for the current pilot session and sets `IsCheater = true` (and zeroes the score, as upstream cheats do); so does every use. Unlocked rows are toggles (infinite-type) or buttons (one-shot). `IsCheater` persists until the next *new game* (upstream clears it there); active toggles and the time scale end at that point too, unlocks stay. Selecting a different pilot relocks everything. `CATHARSIS` relocks all rows, switches every toggle off, and does not itself set the flag; the flag then clears at the next new game.

**Hold-at-max loop.** For each active infinite toggle, once per frame after player think: `energy = MAX_ENERGY`, `shields = MAX_SHIELDS`, `weapon_ammo[i] = max` for the relevant class, `afterburn_time_left = AFTERBURN_TIME`; invulnerability (`PLAYER_FLAGS_INVULNERABLE`) and cloak (`MakeObjectInvisible(..., no_hud_message = true)`) re-applied if lost (death, new level). Skipped while the player is dying. When a hold ends, exactly what the loop applied is undone. Countermeasures are not held yet.

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
| `WRAITH` | toggle | Noclip (not implemented yet) |
| `ARIADNE` | button | Full automap for the current level |
| `FERRYMAN` | button | Level warp (opens mission/level picker) |
| `ARGUS` | toggle | External/third-person camera (not implemented yet) |
| `HOURGLASS` | toggle | Frame-time and render statistics overlay |
| `TORPOR` | slider | Time scale 0.1–1.0 (slow motion); `Frametime` is scaled after `CalcFrameTime()` |
| `STYX` | button | Suicide (restart from last save) |
| `CATHARSIS` | button | Relock all cheats; clear `IsCheater` at next new game |

HUD tag: the string `CHEATS` in the top-right HUD corner below the score, 50 % alpha, whenever `IsCheater` is true in Tartarus mode.

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

`r_vsync` (mirrors the Video tab checkbox, saved as `RS_vsync`), `r_vsync_adaptive 0`, `r_maxfps 60` (0 = up to 1000; whole milliseconds, so 144 gives ~166), `r_trilinear 1`, `r_anisotropy 4` (1–16, clamped to the driver), `r_depthbits 24` (16 = upstream), `r_msaa 0` (2/4/8), `r_renderscale 1.0` (0.5–2.0; game frame size = resolution × scale, menus unchanged), `r_gamma` (1.0–3.0, mirrors `RS_gamma`); depth, MSAA and render scale apply when the offscreen frame is rebuilt: console `r_restart`, a resolution change or the next level, all display options available in Classic Mode (D-013); `r_fov` (default 72 as upstream; 60–120 in Tartarus mode, 72–90 in Classic; affects the 3D view only; the horizontal FOV of a 4:3 view, wider windows see more at the sides; mirrors upstream's `RS_fov`, which saves it), `r_bar_fill 1` (0 black bars as upstream, 1 blurred copy around 4:3 menu screens, 2 around any frame narrower than the window; ARCHIVE, available in Classic Mode as a display option), `r_bar_fill_brightness 0.35` (0–1), `m_raw 0` (1 = read the mouse every frame for the stock mouse modes instead of upstream's 20 Hz; the virtual joystick is always read every frame; off by default until `m_sensitivity` is tuned for it), `m_sensitivity 1.0` (0.05–10, multiplies mouse counts for the stock mouse modes; try 3–4 with `m_raw 1`), `m_accel 0` (1 = desktop acceleration curve via SDL's relative-mode system scale), all `CLASSIC_LOCKED` and single-player only, `g_quicksave_key F5`, `g_quickload_key F9`, `g_checkpoints 1`. Fixed-timestep proposal (D-012): `phys_fixedstep 0|1`, `phys_hz 120`, render interpolation between the last two physics states.

---

## 12. Keybind defaults (Tartarus mode)

| Action | Default |
|---|---|
| Console | `` ` `` |
| Cheat menu | Shift+F12 (stock F12 drops a marker) |
| Cloak | C |
| Repulsor | V |
| Convert → shields / → energy | [ / ] |
| Auto-level hold | Left Alt |
| Snap to up | Z |
| Quicksave / quickload | F5 / F9 |
| Mouse recentre (F-067) | Middle mouse button (`m_vj_recentre_button`); also `m_recentre` |

All bindable in the upstream controls UI; none active in Classic Mode (the console remains available in Classic for non-cheat commands).

---

## 13. Mouse flight modes (F-067)

| Cvar | Default | Notes |
|---|---|---|
| `m_mode` | 1 in Tartarus, 0 in Classic | 0 Classic (upstream), 1 virtual joystick, 2 virtual joystick with auto-centring. `CLASSIC_LOCKED` |
| `m_vj_deadzone` | 0.04 | fraction of the full-rate radius with zero output |
| `m_vj_radius` | 0.20 | full-rate radius as a fraction of screen height |
| `m_vj_curve` | 1.8 | response exponent: `out = sign(x) · ((|x| − dz)/(1 − dz))^curve`, clamped to 1 |
| `m_vj_centre_rate` | 1.5 | per second, used in mode 2 only; offset decays as `offset *= exp(−rate · Frametime)` |
| `m_vj_sensitivity` | 1.0 | scales raw mouse counts into offset before clamping |
| `m_vj_roll_x` | 0 | 1 = mouse X drives roll instead of yaw |
| `m_vj_invert_y` | 0 | |
| `m_vj_indicator` | 1 | draw offset dot, line to reticle, full-rate ring and dead-zone ring (drawn at a fixed 12 % of the view height, whatever `m_vj_radius` is) |
| `m_vj_recentre_button` | 3 | mouse button that centres the stick; 0 = none, 3 = middle (which also fires a flare in stock bindings) |

All `ARCHIVE | CLASSIC_LOCKED`; inactive in multiplayer (D-008). Console command `m_recentre` centres the stick. While the virtual joystick is active, a flare binding on the recentre mouse button is ignored (stock binds flare to both the middle button and F; F keeps working).

**Options menu.** Options (F2, also from the main menu) → **Tartarus** tab (one tab for all Tartarus settings: upstream's option list holds eight buttons): mode (Classic / Virtual joystick / auto-centre), Speed slider 0.25–4.00 in 0.25 steps (`m_vj_sensitivity`), Centre speed slider 0.25–5.00 per second in 0.25 steps (`m_vj_centre_rate`; only the auto-centre mode uses it), Invert pitch, Show indicator, Left/right rolls. Values are written to the cvars, and so to `tartarus.cfg`, when the menu closes. In Classic Mode the tab only explains how to switch Classic Mode off.

Offset model: for each batch of mouse counts (upstream's controller reads the mouse at most 20 times a second) `offset += mouse_delta · m_vj_sensitivity / (m_vj_radius · screen_h)` per axis, the offset vector is clamped to length 1, and the curve output feeds the pitch and heading axes as an analogue rate in −1…1 (the same range a joystick axis produces). Offset resets to zero on: recentre input, console/cheat menu/menu/TelCom/automap open, pause, window focus loss, death, level load or respawn (a new player object), and any gap of more than 0.25 s in control reading (AV-013). Axis mapping is a joystick's: push forward to dive, pull back to climb (`m_vj_invert_y 1` flips it); the slide and bank toggle keys redirect the stick as they do a joystick, and level scripts' `PCBF_*` direction locks are honoured. While the mode is active, upstream's own mouse turning (mouselook or mouse axis) receives zero movement.

---

## 14. Small-view windows (F-060)

Applies to the HUD (non-cockpit) layout only; cockpit view keeps the cockpit monitors. `<w>` is `left` or `right`. All `CLASSIC_LOCKED` (Classic keeps the upstream layout, AV-001).

| Cvar | Default | Notes |
|---|---|---|
| `hud_sv_<w>_size` | 0.25 | window height as a fraction of the 3D view height (upstream: `Game_window_h / 4`) |
| `hud_sv_<w>_aspect` | 1.0 | width ÷ height; 1.0 = upstream square, e.g. 1.6 for a wide mirror |
| `hud_sv_<w>_x` | −1 | centre X as a fraction of the 3D view width (0 = left edge, 1 = right edge); −1 = upstream slot position |
| `hud_sv_<w>_y` | −1 | centre Y as a fraction of the 3D view height (0 = top); −1 = upstream (near the bottom) |
| `hud_sv_bigger_scale` | 1.25 | multiplier applied when a view has `SVF_BIGGER` (e.g. guided missile), 1.0–2.0; 1.25 reproduces upstream's `w += w / 4` |

Ranges: size 0.05–1.0, aspect 0.25–4.0, x and y −1 or 0–1. Placement is clamped so the whole window stays inside the 3D view; a window larger than the view is capped to it. With every value at its default the layout is pixel-identical to upstream (unit test against upstream's own arithmetic). Setting only `x` (or only `y`) leaves the other axis where upstream puts it. Options (F2) → Tartarus → **Rear-view mirror (F3 view)** sets the left window to a preset: Stock (all defaults), Top left (`size 0.18`, `aspect 2.0`, `x 0.18`, `y 0.15`) or Top right (`x 0.82`, otherwise the same); the mirrors sit below the Score and Guide-Bot text. When the left window's values match no preset, a fourth choice "Custom (console)" is shown and selected, and the menu leaves the values alone unless another choice is picked. Shift+F1 still decides what the left window shows. The right window and `hud_sv_bigger_scale` are console-only until F-069.

---

## 15. Options deck (F-069, D-016)

**Open:** F2 in a single-player level, or Options on the main menu, in Tartarus mode. Classic Mode keeps the stock Options menu everywhere; so does multiplayer. The game pauses while it is open (`con_pause`).

**Keys:** Up/Down select, Left/Right change, Enter or Space activate, Tab / Shift+Tab or Page Up/Down change page, Escape or F2 close. **Mouse:** click a page key; click a row to select it, click it again to step (left half down, right half up).

**Pages:** Graphics, Controls, Audio, Gameplay, Tartarus. A curated table in `deck_core.cpp` gives order, labels and choice names; any other saved cvar with a page's prefix (`r_`, `m_`, `s_`, `g_`, `ui_`) appears on that page automatically. Cheat and unsaved cvars do not. Changes apply at once and are saved; rows marked as needing it (`r_resolution`, `r_fullscreen`, `r_renderscale`, `r_msaa`, `r_depthbits`) rebuild the frame when the deck closes, so stepping through a list does not change the screen each time. From the main menu, resolution and fullscreen apply on close and the rest with the next level. On the console, these need `r_restart`.

| Cvar | Default | Notes |
|---|---|---|
| `ui_scanlines` | 28 | 0–60, scanline strength on the deck screen |
| `ui_phosphor` | 0 | 0 amber, 1 green, 2 ice, 3 plain (no scanlines) |
| `ui_casing` | 0 | 0 gunmetal, 1 putty |
| `hud_sv_mirror` | 0 | rear-view mirror preset, §14: 0 stock, 1 top left, 2 top right, 3 custom; not saved itself, it writes `hud_sv_left_*` |
| `s_sfx_volume`, `s_music_volume` | stock | mirror the stock volumes, which the stock settings save |
| `r_resolution` | stock | index into the display's mode list (the deck lists modes 640 wide and up); saved as `RS_width`/`RS_height` |
| `r_fullscreen` | stock | fullscreen (borderless) or windowed; saved as `RS_fullscreen` |

**Other screens:** in Tartarus mode the main menu, Save, Load, New Game, level warp, Pilots, the Keys help (F1), the Guide-Bot menu (F4) and all Yes/No, OK and text-entry boxes use the same casing as a list (message boxes keep stock in multiplayer): Up/Down or the pointer select, Enter, Space or a click chooses, Escape goes back (not on the main menu), and main menu rows keep their stock hotkeys. Save asks for a name, and for confirmation before overwriting. Yes/No boxes start on No. Load shows the save's screenshot. Choosing a pilot opens that pilot's page: fly as this pilot, difficulty, profanity filter, audio taunts, ship and logo, keyboard, controller, the stock pilot screen, delete; the page saves each change at once. Keyboard and Joystick and mouse list every function with its two bindings; Enter opens a page to rebind, clear or invert each one (Escape cancels a capture), and the top rows are Reset to Defaults (asks first), Adjust Settings (stock), Restore Settings and the other pages. Weapon order lists primaries and secondaries, highest priority first; Enter moves a weapon up or down or turns its auto-select off. Deck screens switch the menu frame to the chosen resolution while open and back to 640x480 for stock screens; the pointer keeps the stock speed at any resolution.

Layout is a 1160x680 area scaled to the frame, so the deck keeps its size on screen; text is the game's large font scaled to a target height. Scanlines are drawn under the text because `rend_DrawLine` has no alpha.

