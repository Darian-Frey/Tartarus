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

// Hold and snap to upright. See level_assist.h.

#include "level_assist.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include "binds.h"
#include "classic.h"
#include "controls.h"
#include "cvar.h"
#include "ddio.h"
#include "game.h"
#include "mouse_flight.h"
#include "object.h"
#include "orient_core.h"

namespace tartarus {

namespace {

Cvar *al_strength = nullptr;
Cvar *al_delay = nullptr;
Cvar *al_hold_rate = nullptr;
Cvar *al_snap_time = nullptr;
Cvar *al_hold_key = nullptr;
Cvar *al_snap_key = nullptr;

constexpr float kDeadZoneDeg = 0.5f; // close enough: no hunting around upright (AV-011)
constexpr float kSnapDoneDeg = 1.0f;
constexpr float kNoRollAxis = 0.15f; // nose near vertical: roll has no meaning
// The controls run about 20 times a second; a quicker correction overshoots (AV-011).
constexpr float kSlowestLoop = 0.2f;
constexpr float kSnapPatience = 0.15f;

bool g_snapping = false;
bool g_holding = false;
bool g_slide = false, g_bank = false;
float g_snap_target = 0.0f;
float g_snap_wait = 0.0f; // above 0: a tap waiting for the roll input to end
float g_since_roll = 0.0f;

bool KeyDown(const Cvar *name) {
  const std::optional<int> key = Binds_ParseKey(name->GetString());
  return key.has_value() && (KEY_STATE(*key & 0xFF) != 0);
}

} // namespace

void Level_Init() {
  const uint32_t flags = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  CvarRegistry &cv = Cvars();
  al_strength = cv.RegisterFloat("al_strength", 0.0f, 0.0f, 1.0f, "Extra auto-level on top of stock; 0 leaves stock alone", flags);
  al_delay = cv.RegisterFloat("al_delay", 0.4f, 0.0f, 3.0f, "Seconds after the last roll input before extra auto-level starts", flags);
  al_hold_rate = cv.RegisterFloat("al_hold_rate", 180.0f, 30.0f, 360.0f, "Auto-level hold: degrees a second toward upright", flags);
  al_snap_time = cv.RegisterFloat("al_snap_time", 0.35f, 0.1f, 2.0f, "Snap to up: seconds to reach the nearest upright", flags);
  al_hold_key = cv.RegisterString("al_hold_key", "x", "Auto-level hold: rolls upright while held", flags);
  al_snap_key = cv.RegisterString("al_snap_key", "z", "Snap to up: a tap rolls to the nearest upright", flags);
}

bool Level_HandleKey(int key) {
  if (IsClassic() || !Flight_ContextActive())
    return false;
  const std::optional<int> snap = Binds_ParseKey(al_snap_key->GetString());
  if (!snap.has_value() || key != *snap)
    return false;
  g_snap_wait = kSnapPatience;
  return true;
}

bool Level_Holding() { return g_holding; }
bool Level_Snapping() { return g_snapping; }
bool Level_SlideToggled() { return g_slide; }
bool Level_BankToggled() { return g_bank; }

void Level_Apply(game_controls *controls) {
  g_holding = false;
  if (IsClassic() || !Flight_ContextActive() || Player_object == nullptr || Viewer_object != Player_object) {
    g_snapping = false;
    return;
  }
  g_slide = controls->toggle_slide;
  g_bank = controls->toggle_bank;
  // Any roll input wins, and cancels a snap in progress (AV-011). A tap just
  // after letting go of the bank key still sees it for a moment, so it waits.
  g_since_roll += Frametime;
  if (std::fabs(controls->bank_thrust) > 0.01f) {
    g_snapping = false;
    g_since_roll = 0.0f;
    g_snap_wait = std::max(0.0f, g_snap_wait - Frametime);
    return;
  }
  const bool tapped = g_snap_wait > 0.0f;
  g_snap_wait = 0.0f;

  const matrix &orient = Player_object->orient;
  const float up_right = orient.rvec.y(), up_up = orient.uvec.y();
  if (std::sqrt(up_right * up_right + up_up * up_up) < kNoRollAxis) {
    g_snapping = false;
    return;
  }
  const float roll = std::atan2(up_right, up_up) * 180.0f / 3.14159265f;
  if (tapped) {
    g_snapping = true;
    g_snap_target = NearestUpright(roll);
  }

  const physics_info &physics = Player_object->mtype.phys_info;
  float rate = 0.0f;
  if (g_snapping) {
    const float error = WrapDegrees(roll - g_snap_target);
    if (std::fabs(error) < kSnapDoneDeg)
      g_snapping = false;
    else
      rate = LevelRate(error, std::max(al_snap_time->GetFloat() / 3.0f, kSlowestLoop), 1000.0f); // three time constants: nearly there
  } else if (KeyDown(al_hold_key)) {
    g_holding = true;
    if (std::fabs(roll) > kDeadZoneDeg)
      rate = LevelRate(roll, kSlowestLoop, al_hold_rate->GetFloat());
  } else if (al_strength->GetFloat() > 0.0f && g_since_roll >= al_delay->GetFloat()) {
    if (std::fabs(roll) > kDeadZoneDeg)
      rate = LevelRate(roll, 0.5f / al_strength->GetFloat(), al_hold_rate->GetFloat());
  }
  controls->bank_thrust += BankCommand(rate, physics.full_rotthrust, physics.rotdrag);
}

} // namespace tartarus
