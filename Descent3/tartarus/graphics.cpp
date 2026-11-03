/*
 * Tartarus, a fork of Descent 3
 * Copyright (C) 2026 Tartarus contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// Graphics options (F-068). See graphics.h.

#include "graphics.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "args.h"
#include "console.h"
#include "game.h"
#include "cvar.h"
#include "log.h"
#include "renderer.h"

extern renderer_preferred_state Render_preferred_state; // config.cpp / init.cpp: the stock video settings
extern int Min_allowed_frametime;                       // GameLoop.cpp: frame cap in ms (0 = none)

// Defined in renderer/HardwareOpenGL.cpp (TARTARUS hooks).
extern void (*Tartarus_before_flip)();
extern int Tartarus_swap_interval;
extern bool Tartarus_trilinear;
extern int Tartarus_anisotropy;
extern bool Tartarus_filter_dirty;
extern int Tartarus_depth_bits;
extern int Tartarus_msaa;

namespace tartarus {

namespace {

Cvar *r_vsync = nullptr;
Cvar *r_vsync_adaptive = nullptr;
Cvar *r_maxfps = nullptr;
Cvar *r_trilinear = nullptr;
Cvar *r_anisotropy = nullptr;
Cvar *r_depthbits = nullptr;
Cvar *r_msaa = nullptr;
Cvar *r_gamma = nullptr;
bool g_restart_pending = false;
bool g_syncing = false;
bool g_framecap_arg = false;

void ApplyFrameCap() {
  if (g_framecap_arg)
    return; // -framecap wins for this session
  // The game's frame timer counts whole milliseconds: a frame under 1 ms would measure as zero time,
  // and some code divides by Frametime. So "uncapped" still keeps frames >= 1 ms (<= 1000 fps), and
  // caps are whole milliseconds (144 -> 6 ms, about 166 fps; use VSync to lock to the refresh rate).
  const int fps = r_maxfps->GetInt();
  Min_allowed_frametime = fps > 0 ? std::max(1, static_cast<int>(1000.0 / fps)) : 1;
}

// The stock Video checkbox owns RS_vsync; r_vsync mirrors it both ways.
void SyncVsyncCvar() {
  const int want = Render_preferred_state.vsync_on ? 1 : 0;
  if (r_vsync->GetInt() == want)
    return;
  g_syncing = true;
  Cvars().Set("r_vsync", std::to_string(want), CvarSource::Code);
  g_syncing = false;
}

// The stock brightness dialog owns RS_gamma; r_gamma mirrors it both ways.
void SyncGammaCvar() {
  if (std::abs(r_gamma->GetFloat() - Render_preferred_state.gamma) < 0.001f)
    return;
  g_syncing = true;
  Cvars().Set("r_gamma", std::to_string(Render_preferred_state.gamma), CvarSource::Code);
  g_syncing = false;
}

void BeforeFlip() {
  SyncVsyncCvar();
  SyncGammaCvar();
  Tartarus_swap_interval = Render_preferred_state.vsync_on ? (r_vsync_adaptive->GetBool() ? -1 : 1) : 0;
  Tartarus_trilinear = r_trilinear->GetBool();
  Tartarus_anisotropy = r_anisotropy->GetInt();
  Tartarus_depth_bits = r_depthbits->GetInt() >= 24 ? 24 : 16;
  Tartarus_msaa = r_msaa->GetInt();
}

void MarkFiltersDirty(const Cvar &) { Tartarus_filter_dirty = true; }

} // namespace

void Graphics_Init() {
  CvarRegistry &cv = Cvars();
  r_vsync = cv.RegisterBool("r_vsync", Render_preferred_state.vsync_on != 0,
                            "Vertical sync (the Video tab's checkbox; saved with the video settings)");
  r_vsync_adaptive = cv.RegisterBool("r_vsync_adaptive", false,
                                     "Adaptive VSync: tear instead of stutter when the frame rate drops below the "
                                     "refresh rate (needs r_vsync 1 and driver support)",
                                     CVAR_ARCHIVE);
  r_maxfps = cv.RegisterInt("r_maxfps", 60, 0, 1000,
                            "Frame-rate cap (0 = up to 1000; upstream 60; whole-millisecond steps). Very high rates can change game feel until "
                            "fixed-timestep physics (F-061). -framecap overrides it for one session",
                            CVAR_ARCHIVE);
  r_trilinear = cv.RegisterBool("r_trilinear", true, "Trilinear texture filtering (blends mipmap levels)", CVAR_ARCHIVE);
  r_anisotropy = cv.RegisterInt("r_anisotropy", 4, 1, 16,
                                "Anisotropic texture filtering, 1 (off) to 16; sharper floors and walls at angles",
                                CVAR_ARCHIVE);
  r_depthbits = cv.RegisterInt("r_depthbits", 24, 16, 24,
                               "Depth-buffer bits, 16 (upstream) or 24 (less flicker on distant surfaces); applies "
                               "after a resolution change or restart",
                               CVAR_ARCHIVE);
  r_msaa = cv.RegisterInt("r_msaa", 0, 0, 16,
                          "Anti-aliasing samples: 0 off, 2, 4 or 8 (clamped to what the driver supports); applies "
                          "after a resolution change or restart",
                          CVAR_ARCHIVE);

  r_gamma = cv.RegisterFloat("r_gamma", Render_preferred_state.gamma, 1.0f, 3.0f,
                             "Brightness (gamma), as the Video tab's Adjust Brightness; saved with the video settings");
  r_gamma->AddChangeCallback([](const Cvar &c) {
    if (g_syncing)
      return;
    Render_preferred_state.gamma = c.GetFloat();
    rend_SetGammaValue(c.GetFloat());
  });

  Console().AddCommand({"r_restart", "r_restart",
                        "Rebuild the game frame so r_renderscale, r_msaa and r_depthbits take effect now", CMD_NONE,
                        [](const CommandArgs &, ConsoleCore &c) {
                          if (GetScreenMode() != SM_GAME) {
                            c.Print("r_restart: only in a level (menus use a fixed 640x480 frame)");
                            return;
                          }
                          g_restart_pending = true; // done at the start of the next frame, outside key handling
                          c.Print("r_restart: rebuilding the frame");
                        }});

  r_vsync->AddChangeCallback([](const Cvar &c) {
    if (!g_syncing)
      Render_preferred_state.vsync_on = c.GetBool() ? 1 : 0;
  });
  r_maxfps->AddChangeCallback([](const Cvar &) { ApplyFrameCap(); });
  r_trilinear->AddChangeCallback(MarkFiltersDirty);
  r_anisotropy->AddChangeCallback(MarkFiltersDirty);

  g_framecap_arg = FindArg("-framecap") != 0;
  if (g_framecap_arg)
    LOG_INFO << "[tartarus] -framecap given: r_maxfps ignored for this session";
  ApplyFrameCap();
  BeforeFlip();
  Tartarus_before_flip = &BeforeFlip;
}

void Graphics_RequestRestart() {
  if (GetScreenMode() == SM_GAME)
    g_restart_pending = true;
}

void Graphics_Frame() {
  if (!g_restart_pending)
    return;
  g_restart_pending = false;
  if (GetScreenMode() == SM_GAME) {
    // As the Video tab does after a change: new screen mode, then a forced renderer re-init, so the
    // offscreen frame is rebuilt (MSAA, depth bits) even when its size stays the same.
    SetScreenMode(SM_GAME, true);
    rend_SetPreferredState(&Render_preferred_state, true);
    LOG_INFO << "[tartarus] r_restart: frame rebuilt";
  }
}

} // namespace tartarus
