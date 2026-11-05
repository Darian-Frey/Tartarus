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

// Orientation maths (F-040 - F-042, SPEC.md §9). Pure standard C++, unit-tested; the
// drawing is in orient_hud.cpp.

#pragma once

#include <array>
#include <vector>

namespace tartarus {

struct UpTick {
  float angle = 0; // radians from the top of the screen, clockwise
  float alpha = 0; // 0 hidden, 1 full
};

/// up_right and up_up are world up measured along the ship's right and up
/// axes. The tick fades in between fade_deg and three times that from upright,
/// and fades out when up points along the view, where its direction means
/// nothing.
UpTick ComputeUpTick(float up_right, float up_up, float fade_deg);

struct Point2 {
  float x = 0, y = 0;
};

/// Pitch of the nose above the horizon, from world up along the ship's forward axis.
float PitchDegrees(float up_forward);

/// Where a pitch mark sits on the attitude ball, in ball radii along the
/// screen direction of world up. The horizon is the mark at 0 degrees.
float BallOffset(float mark_deg, float pitch_deg, float range_deg);

/// The part of the unit disc above the line y = h, as a convex polygon.
/// Empty if the line is above the disc; the whole disc if below it.
std::vector<Point2> DiscAbove(float h, int segments);

/// The part below it: DiscAbove() turned over.
std::vector<Point2> DiscBelow(float h, int segments);

struct GizmoAxis {
  float x = 0, y = 0;  // on screen, y up, length 1 at most
  float depth = 0;     // positive points away from the viewer
  int axis = 0;        // 0 X, 1 Y, 2 Z
};

/// World axes as the ship sees them, farthest first so nearer ones draw over
/// them. right, up and forward are the ship's axes in world space.
std::array<GizmoAxis, 3> ComputeGizmo(const float right[3], const float up[3], const float forward[3]);

} // namespace tartarus
