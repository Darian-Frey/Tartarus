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

// Debug overlay maths (F-013): world points on the main view. Pure standard
// C++, unit-tested; the drawing is in debugdraw.cpp.

#pragma once

#include "orient_core.h"

namespace tartarus {

/// Nearest distance in front of the eye that is drawn; anything closer is cut off.
inline constexpr float kDebugNear = 0.5f;

/// A world point on screen, and its distance along the view. False if it is
/// nearer than kDebugNear (or behind the eye).
bool ProjectPoint(const ViewCamera &camera, const float eye[3], const float point[3], Point2 *out, float *depth);

/// A world line on screen, cut where it crosses kDebugNear. False if all of it
/// is behind that.
bool ProjectSegment(const ViewCamera &camera, const float eye[3], const float a[3], const float b[3], Point2 *pa,
                    Point2 *pb);

/// Screen radius in pixels of a sphere of radius r at depth.
float ProjectedRadius(const ViewCamera &camera, float depth, float radius);

} // namespace tartarus
