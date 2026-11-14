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

// Debug overlay maths. See debug_core.h.

#include "debug_core.h"

namespace tartarus {

namespace {

void Sub(const float a[3], const float b[3], float out[3]) {
  for (int i = 0; i < 3; ++i)
    out[i] = a[i] - b[i];
}

float Dot3(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

// View-space coordinates of a world point: x right, y up, z along the view.
void ToView(const ViewCamera &camera, const float eye[3], const float point[3], float view[3]) {
  float d[3];
  Sub(point, eye, d);
  view[0] = Dot3(d, camera.right);
  view[1] = Dot3(d, camera.up);
  view[2] = Dot3(d, camera.forward);
}

Point2 ToScreen(const ViewCamera &camera, const float view[3]) {
  return {camera.cx + view[0] / view[2] * camera.fx, camera.cy - view[1] / view[2] * camera.fy};
}

} // namespace

bool ProjectPoint(const ViewCamera &camera, const float eye[3], const float point[3], Point2 *out, float *depth) {
  float view[3];
  ToView(camera, eye, point, view);
  if (view[2] < kDebugNear)
    return false;
  *out = ToScreen(camera, view);
  if (depth != nullptr)
    *depth = view[2];
  return true;
}

bool ProjectSegment(const ViewCamera &camera, const float eye[3], const float a[3], const float b[3], Point2 *pa,
                    Point2 *pb) {
  float va[3], vb[3];
  ToView(camera, eye, a, va);
  ToView(camera, eye, b, vb);
  if (va[2] < kDebugNear && vb[2] < kDebugNear)
    return false;
  // Move the end that is too near along the line to the near plane.
  float *near_end = va[2] < kDebugNear ? va : vb[2] < kDebugNear ? vb : nullptr;
  if (near_end != nullptr) {
    const float *far_end = near_end == va ? vb : va;
    const float t = (kDebugNear - far_end[2]) / (near_end[2] - far_end[2]);
    for (int i = 0; i < 3; ++i)
      near_end[i] = far_end[i] + (near_end[i] - far_end[i]) * t;
    near_end[2] = kDebugNear;
  }
  *pa = ToScreen(camera, va);
  *pb = ToScreen(camera, vb);
  return true;
}

float ProjectedRadius(const ViewCamera &camera, float depth, float radius) {
  return depth > 0.0f ? radius / depth * camera.fy : 0.0f;
}

} // namespace tartarus
