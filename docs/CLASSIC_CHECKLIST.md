# Classic Mode checklist (AV-001)

Classic Mode (`classic 1`, the default on first run, or `-classic`) must play exactly like stock Descent 3 (D-004). Two tools keep that promise:

1. **The hook table below**, checked by `tools/classic_audit.py` (also in CI). Every upstream file carrying a `TARTARUS:` marker, and every `tartarus::` function or `Tartarus_*` global used from upstream code, must appear here with its Classic behaviour. A new hook therefore fails the audit until someone records what it does in Classic Mode.
2. **The manual run** at the end, before each release and after any change to a hook.

Behaviour column: **gated** = does nothing (or exactly stock) in Classic Mode; **active (compat)** = a compatibility or bug fix that changes no gameplay and stays on by design (D-004, D-013); **tooling** = build, test or command-line-only.

## Hook table

| Upstream file | Tartarus entry points used there | Classic behaviour |
|---|---|---|
| `Descent3/CMakeLists.txt` | `include(tartarus/tartarus.cmake)` | tooling |
| `Descent3/init.cpp` | `tartarus::DataDir_Probe`, `tartarus::DataDir_RequireHog` | active (compat): finds the Steam install (F-003) |
| `Descent3/init.cpp` | `tartarus::Tartarus_Init` | active: loads `tartarus.cfg`, sets up cvars, console and modules; no gameplay change by itself |
| `Descent3/init.cpp` | `tartarus::Display_SaveSettings` | active (compat): BUG-004/005 |
| `Descent3/init.cpp` | `tartarus::kFovAbsMin`, `tartarus::kFovAbsMax` | gated: the load-time clamp is widened, then `Display_Init()` narrows FOV to upstream's 72–90 in Classic Mode |
| `Descent3/mmItem.cpp` | `tartarus::Deck_DrawMenuArt` | gated: returns false in Classic Mode, stock art drawing (D-016) |
| `Descent3/mmItem.cpp` | `tartarus::DrawMenuBuildString` | active (cosmetic): main-menu build string and upstream credit (F-004) |
| `Descent3/game.cpp` | `tartarus::Display_MenuSize` | gated: menus stay 640x480 in Classic Mode (D-016) |
| `Descent3/game.cpp` | `tartarus::Display_ScaleRenderSize`, `tartarus::Display_AdjustGameWindow` | active (display, F-068): no-ops at `r_renderscale 1` (default); otherwise the game frame renders at that multiple and full-window views stay full |
| `Descent3/menu.cpp` | `tartarus::Deck_RunFromMenu`, `tartarus::MainDeck_Run`, `tartarus::kMainDeckRows` | gated: both return at once in Classic Mode, so the stock menus run (D-016) |
| `Descent3/gamesave.cpp` | `tartarus::SaveDeck_Run`, `tartarus::SaveDeck_PickLoad`, `tartarus::SaveDeck_SlotPath`, `tartarus::kSaveDeckStock` | gated: both report Classic Mode at once, so the stock Save and Load dialogs run (D-016) |
| `Descent3/newui.cpp` | `tartarus::Deck_MessageBox`, `tartarus::Deck_EditBox`, `tartarus::kDeckStock` | gated: report stock in Classic Mode and multiplayer (D-016) |
| `Descent3/ctlconfig.cpp` | `tartarus::ControlsDeck_Run` | gated: false in Classic Mode and multiplayer, stock controls screen (D-016) |
| `Descent3/gamesequence.cpp` | `tartarus::Deck_RunPendingControls` | gated: false unless the deck asked, and the deck never opens in Classic Mode, so `OptionsMenu()` runs (D-016) |
| `Descent3/hud.cpp` | `tartarus::OrientHud_RenderCanopy` | gated: returns at once in Classic Mode, so nothing is drawn on the canopy (F-046) |
| `Descent3/hud.cpp` | `tartarus::OrientHud_RenderReticle` | gated: false in Classic Mode, so the stock reticle draws (F-047) |
| `Descent3/help.cpp` | `tartarus::HelpDeck_Run` | gated: false in Classic Mode, stock Keys screen (D-016) |
| `Descent3/buddymenu.cpp` | `tartarus::BuddyDeck_Pick`, `tartarus::kDeckStock` | gated: stock Guide-Bot menu in Classic Mode and multiplayer (D-016) |
| `Descent3/pilot.cpp` | `tartarus::PilotDeck_Run` | gated: false in Classic Mode, stock Pilots screen (D-016) |
| `Descent3/menu.cpp` | `tartarus::MissionDeck_Pick`, `tartarus::LevelDeck_Pick`, `tartarus::kDeckStock` | gated: stock New Game and level warp in Classic Mode (D-016) |
| `Descent3/config.cpp` | `tartarus::Display_RestoreResolution` | active (compat): BUG-004 |
| `Descent3/config.cpp` | `tartarus::Display_FovMin`, `tartarus::Display_FovMax`, `tartarus::Display_SetFov` | gated: Video FOV slider is 72–90 as upstream |
| `Descent3/config.cpp` | `tartarus::TartarusOptionsSheet` | gated: the Tartarus tab only shows text explaining Classic Mode |
| `Descent3/GameLoop.cpp` | `tartarus::HandleKey` | console: active (non-cheat commands only); cheat menu (Shift+F12, a key stock leaves unused): refuses with a message |
| `Descent3/GameLoop.cpp` | `tartarus::Frame` | gated: cheat hold loop switches everything off and returns; mouse-flight and raw-mouse frames reset and leave the stock mouse alone |
| `Descent3/GameLoop.cpp` | `tartarus::TimeScale` | gated: returns 1 |
| `Descent3/GameLoop.cpp` | `tartarus::PausedFrame` | active: only does anything while the console holds the pause (`con_pause`, a dev-tool convenience that changes no gameplay); the cheat menu cannot open in Classic Mode |
| `Descent3/GameLoop.cpp` | `tartarus::RenderOverlays` | gated: no stick indicator, no CHEATS tag; the console draws when opened |
| `Descent3/Controls.cpp` | `tartarus::MouseFlight_Apply`, `tartarus::MouseFlight_ReservesButton` | gated: return at once; flare on the middle button works as stock |
| `Descent3/TelComAutoMap.cpp` | `tartarus::MapScan_Reset`, `tartarus::MapScan_MapOpened`, `tartarus::MapScan_DrawFace`, `tartarus::MapScan_FinishRooms`, `tartarus::MapScan_Background` | gated: the first two only touch a side table, `DrawFace` returns false and `Background` returns the stock colour in Classic Mode so the map draws as stock, and `FinishRooms` then has nothing to draw (F-050) |
| `Descent3/TelCom.cpp`, `Descent3/TelComGoals.cpp` | `tartarus::kUiMouseScale` | active (compat): BUG-006 TelCom pointer speed |
| `Descent3/Controls.cpp` | `tartarus::Level_Apply` | gated: returns at once in Classic Mode, so no bank input is added (F-043) |
| `Descent3/SmallViews.cpp` | `tartarus::SmallViews_Adjust` | gated: upstream rectangle untouched |
| `Descent3/gamesequence.cpp` | `tartarus::SkipBriefing` | tooling: only with `-skipbriefing` |
| `Descent3/gamecinematics.cpp` | `tartarus::SkipCinematics` | tooling: only with `-skipcinematics` |
| `ddio/sdlcontroller.cpp` | `Tartarus_mouse_filter`, `Tartarus_mouse_every_frame` | gated: the filter passes counts through untouched and every-frame sampling is off, so upstream's 20 Hz mouse is unchanged |
| `ddio/lnxmouse.cpp` | (inline: SDL middle/right swap) | active (compat): BUG-007, right button = secondary fire as on Windows |
| `renderer/HardwareOpenGL.cpp`, `renderer/dyna_gl.h` (`glFramebufferTexture2D`, `glRenderbufferStorageMultisample`) | `Tartarus_bar_fill_mode`, `Tartarus_bar_fill_brightness` | active (display): blurred bars around 4:3 menus (F-060); `r_bar_fill 0` restores black bars |
| `renderer/HardwareOpenGL.cpp` | `Tartarus_before_flip`, `Tartarus_swap_interval`, `Tartarus_trilinear`, `Tartarus_anisotropy`, `Tartarus_filter_dirty`, `Tartarus_depth_bits`, `Tartarus_msaa` (and internal `Tartarus_applied_swap`, `Tartarus_max_aniso`, `Tartarus_fbo_samples`, `Tartarus_resolve_fbo`, `Tartarus_resolve_rbo`, `Tartarus_frame_src`) | active (display, F-068): VSync follows the stock checkbox (fixes BUG-010); frame cap (`r_maxfps`, 60 as upstream); trilinear, anisotropic and 24-bit depth defaults are visual only, each settable back to stock (`r_trilinear 0`, `r_anisotropy 1`, `r_depthbits 16`); MSAA off by default (`r_msaa`) |
| `lib/renderer.h`, `renderer/HardwareOpenGL.cpp` | (inline: `rend_DrawTriangles2D`, `rend_DrawLines3D`) | active but unused: only the orientation aids and the scanner map call them, and they are off in Classic Mode (F-046, F-050) |
| `renderer/ShaderProgram.h` | (inline: vertex stream written with `glBufferSubData`) | active (compat): BUG-012, no visual change except removing the corruption |
| `.github/workflows/build.yml` | (matrix entries commented out) | tooling: D-014, D-015 |
| `.github/workflows/cd.yml` | (push trigger replaced by manual and weekly runs) | tooling: D-015 |

