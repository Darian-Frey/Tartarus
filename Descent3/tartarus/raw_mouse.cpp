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

// Raw mouse input (F-062). See raw_mouse.h.

#include "raw_mouse.h"

#include <SDL3/SDL_hints.h>

#include "classic.h"
#include "cvar.h"
#include "game.h"
#include "mouse_flight.h"

// Defined in ddio/sdlcontroller.cpp (TARTARUS hook): sample the mouse every
// frame instead of every MOUSE_POLLING_TIME.
extern bool Tartarus_mouse_every_frame;

namespace tartarus {

namespace {

Cvar *m_raw = nullptr;
Cvar *m_sensitivity = nullptr;
Cvar *m_accel = nullptr;
float g_rem_x = 0.0f, g_rem_y = 0.0f;
int g_applied_accel = -1;

bool Active() { return !IsClassic() && !(Game_mode & GM_MULTI); }

void ApplyAccel() {
  const int want = (Active() && m_accel != nullptr && m_accel->GetBool()) ? 1 : 0;
  if (want == g_applied_accel)
    return;
  SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, want ? "1" : "0");
  g_applied_accel = want;
}

} // namespace

void RawMouse_Init() {
  const uint32_t locked = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  CvarRegistry &cv = Cvars();
  // Off by default: the stock mouse paths turn ~3-4x less per count when read every frame (upstream
  // reuses each 50 ms reading on the frames between), and the matching m_sensitivity has not been
  // tuned yet. The virtual joystick is read every frame regardless (it consumes the counts itself).
  m_raw = cv.RegisterBool("m_raw", false,
                          "Read the mouse every frame for the stock mouse modes (0 = upstream: 20 times a second; "
                          "try m_sensitivity 3 to 4 with it)",
                          locked);
  m_sensitivity = cv.RegisterFloat("m_sensitivity", 1.0f, 0.05f, 10.0f,
                                   "Mouse counts multiplier for the stock mouse modes (virtual joystick: m_vj_sensitivity)",
                                   locked);
  m_accel = cv.RegisterBool("m_accel", false, "Use the desktop's mouse acceleration curve (0 = raw, as upstream)", locked);
  m_accel->AddChangeCallback([](const Cvar &) { ApplyAccel(); });
  RawMouse_Frame();
}

void RawMouse_Frame() {
  // The virtual joystick takes every count itself, so reading every frame only makes it respond sooner.
  Tartarus_mouse_every_frame = Active() && ((m_raw != nullptr && m_raw->GetBool()) || MouseFlight_Active());
  ApplyAccel();
}

void RawMouse_ScaleCounts(int *dx, int *dy) {
  if (!Active() || m_sensitivity == nullptr)
    return;
  const float s = m_sensitivity->GetFloat();
  if (s == 1.0f)
    return;
  *dx = ScaleCounts(*dx, s, &g_rem_x);
  *dy = ScaleCounts(*dy, s, &g_rem_y);
}

} // namespace tartarus
