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

#include "gravwell_core.h"

#include <algorithm>
#include <cmath>

namespace tartarus {

float WellAcceleration(float surface_dist, float life_left, float mass, const WellSettings &s) {
  if (!std::isfinite(surface_dist) || !std::isfinite(life_left) || !std::isfinite(mass))
    return 0.0f;
  if (s.radius <= 0.0f || s.time <= 0.0f || s.force <= 0.0f || mass <= 0.0f || life_left <= 0.0f)
    return 0.0f;
  if (s.immune_mass > 0.0f && mass >= s.immune_mass)
    return 0.0f;
  const float d = std::max(0.0f, surface_dist);
  if (d >= s.radius)
    return 0.0f;
  const float fall = 1.0f - d / s.radius;
  const float fade = std::min(1.0f, life_left / s.time);
  const float scale = s.mass_ref > 0.0f ? std::clamp(s.mass_ref / mass, s.mass_min_scale, 1.0f) : 1.0f;
  return std::min(s.force, s.force * fall * fade * scale);
}

float WellDeltaV(float accel, float dt) {
  if (!std::isfinite(accel) || !std::isfinite(dt) || accel <= 0.0f || dt <= 0.0f)
    return 0.0f;
  return accel * dt;
}

} // namespace tartarus
