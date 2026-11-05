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

// Orientation aids on the HUD. See orient_hud.h.

#include "orient_hud.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "3d.h"
#include "bitmap.h"
#include "classic.h"
#include "cvar.h"
#include "game.h"
#include "grdefs.h"
#include "mouse_flight.h"
#include "object.h"
#include "orient_core.h"
#include "renderer.h"

namespace tartarus {

namespace {

Cvar *hud_uptick = nullptr;
Cvar *hud_uptick_len = nullptr;
Cvar *hud_uptick_fade_deg = nullptr;
Cvar *hud_attitude = nullptr;
Cvar *hud_attitude_size = nullptr;
Cvar *hud_attitude_range = nullptr;
Cvar *hud_attitude_x = nullptr;
Cvar *hud_attitude_y = nullptr;
Cvar *hud_gizmo = nullptr;
Cvar *hud_gizmo_size = nullptr;
Cvar *hud_gizmo_x = nullptr;
Cvar *hud_gizmo_y = nullptr;

const ddgr_color kTick = GR_RGB(255, 225, 120);
const ddgr_color kSky = GR_RGB(45, 100, 165);
const ddgr_color kGround = GR_RGB(115, 80, 45);
const ddgr_color kLadder = GR_RGB(235, 235, 225);
const ddgr_color kAxisColour[3] = {GR_RGB(225, 75, 65), GR_RGB(255, 255, 255), GR_RGB(80, 145, 235)};

constexpr int kDiscSegments = 32;

// One white texture lets a flat shape be a single textured polygon: the
// renderer has no flat 2D polygon, and a fan of lines is many draw calls.
int WhiteBitmap() {
  static int handle = -1;
  if (handle < 0) {
    handle = bm_AllocBitmap(32, 32, 0);
    if (handle >= 0) {
      uint16_t *pixels = bm_data(handle, 0);
      std::fill(pixels, pixels + 32 * 32, static_cast<uint16_t>(0xffff)); // white, opaque bit set
    }
  }
  return handle;
}

void BeginShapes() {
  StartFrame(0, 0, Max_window_w, Max_window_h, false);
  rend_SetOverlayType(OT_NONE);
  rend_SetZBufferState(0);
  rend_SetColorModel(CM_MONO);
  rend_SetAlphaType(AT_CONSTANT_TEXTURE);
}

void EndShapes() {
  rend_SetLighting(LS_NONE);
  rend_SetAlphaType(AT_ALWAYS);
  rend_SetZBufferState(1);
  EndFrame();
}

void FillShape(const std::vector<Point2> &shape, ddgr_color colour, float alpha) {
  const int bitmap = WhiteBitmap();
  if (bitmap < 0 || shape.size() < 3)
    return;
  std::vector<g3Point> points(shape.size());
  std::vector<g3Point *> pointers(shape.size());
  for (size_t i = 0; i < shape.size(); ++i) {
    points[i].p3_sx = shape[i].x;
    points[i].p3_sy = shape[i].y;
    points[i].p3_z = 1.0f;
    points[i].p3_flags = PF_PROJECTED;
    points[i].p3_u = points[i].p3_v = 0.5f;
    pointers[i] = &points[i];
  }
  rend_SetLighting(LS_FLAT_GOURAUD); // takes its colour from the flat colour
  rend_SetTextureType(TT_LINEAR);
  rend_SetFlatColor(colour);
  rend_SetAlphaValue(static_cast<uint8_t>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f));
  rend_DrawPolygon2D(bitmap, pointers.data(), static_cast<int>(pointers.size()));
}

float Scale() { return Game_window_h / 480.0f; }

Point2 ScreenPosition(const Cvar *x, const Cvar *y) {
  return {Game_window_x + x->GetFloat() * Game_window_w, Game_window_y + y->GetFloat() * Game_window_h};
}

// A triangle from base, half_width wide, out to tip, along (dir_x, dir_y).
std::vector<Point2> Wedge(Point2 centre, float dir_x, float dir_y, float base, float half_width, float tip) {
  return {{centre.x + dir_x * base - dir_y * half_width, centre.y + dir_y * base + dir_x * half_width},
          {centre.x + dir_x * tip, centre.y + dir_y * tip},
          {centre.x + dir_x * base + dir_y * half_width, centre.y + dir_y * base - dir_x * half_width}};
}

void DrawUpTick(const matrix &orient) {
  // World up is +Y everywhere: gravity is always along that axis (SPEC.md §9).
  const UpTick tick = ComputeUpTick(orient.rvec.y(), orient.uvec.y(), hud_uptick_fade_deg->GetFloat());
  if (tick.alpha <= 0.0f)
    return;
  const Point2 centre{Game_window_x + Game_window_w / 2.0f, Game_window_y + Game_window_h / 2.0f};
  const float scale = Scale();
  const float inner = std::max(24.0f, Game_window_h * 0.12f); // on the stick ring, with or without it
  const float length = hud_uptick_len->GetFloat() * scale;
  const float dir_x = std::sin(tick.angle), dir_y = -std::cos(tick.angle);
  // The dark one keeps it readable against bright walls.
  FillShape(Wedge(centre, dir_x, dir_y, inner - 2.0f * scale, length * 0.5f + 2.0f * scale, inner + length + 3.0f * scale),
            GR_BLACK, tick.alpha);
  FillShape(Wedge(centre, dir_x, dir_y, inner, length * 0.5f, inner + length), kTick, tick.alpha);
}

void DrawAttitude(const matrix &orient) {
  const Point2 centre = ScreenPosition(hud_attitude_x, hud_attitude_y);
  const float radius = hud_attitude_size->GetFloat() * 0.5f * Scale();
  const float range = hud_attitude_range->GetFloat();
  const float pitch = PitchDegrees(orient.fvec.y());
  const float roll = std::atan2(orient.rvec.y(), orient.uvec.y());
  // Ball space: x along the horizon, y toward world up on screen.
  const float up_x = std::sin(roll), up_y = -std::cos(roll), side_x = std::cos(roll), side_y = std::sin(roll);
  auto place = [&](const std::vector<Point2> &ball, float size) {
    std::vector<Point2> out;
    out.reserve(ball.size());
    for (const Point2 &p : ball)
      out.push_back({centre.x + size * (p.x * side_x + p.y * up_x), centre.y + size * (p.x * side_y + p.y * up_y)});
    return out;
  };

  const float rim = 2.0f * Scale();
  std::vector<Point2> disc = DiscAbove(-2.0f, kDiscSegments);
  FillShape(place(disc, radius + rim), GR_BLACK, 0.9f);
  const float horizon = BallOffset(0.0f, pitch, range);
  FillShape(place(DiscAbove(horizon, kDiscSegments), radius), kSky, 0.9f);
  FillShape(place(DiscBelow(horizon, kDiscSegments), radius), kGround, 0.9f);

  const float line = std::max(1.0f, 0.75f * Scale()) / radius;
  auto bar = [&](float y, float half_length, ddgr_color colour) {
    FillShape(place({{-half_length, y - line}, {half_length, y - line}, {half_length, y + line}, {-half_length, y + line}}, radius),
              colour, 1.0f);
  };
  bar(horizon, std::sqrt(std::max(0.0f, 1.0f - horizon * horizon)), kLadder);
  for (int mark = -80; mark <= 80; mark += 10) {
    const float y = BallOffset(static_cast<float>(mark), pitch, range);
    if (mark != 0 && std::fabs(y) < 0.85f)
      bar(y, mark % 30 == 0 ? 0.4f : 0.22f, kLadder);
  }
  // The fixed aircraft mark.
  const float wing = radius * 0.5f, thick = std::max(1.0f, Scale());
  FillShape({{centre.x - wing, centre.y - thick}, {centre.x - wing * 0.3f, centre.y - thick},
             {centre.x - wing * 0.3f, centre.y + thick}, {centre.x - wing, centre.y + thick}}, kTick, 1.0f);
  FillShape({{centre.x + wing * 0.3f, centre.y - thick}, {centre.x + wing, centre.y - thick},
             {centre.x + wing, centre.y + thick}, {centre.x + wing * 0.3f, centre.y + thick}}, kTick, 1.0f);
  FillShape({{centre.x - 2 * thick, centre.y - 2 * thick}, {centre.x + 2 * thick, centre.y - 2 * thick},
             {centre.x + 2 * thick, centre.y + 2 * thick}, {centre.x - 2 * thick, centre.y + 2 * thick}}, kTick, 1.0f);
}

void DrawGizmo(const matrix &orient) {
  const Point2 centre = ScreenPosition(hud_gizmo_x, hud_gizmo_y);
  const float radius = hud_gizmo_size->GetFloat() * 0.5f * Scale();
  const float right[3] = {orient.rvec.x(), orient.rvec.y(), orient.rvec.z()};
  const float up[3] = {orient.uvec.x(), orient.uvec.y(), orient.uvec.z()};
  const float forward[3] = {orient.fvec.x(), orient.fvec.y(), orient.fvec.z()};

  std::vector<Point2> back;
  for (const Point2 &p : DiscAbove(-2.0f, kDiscSegments))
    back.push_back({centre.x + p.x * radius * 1.15f, centre.y + p.y * radius * 1.15f});
  FillShape(back, GR_BLACK, 0.45f);

  for (const GizmoAxis &axis : ComputeGizmo(right, up, forward)) {
    const float length = std::sqrt(axis.x * axis.x + axis.y * axis.y);
    if (length < 0.05f)
      continue; // pointing at or away from the viewer
    const float dir_x = axis.x / length, dir_y = -axis.y / length;
    const bool world_up = axis.axis == 1;
    const float half = (world_up ? 1.5f : 1.0f) * Scale();
    const float reach = length * radius;
    const float alpha = world_up ? 1.0f : (axis.depth > 0.0f ? 0.55f : 0.85f);
    FillShape({{centre.x + dir_y * half, centre.y - dir_x * half},
               {centre.x + dir_x * reach + dir_y * half, centre.y + dir_y * reach - dir_x * half},
               {centre.x + dir_x * reach - dir_y * half, centre.y + dir_y * reach + dir_x * half},
               {centre.x - dir_y * half, centre.y + dir_x * half}},
              kAxisColour[axis.axis], alpha);
    if (world_up)
      FillShape(Wedge(centre, dir_x, dir_y, reach - 2.0f * half, 3.0f * half, reach + 3.0f * half), kAxisColour[1], 1.0f);
  }
}

} // namespace

