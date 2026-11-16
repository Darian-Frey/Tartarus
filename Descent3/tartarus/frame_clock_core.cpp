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

#include "frame_clock_core.h"

#include <cmath>

namespace tartarus {

float RefineFrametime(float stock_s, float precise_s) {
  if (!std::isfinite(precise_s) || precise_s < 0.0f)
    return stock_s;
  if (std::fabs(precise_s - stock_s) > 0.0015f) // more than the 1 ms rounding: a pause or a jump
    return stock_s;
  return precise_s < kMinFrametime ? kMinFrametime : precise_s;
}

int64_t FrameWaitUs(int64_t start_us, int64_t now_us, float fps) {
  if (!(fps > 0.0f) || !std::isfinite(fps))
    return 0;
  const int64_t target = start_us + static_cast<int64_t>(std::llround(1e6 / fps));
  return now_us < target ? target - now_us : 0;
}

float RotationMeanSpeed(float v0, float v1, float force, float drag, float mass, float dt) {
  if (!(drag > 0.0f) || !(mass > 0.0f) || !(dt > 0.0f))
    return v1;
  const double k = static_cast<double>(drag) / mass;
  const double terminal = static_cast<double>(force) / drag;
  // v(t) = terminal + (v0 - terminal) e^-kt; its mean over dt is terminal + (v0 - v1) / (k dt).
  const double mean = terminal + (static_cast<double>(v0) - v1) / (k * dt);
  return std::isfinite(mean) ? static_cast<float>(mean) : v1;
}

} // namespace tartarus
