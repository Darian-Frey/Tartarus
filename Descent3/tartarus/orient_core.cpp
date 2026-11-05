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
#include <cstdio>

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

namespace {

float Dot(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

constexpr float kBehind = 0.001f;

} // namespace

void ViewCameraScale(ViewCamera *camera, float window_w, float window_h, float zoom) {
  // g3_StartFrame(): the window's aspect narrows x, and zoom is scaled by 3/4.
  const float s = window_h / window_w;
  const float view_zoom = zoom * 0.75f;
  camera->fx = (s <= 1.0f ? s : 1.0f / s) / view_zoom * window_w * 0.5f;
  camera->fy = window_h * 0.5f / view_zoom;
}

bool ProjectDirection(const ViewCamera &camera, const float direction[3], Point2 *out) {
  const float z = Dot(direction, camera.forward);
  if (z <= kBehind)
    return false;
  out->x = camera.cx + Dot(direction, camera.right) / z * camera.fx;
  out->y = camera.cy - Dot(direction, camera.up) / z * camera.fy;
  return true;
}

bool ClipLine(const ScreenRect &rect, Point2 *a, Point2 *b) {
  const float dx = b->x - a->x, dy = b->y - a->y;
  float t0 = 0.0f, t1 = 1.0f;
  const float p[4] = {-dx, dx, -dy, dy};
  const float q[4] = {a->x - rect.x1, rect.x2 - a->x, a->y - rect.y1, rect.y2 - a->y};
  for (int i = 0; i < 4; ++i) {
    if (p[i] == 0.0f) {
      if (q[i] < 0.0f)
        return false;
      continue;
    }
    const float t = q[i] / p[i];
    if (p[i] < 0.0f)
      t0 = std::max(t0, t);
    else
      t1 = std::min(t1, t);
  }
  if (t0 > t1)
    return false;
  const Point2 start = *a;
  *a = {start.x + dx * t0, start.y + dy * t0};
  *b = {start.x + dx * t1, start.y + dy * t1};
  return true;
}

void NoseHeading(const float forward[3], float *heading_x, float *heading_z) {
  const float length = std::sqrt(forward[0] * forward[0] + forward[2] * forward[2]);
  if (length < 0.1f)
    return;
  *heading_x = forward[0] / length;
  *heading_z = forward[2] / length;
}

bool ComputeLadderBar(const ViewCamera &camera, float heading_x, float heading_z, float mark_deg, float inner_deg,
                      float outer_deg, LadderBar *bar) {
  const float pitch = mark_deg * kPi / 180.0f;
  const float cos_p = std::cos(pitch), sin_p = std::sin(pitch);
  auto point = [&](float yaw_deg, Point2 *out) {
    const float yaw = yaw_deg * kPi / 180.0f;
    const float c = std::cos(yaw), s = std::sin(yaw);
    // The nose's ground direction turned by yaw, then raised by the mark.
    const float direction[3] = {cos_p * (c * heading_x + s * heading_z), sin_p, cos_p * (c * heading_z - s * heading_x)};
    return ProjectDirection(camera, direction, out);
  };
  return point(-outer_deg, &bar->outer_left) && point(-inner_deg, &bar->inner_left) &&
         point(inner_deg, &bar->inner_right) && point(outer_deg, &bar->outer_right);
}

float HeadingDegrees(float heading_x, float heading_z) {
  const float degrees = std::atan2(heading_x, heading_z) * 180.0f / kPi;
  return degrees < 0.0f ? degrees + 360.0f : degrees;
}

std::vector<HeadingMark> HeadingMarks(float heading, float span, int step) {
  std::vector<HeadingMark> marks;
  const int first = static_cast<int>(std::ceil((heading - span) / step)) * step;
  for (int mark = first; static_cast<float>(mark) <= heading + span; mark += step)
    marks.push_back({(static_cast<float>(mark) - heading) / span, ((mark % 360) + 360) % 360});
  return marks;
}

void HeadingLabel(int degrees, char *text, size_t size) {
  switch (degrees) {
  case 0:
    std::snprintf(text, size, "N");
    break;
  case 90:
    std::snprintf(text, size, "E");
    break;
  case 180:
    std::snprintf(text, size, "S");
    break;
  case 270:
    std::snprintf(text, size, "W");
    break;
  default:
    std::snprintf(text, size, "%02d", degrees / 10);
    break;
  }
}

float RollPointer(float roll, float limit, bool *pinned) {
  *pinned = std::fabs(roll) > limit;
  return std::clamp(roll, -limit, limit);
}

std::vector<TapeMark> TapeMarks(float value, float span, int step) {
  std::vector<TapeMark> marks;
  const int first = std::max(0, static_cast<int>(std::ceil((value - span) / step)) * step);
  for (int mark = first; static_cast<float>(mark) <= value + span; mark += step)
    marks.push_back({(static_cast<float>(mark) - value) / span, mark});
  return marks;
}

void SmoothValue(float *value, float target, float frame_time, float time_constant) {
  const float blend = time_constant > 0.0f ? 1.0f - std::exp(-frame_time / time_constant) : 1.0f;
  *value += (target - *value) * blend;
}

void ClimbLabel(float climb, char *text, size_t size) {
  const int rounded = static_cast<int>(std::lround(climb));
  if (rounded == 0)
    std::snprintf(text, size, "0");
  else
    std::snprintf(text, size, "%+d", rounded);
}

std::vector<ReticleLine> ReticleLines(int style) {
  std::vector<ReticleLine> lines;
  switch (style) {
  case kReticleGunsight: { // four arcs, broken at the compass points
    constexpr int kSteps = 6;
    for (int quadrant = 0; quadrant < 4; ++quadrant) {
      const float start = quadrant * kPi / 2.0f + 0.3f, end = (quadrant + 1) * kPi / 2.0f - 0.3f;
      for (int i = 0; i < kSteps; ++i) {
        const float a = start + (end - start) * i / kSteps, b = start + (end - start) * (i + 1) / kSteps;
        lines.push_back({std::cos(a), std::sin(a), std::cos(b), std::sin(b)});
      }
    }
    break;
  }
  case kReticleBrackets:
    for (float sx : {-1.0f, 1.0f}) {
      for (float sy : {-1.0f, 1.0f}) {
        lines.push_back({sx * 0.8f, sy * 0.8f, sx * 0.4f, sy * 0.8f});
        lines.push_back({sx * 0.8f, sy * 0.8f, sx * 0.8f, sy * 0.4f});
      }
    }
    break;
  case kReticleChevrons:
    for (float side : {-1.0f, 1.0f}) {
      lines.push_back({side * 1.0f, -0.35f, side * 0.55f, 0.0f});
      lines.push_back({side * 1.0f, 0.35f, side * 0.55f, 0.0f});
    }
    lines.push_back({-0.35f, 1.0f, 0.0f, 0.55f});
    lines.push_back({0.35f, 1.0f, 0.0f, 0.55f});
    break;
  case kReticleCross:
    lines.push_back({-1.0f, 0.0f, -0.35f, 0.0f});
    lines.push_back({0.35f, 0.0f, 1.0f, 0.0f});
    lines.push_back({0.0f, -1.0f, 0.0f, -0.35f});
    lines.push_back({0.0f, 0.35f, 0.0f, 1.0f});
    break;
  default:
    break;
  }
  return lines;
}

bool ReticleHasDot(int style) { return style == kReticleGunsight || style == kReticleBrackets || style == kReticleChevrons; }

ReticleBank DecodeReticleMask(unsigned exists_mask, unsigned ready_mask) {
  ReticleBank bank;
  struct Bit {
    unsigned bit;
    int slot;
  };
  for (const Bit &b : {Bit{0, kSlotCentre}, Bit{8, kSlotCentre}, Bit{1, kSlotRight1}, Bit{2, kSlotLeft1}, Bit{3, kSlotRight2},
                       Bit{4, kSlotLeft2}}) {
    bank.exists[b.slot] = bank.exists[b.slot] || ((exists_mask >> b.bit) & 1u);
    bank.ready[b.slot] = bank.ready[b.slot] || ((ready_mask >> b.bit) & 1u);
  }
  return bank;
}

float WrapDegrees(float degrees) {
  degrees = std::fmod(degrees + 180.0f, 360.0f);
  if (degrees < 0.0f)
    degrees += 360.0f;
  return degrees - 180.0f;
}

float NearestUpright(float roll_deg) { return std::round(roll_deg / 90.0f) * 90.0f; }

float LevelRate(float error_deg, float time_constant, float limit) {
  return std::clamp(error_deg / time_constant, -limit, limit);
}

float BankCommand(float rate_deg, float full_rotthrust, float rotdrag) {
  if (full_rotthrust <= 0.0f || rotdrag <= 0.0f)
    return 0.0f;
  const float top_rate = full_rotthrust / rotdrag * 360.0f / 65536.0f;
  // Roll is negative when rolled right, and needs bank left, which is positive.
  return std::clamp(-rate_deg / top_rate, -1.0f, 1.0f);
}

void SmoothVelocity(float smoothed[3], const float velocity[3], float frame_time, float time_constant) {
  for (int i = 0; i < 3; ++i)
    SmoothValue(&smoothed[i], velocity[i], frame_time, time_constant);
}

float FlightMarkerAlpha(const float velocity[3], float min_speed) {
  return Smoothstep(min_speed, min_speed * 2.0f, std::sqrt(Dot(velocity, velocity)));
}

bool ComputeFlightMarker(const ViewCamera &camera, const float velocity[3], const ScreenRect &rect, FlightMarker *marker) {
  const float speed = std::sqrt(Dot(velocity, velocity));
  if (speed <= 0.0f)
    return false;
  float direction[3] = {velocity[0] / speed, velocity[1] / speed, velocity[2] / speed};
  marker->reversed = Dot(direction, camera.forward) < 0.0f;
  if (marker->reversed) {
    for (float &d : direction)
      d = -d;
  }
  const float x = Dot(direction, camera.right), y = Dot(direction, camera.up);
  const float z = std::max(Dot(direction, camera.forward), 0.01f); // sliding sideways: far off to the side
  Point2 p{camera.cx + x / z * camera.fx, camera.cy - y / z * camera.fy};
  Point2 centre{camera.cx, camera.cy};
  marker->clamped = p.x < rect.x1 || p.x > rect.x2 || p.y < rect.y1 || p.y > rect.y2;
  if (marker->clamped) {
    ClipLine(rect, &centre, &p); // the end that stays is on the edge, on the way to the point
  }
  marker->position = p;
  return true;
}

} // namespace tartarus
