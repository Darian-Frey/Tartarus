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

float PitchDegrees(float up_forward) { return std::asin(std::clamp(up_forward, -1.0f, 1.0f)) * 180.0f / kPi; }

float BallOffset(float mark_deg, float pitch_deg, float range_deg) { return (mark_deg - pitch_deg) / range_deg; }

std::vector<Point2> DiscAbove(float h, int segments) {
  std::vector<Point2> points;
  if (h >= 1.0f)
    return points;
  if (h <= -1.0f) {
    for (int i = 0; i < segments; ++i) {
      const float t = 2.0f * kPi * i / segments;
      points.push_back({std::cos(t), std::sin(t)});
    }
    return points;
  }
  const float start = std::asin(h), end = kPi - start; // the arc above the line; the chord closes it
  for (int i = 0; i <= segments; ++i) {
    const float t = start + (end - start) * i / segments;
    points.push_back({std::cos(t), std::sin(t)});
  }
  return points;
}

std::vector<Point2> DiscBelow(float h, int segments) {
  std::vector<Point2> points = DiscAbove(-h, segments);
  for (Point2 &p : points)
    p.y = -p.y;
  return points;
}

std::array<GizmoAxis, 3> ComputeGizmo(const float right[3], const float up[3], const float forward[3]) {
  std::array<GizmoAxis, 3> axes;
  for (int i = 0; i < 3; ++i)
    axes[static_cast<size_t>(i)] = {right[i], up[i], forward[i], i};
  std::sort(axes.begin(), axes.end(), [](const GizmoAxis &a, const GizmoAxis &b) { return a.depth > b.depth; });
  return axes;
}

} // namespace tartarus