Console key bindings (`bind`) work in Classic Mode like the console itself; they only run what the user bound, and cheat lines are refused there.

Cvars: every gameplay- or feel-changing cvar is `CLASSIC_LOCKED` and reads as its default in Classic Mode; the modules also check `IsClassic()` directly, so a locked cvar whose default is a Tartarus behaviour (for example `m_mode 1`) still has no effect.

## Manual run

Launch with `-classic` (scripted runs: silent profile, `-skipbriefing -skipcinematics`), then:

- [ ] Main menu: build string bottom right; menus otherwise stock (blurred side bars are expected, `r_bar_fill`).
- [ ] Main menu, New Game, Pilots, key bindings, Save (F9), Load (Alt+F3), Keys (F1), Guide-Bot (F4) and the Quit and Abort boxes are the stock screens, not the deck (D-016).
- [ ] Level 1: stock reticle, even with `hud_reticle` set in `tartarus.cfg`; no virtual-joystick ring; the mouse turns the ship as stock (movement = rotation).
- [ ] No CHEATS tag; Shift+F12 prints "Cheats are not available in Classic Mode"; `god`, `give all`, `timescale 0.5` refused in the console.
- [ ] Console (backtick) opens; `version`, `where`, `help` work.
- [ ] F2 and main menu Options open the stock Options menu, not the options deck (D-016). Video: FOV slider runs 72–90. Tartarus tab: only the Classic Mode explanation.
- [ ] F3 HUD view, Shift+F1: the small view sits in the stock bottom-left square.
- [ ] Middle mouse button fires a flare; right button fires a secondary (BUG-007).
- [ ] Round trip, in a separate launch **without** `-classic` (that flag makes `classic 0` do nothing): `classic 1`, `classic 0`, `classic 1` in a level; afterwards no ring, stock mouse, F2 opens the stock Options menu, cheats refused.

