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

// Mouse flight modes (F-067). See mouse_flight.h.

#include "mouse_flight.h"

#include <algorithm>
#include <cmath>

#include "cheats.h"
#include "classic.h"
#include "console.h"
#include "controls.h"
#include "cvar.h"
#include "descent.h"
#include "game.h"
#include "gameloop.h"
#include "gamesequence.h"
#include "grdefs.h"
#include "mouse_flight_core.h"
#include "object.h"
#include "player.h"
#include "renderer.h"
#include "ddio.h"

extern bool Menu_interface_mode; // GameLoop.cpp

// Defined in ddio/sdlcontroller.cpp (TARTARUS hook): sees every batch of mouse
// counts before the controller uses them, and may zero them.
extern void (*Tartarus_mouse_filter)(int *dx, int *dy, unsigned btnmask);

namespace tartarus {

namespace {

VirtualJoystick g_vj;

Cvar *m_mode = nullptr;
Cvar *m_vj_deadzone = nullptr;
Cvar *m_vj_radius = nullptr;
Cvar *m_vj_curve = nullptr;
Cvar *m_vj_centre_rate = nullptr;
Cvar *m_vj_sensitivity = nullptr;
Cvar *m_vj_roll_x = nullptr;
Cvar *m_vj_invert_y = nullptr;
Cvar *m_vj_indicator = nullptr;
Cvar *m_vj_recentre_button = nullptr;

enum MouseMode { kModeClassic = 0, kModeVirtualJoystick = 1, kModeVirtualJoystickCentring = 2 };

int g_last_player_handle = -1;
unsigned g_last_buttons = 0;
float g_last_apply_time = -1.0f;

bool InLevel() {
  return GetFunctionMode() == GAME_MODE && Player_object != nullptr && Player_num >= 0 &&
         Players[Player_num].objnum >= 0;
}

/// Places where the stick must be centred and the mouse left to upstream
/// (AV-013): menus, TelCom/automap, console, cheat menu, focus loss, pause,
/// death.
bool StickContext() {
  if (!InLevel() || Game_paused || Menu_interface_mode || Game_interface_mode != GAME_INTERFACE)
    return false;
  if (!Control_poll_flag || Console_IsOpen() || Cheats_MenuIsOpen())
    return false;
  if (Descent != nullptr && !Descent->active())
    return false;
  if (Players[Player_num].flags & (PLAYER_FLAGS_DYING | PLAYER_FLAGS_DEAD))
    return false;
  return true;
}

float RadiusCounts() {
  const int h = Game_window_h > 0 ? Game_window_h : 480;
  return m_vj_radius->GetFloat() * static_cast<float>(h);
}

void Filter(int *dx, int *dy, unsigned btnmask) {
  if (!MouseFlight_Active())
    return; // upstream mouse, untouched
  const unsigned pressed = btnmask & ~g_last_buttons;
  g_last_buttons = btnmask;
  if (!StickContext()) {
    g_vj.Reset();
    return; // menus and the automap still get their mouse movement
  }
  const int button = m_vj_recentre_button->GetInt();
  if (button > 0 && (pressed & (1u << (button - 1))))
    g_vj.Reset();
  else
    g_vj.AddDelta(static_cast<float>(*dx), static_cast<float>(*dy), RadiusCounts(), m_vj_sensitivity->GetFloat());
  *dx = 0; // upstream must not also turn the ship
  *dy = 0;
}

} // namespace

void MouseFlight_Init() {
  CvarRegistry &cv = Cvars();
  const uint32_t locked = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  m_mode = cv.RegisterInt("m_mode", kModeVirtualJoystick, 0, 2,
                          "Mouse flight: 0 classic (movement = rotation), 1 virtual joystick, 2 virtual joystick "
                          "with auto-centring",
                          locked);
  m_vj_deadzone = cv.RegisterFloat("m_vj_deadzone", 0.04f, 0.0f, 0.5f,
                                   "Virtual joystick: dead zone as a fraction of the full-rate radius", locked);
  m_vj_radius = cv.RegisterFloat("m_vj_radius", 0.20f, 0.02f, 1.0f,
                                 "Virtual joystick: mouse travel for full turn rate, as a fraction of screen height",
                                 locked);
  m_vj_curve = cv.RegisterFloat("m_vj_curve", 1.8f, 0.5f, 4.0f,
                                "Virtual joystick: response exponent (1 = linear, higher = finer near centre)", locked);
  m_vj_centre_rate = cv.RegisterFloat("m_vj_centre_rate", 1.5f, 0.0f, 20.0f,
                                      "Virtual joystick: auto-centring speed per second (m_mode 2 only)", locked);
  m_vj_sensitivity =
      cv.RegisterFloat("m_vj_sensitivity", 1.0f, 0.05f, 10.0f, "Virtual joystick: mouse counts scale", locked);
  m_vj_roll_x = cv.RegisterBool("m_vj_roll_x", false, "Virtual joystick: mouse X rolls instead of turning", locked);
  m_vj_invert_y = cv.RegisterBool("m_vj_invert_y", false,
                                  "Virtual joystick: invert pitch (0 = push forward to dive, as a joystick)", locked);
  m_vj_indicator =
      cv.RegisterBool("m_vj_indicator", true, "Virtual joystick: draw the offset indicator and dead-zone ring", locked);
  m_vj_recentre_button = cv.RegisterInt("m_vj_recentre_button", 3, 0, 8,
                                        "Virtual joystick: mouse button that centres the stick (0 = none, 3 = middle)",
                                        locked);

  m_mode->AddChangeCallback([](const Cvar &) { g_vj.Reset(); });

  Console().AddCommand({"m_recentre", "m_recentre", "Centre the virtual joystick", CMD_NONE,
                        [](const CommandArgs &, ConsoleCore &) { MouseFlight_Recentre(); }});

  Tartarus_mouse_filter = &Filter;
}

bool MouseFlight_Active() {
  if (m_mode == nullptr || IsClassic() || (Game_mode & GM_MULTI)) // D-008: single-player only for now
    return false;
  return m_mode->GetInt() != kModeClassic;
}

void MouseFlight_Recentre() { g_vj.Reset(); }

bool MouseFlight_ReservesButton(int ctf_function) {
  if (!MouseFlight_Active() || Controller == nullptr)
    return false;
  const int button = m_vj_recentre_button->GetInt();
  if (button <= 0)
    return false;
  ct_type type[2];
  ct_config_data value;
  uint8_t flags[2];
  Controller->get_controller_function(ctf_function, type, &value, flags);
  const uint16_t bound = CONTROLLER_VALUE(value);
  return (type[0] == ctMouseButton && CONTROLLER_CTL1_VALUE(bound) == button) ||
         (type[1] == ctMouseButton && CONTROLLER_CTL2_VALUE(bound) == button);
}

void MouseFlight_Frame() {
  if (!MouseFlight_Active()) {
    g_vj.Reset();
    return;
  }
  // Level load or respawn: a new player object (AV-013).
  const int handle = InLevel() ? Objects[Players[Player_num].objnum].handle : -1;
  if (handle != g_last_player_handle) {
    g_last_player_handle = handle;
    g_vj.Reset();
  }
  if (!StickContext())
    g_vj.Reset();
}

void MouseFlight_Apply(game_controls *controls) {
  if (!MouseFlight_Active())
    return;

  // Anything that stopped the controls for a moment (pause, a dialog, a
  // level transition) comes back to a centred stick (AV-013).
  const float now = timer_GetTime();
  const bool stale = g_last_apply_time < 0.0f || now - g_last_apply_time > 0.25f;
  g_last_apply_time = now;
  if (stale || !StickContext()) {
    g_vj.Reset();
    return;
  }

  if (m_mode->GetInt() == kModeVirtualJoystickCentring)
    g_vj.Decay(m_vj_centre_rate->GetFloat(), Frametime);

  const float dz = m_vj_deadzone->GetFloat();
  const float curve = m_vj_curve->GetFloat();
  const float sx = VjCurve(g_vj.X(), dz, curve);
  float sy = VjCurve(g_vj.Y(), dz, curve);
  if (m_vj_invert_y->GetBool())
    sy = -sy;

  // Same mapping as a joystick axis in DoControllerMovement(): Y forward
  // (negative) pitches down, X right turns right.
  float pitch = -sy;
  float heading = 0.0f;
  float bank = 0.0f;
  if (controls->toggle_slide) {
    controls->sideways_thrust += sx;
    controls->vertical_thrust += -sy;
    return;
  }
  if (controls->toggle_bank || m_vj_roll_x->GetBool())
    bank = -sx;
  else
    heading = sx;
  if (controls->toggle_bank)
    pitch = 0.0f; // as upstream: bank toggle takes over the turn axes

  // Level scripts can forbid directions (as upstream's mouselook honours).
  const int allow = Players[Player_num].controller_bitflags;
  if ((pitch < 0 && !(allow & PCBF_PITCHUP)) || (pitch > 0 && !(allow & PCBF_PITCHDOWN)))
    pitch = 0.0f;
  if ((heading < 0 && !(allow & PCBF_HEADINGLEFT)) || (heading > 0 && !(allow & PCBF_HEADINGRIGHT)))
    heading = 0.0f;
  if ((bank > 0 && !(allow & PCBF_BANKLEFT)) || (bank < 0 && !(allow & PCBF_BANKRIGHT)))
    bank = 0.0f;

  controls->pitch_thrust += pitch;
  controls->heading_thrust += heading;
  controls->bank_thrust += bank;
}

void MouseFlight_Render() {
  if (!MouseFlight_Active() || !m_vj_indicator->GetBool() || !StickContext())
    return;

  const int cx = Game_window_x + Game_window_w / 2;
  const int cy = Game_window_y + Game_window_h / 2;
  // Drawn at a fixed size so it stays readable whatever m_vj_radius is.
  const float r = std::max(24.0f, Game_window_h * 0.12f);
  const float dz_r = std::max(2.0f, r * m_vj_deadzone->GetFloat());
  const int dot_x = cx + static_cast<int>(std::lround(g_vj.X() * r));
  const int dot_y = cy + static_cast<int>(std::lround(g_vj.Y() * r));

  StartFrame(0, 0, Max_window_w, Max_window_h, false);
  rend_SetAlphaType(AT_CONSTANT);
  rend_SetAlphaValue(110);

  // Outer ring (full rate) and dead-zone ring as line segments: the GL
  // renderer's rend_DrawCircle is an empty stub.
  auto ring = [&](float radius, ddgr_color colour, int segments) {
    rend_SetFlatColor(colour);
    for (int i = 0; i < segments; ++i) {
      const float a0 = 6.2831853f * i / segments;
      const float a1 = 6.2831853f * (i + 1) / segments;
      rend_DrawLine(cx + static_cast<int>(radius * std::cos(a0)), cy + static_cast<int>(radius * std::sin(a0)),
                    cx + static_cast<int>(radius * std::cos(a1)), cy + static_cast<int>(radius * std::sin(a1)));
    }
  };
  ring(r, GR_RGB(60, 160, 90), 48);
  ring(dz_r, GR_RGB(120, 220, 140), 16);

  if (!g_vj.IsCentred()) {
    rend_SetAlphaValue(170);
    rend_SetFlatColor(GR_RGB(120, 255, 150));
    rend_DrawLine(cx, cy, dot_x, dot_y);
    rend_FillRect(GR_RGB(150, 255, 170), dot_x - 2, dot_y - 2, dot_x + 3, dot_y + 3);
  }
  rend_SetAlphaType(AT_ALWAYS);
  EndFrame();
}

} // namespace tartarus
