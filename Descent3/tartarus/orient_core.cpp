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

// Orientation maths. See orient_core.h.

#include "orient_core.h"

#include <algorithm>
#include <cmath>

namespace tartarus {

namespace {

constexpr float kPi = 3.14159265f;
// Length of world up in the view plane: sin of the angle from the view axis.
constexpr float kProjectionHide = 0.05f, kProjectionShow = 0.2f;

float Smoothstep(float edge0, float edge1, float x) {
  if (edge1 <= edge0)
    return x < edge0 ? 0.0f : 1.0f;
  const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

} // namespace

UpTick ComputeUpTick(float up_right, float up_up, float fade_deg) {
  UpTick tick;
  const float length = std::sqrt(up_right * up_right + up_up * up_up);
  if (length < kProjectionHide)
    return tick;
  tick.angle = std::atan2(up_right, up_up);
  const float degrees = std::fabs(tick.angle) * 180.0f / kPi;
  tick.alpha = Smoothstep(fade_deg, fade_deg * 3.0f, degrees) * Smoothstep(kProjectionHide, kProjectionShow, length);
  return tick;
}

} // namespace tartarus