## Runs

| Date | Build | Result |
|---|---|---|
| 2026-11-04 | `65dd4ab1` + Key bindings row | Pass (scripted): no ring, stock mouse turned the ship (heading 359.7 to 26.3), no CHEATS tag, Shift+F12 and `god`, `give all`, `timescale 0.5` refused, console works, stock Options with FOV 72 and the Classic text on the Tartarus tab, stock small view, middle button flare and right button secondary (count 7 to 6), and stock main menu, New Game, Pilots, Save, Load, Keys, Guide-Bot, Quit, Abort and key bindings. Round trip tested from Tartarus mode. Not observed: the small view after Shift+F1 in the HUD view (the shot was in the cockpit view). The 2026-11-02 round trip may have run under `-classic` and so proved nothing; it is superseded by this one. |
| 2026-11-02 | `648ace30` + F-062 | Pass (scripted, `-classic`): no stick ring, stock mouse turned the ship, no CHEATS tag, Shift+F12 refused, console works, FOV 72, Tartarus tab shows only the Classic text, stock small view, `classic 0`/`1` round trip clean. Fixed on the way: cheat commands in Classic said "unlock it first" instead of the Classic refusal; the tab's Classic text overflowed. Not covered by the script: middle/right buttons (checked by hand for BUG-007 the same day). |