void OrientHud_Init() {
  const uint32_t flags = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  CvarRegistry &cv = Cvars();
  hud_uptick = cv.RegisterBool("hud_uptick", true, "Wedge on the reticle pointing to world up; fades out when upright", flags);
  hud_uptick_len = cv.RegisterFloat("hud_uptick_len", 14.0f, 4.0f, 40.0f, "Up-tick length, in pixels of a 480-line view", flags);
  hud_uptick_fade_deg = cv.RegisterFloat("hud_uptick_fade_deg", 8.0f, 0.0f, 45.0f,
                                         "Up-tick: hidden within this many degrees of upright, full at three times that", flags);
  hud_attitude = cv.RegisterBool("hud_attitude", false, "Attitude ball: pitch and roll against the horizon", flags);
  hud_attitude_size = cv.RegisterFloat("hud_attitude_size", 64.0f, 32.0f, 160.0f, "Attitude ball size, in pixels of a 480-line view", flags);
  hud_attitude_range = cv.RegisterFloat("hud_attitude_range", 45.0f, 20.0f, 90.0f, "Attitude ball: degrees of pitch from the centre to the edge", flags);
  hud_attitude_x = cv.RegisterFloat("hud_attitude_x", 0.9f, 0.0f, 1.0f, "Attitude ball centre, 0 = left edge, 1 = right edge", flags);
  hud_attitude_y = cv.RegisterFloat("hud_attitude_y", 0.5f, 0.0f, 1.0f, "Attitude ball centre, 0 = top, 1 = bottom", flags);
  hud_gizmo = cv.RegisterBool("hud_gizmo", false, "Axis gizmo: world axes as the ship sees them, world up in white", flags);
  hud_gizmo_size = cv.RegisterFloat("hud_gizmo_size", 48.0f, 24.0f, 120.0f, "Gizmo size, in pixels of a 480-line view", flags);
  hud_gizmo_x = cv.RegisterFloat("hud_gizmo_x", 0.1f, 0.0f, 1.0f, "Gizmo centre, 0 = left edge, 1 = right edge", flags);
  hud_gizmo_y = cv.RegisterFloat("hud_gizmo_y", 0.5f, 0.0f, 1.0f, "Gizmo centre, 0 = top, 1 = bottom", flags);
}

void OrientHud_Render() {
  if (IsClassic())
    return;
  const bool tick = hud_uptick->GetBool(), ball = hud_attitude->GetBool(), gizmo = hud_gizmo->GetBool();
  if ((!tick && !ball && !gizmo) || !Flight_ContextActive() || Player_object == nullptr || Viewer_object != Player_object)
    return;
  const matrix &orient = Player_object->orient;
  BeginShapes();
  if (tick)
    DrawUpTick(orient);
  if (ball)
    DrawAttitude(orient);
  if (gizmo)
    DrawGizmo(orient);
  EndShapes();
}

} // namespace tartarus
