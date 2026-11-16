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

#include "phys_exact.h"

#include <array>
#include <cmath>

#include "classic.h"
#include "cvar.h"
#include "game.h"
#include "frame_clock_core.h"
#include "object.h"

namespace tartarus {

namespace {

Cvar *phys_exact_rot = nullptr;

constexpr float kUnitsToRadians = 6.2831853f / 65536.0f;

struct Carry {
  int handle = -1;
  float fraction = 0.0f;
};
std::array<Carry, MAX_OBJECTS> g_turnroll{};

bool Enabled() {
  return phys_exact_rot != nullptr && phys_exact_rot->GetBool() && !IsClassic() && !(Game_mode & GM_MULTI);
}

} // namespace

void PhysExact_Init() {
  phys_exact_rot = Cvars().RegisterBool("phys_exact_rot", true,
                                        "Turn objects by the exact angle each frame, not rounded down (F-061)",
                                        CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED);
}

void PhysExact_Rotation(const object &obj, const vector &rotvel0, const vector &rotvel1, const vector &rotthrust,
                        float frame_time, matrix *rotmat) {
  if (!Enabled() || !(frame_time > 0.0f))
    return;
  const float drag = obj.mtype.phys_info.rotdrag, mass = obj.mtype.phys_info.mass;
  const float p = RotationMeanSpeed(rotvel0.x(), rotvel1.x(), rotthrust.x(), drag, mass, frame_time) * frame_time * kUnitsToRadians;
  const float h = RotationMeanSpeed(rotvel0.y(), rotvel1.y(), rotthrust.y(), drag, mass, frame_time) * frame_time * kUnitsToRadians;
  const float b = RotationMeanSpeed(rotvel0.z(), rotvel1.z(), rotthrust.z(), drag, mass, frame_time) * frame_time * kUnitsToRadians;
  if (!std::isfinite(p) || !std::isfinite(h) || !std::isfinite(b))
    return;
  vm_SinCosToMatrix(rotmat, std::sin(p), std::cos(p), std::sin(b), std::cos(b), std::sin(h), std::cos(h));
}

int PhysExact_TurnrollStep(const object *obj, float exact, int stock) {
  if (!Enabled() || obj == nullptr || !std::isfinite(exact) || exact < 0.0f)
    return stock;
  const int index = static_cast<int>(obj - Objects);
  if (index < 0 || index >= MAX_OBJECTS)
    return stock;
  Carry &c = g_turnroll[static_cast<size_t>(index)];
  if (c.handle != obj->handle) // a new object in this slot
    c = Carry{obj->handle, 0.0f};
  const float step = exact + c.fraction;
  const int whole = static_cast<int>(step);
  c.fraction = step - static_cast<float>(whole);
  return whole;
}

} // namespace tartarus
