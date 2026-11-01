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

// Virtual-joystick offset model. See mouse_flight_core.h.

#include "mouse_flight_core.h"

#include <algorithm>
#include <cmath>

namespace tartarus {

float VjCurve(float v, float deadzone, float curve) {
  if (!std::isfinite(v))
    return 0.0f;
  deadzone = std::clamp(deadzone, 0.0f, 0.95f);
  const float mag = std::fabs(v);
  if (mag <= deadzone)
    return 0.0f;
  const float t = std::min(1.0f, (mag - deadzone) / (1.0f - deadzone));
  const float out = std::pow(t, std::max(0.1f, curve));
  return std::copysign(std::min(out, 1.0f), v);
}

void VirtualJoystick::AddDelta(float dx, float dy, float radius_counts, float sensitivity) {
  if (radius_counts <= 0.0f || !std::isfinite(dx) || !std::isfinite(dy))
    return;
  x_ += dx * sensitivity / radius_counts;
  y_ += dy * sensitivity / radius_counts;
  const float len = std::sqrt(x_ * x_ + y_ * y_);
  if (len > 1.0f) {
    x_ /= len;
    y_ /= len;
  }
}

void VirtualJoystick::Decay(float rate, float dt) {
  if (rate <= 0.0f || dt <= 0.0f)
    return;
  const float k = std::exp(-rate * dt);
  x_ *= k;
  y_ *= k;
  // Snap tiny residues to zero so the ship really stops.
  if (std::fabs(x_) < 1e-3f && std::fabs(y_) < 1e-3f)
    Reset();
}

} // namespace tartarus
