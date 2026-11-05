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
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "3d.h"
#include "bitmap.h"
#include "classic.h"
#include "cockpit.h"
#include "cvar.h"
#include "findintersection.h"
#include "game.h"
#include "gamefont.h"
#include "gameloop.h"
#include "grdefs.h"
#include "grtext.h"
#include "hud.h"
#include "mouse_flight.h"
#include "object.h"
#include "orient_core.h"
#include "renderer.h"

namespace tartarus {

namespace {

Cvar *hud_aids = nullptr;
Cvar *hud_flight = nullptr;
Cvar *hud_flight_fullscreen = nullptr;
Cvar *hud_flight_horizon = nullptr;
Cvar *hud_flight_horizon_deg = nullptr;
Cvar *hud_flight_ladder = nullptr;
Cvar *hud_flight_ladder_step = nullptr;
Cvar *hud_flight_ladder_limit = nullptr;
Cvar *hud_flight_fpm = nullptr;
Cvar *hud_flight_fpm_speed = nullptr;
Cvar *hud_flight_fpm_smooth = nullptr;
Cvar *hud_flight_alpha = nullptr;
Cvar *hud_flight_roll = nullptr;
Cvar *hud_flight_roll_radius = nullptr;
Cvar *hud_flight_heading = nullptr;
Cvar *hud_flight_heading_y = nullptr;
Cvar *hud_flight_heading_width = nullptr;
Cvar *hud_flight_heading_span = nullptr;
Cvar *hud_flight_speed = nullptr;
Cvar *hud_flight_range = nullptr;
Cvar *hud_flight_range_max = nullptr;
Cvar *hud_flight_range_smooth = nullptr;
Cvar *hud_flight_range_every = nullptr;
Cvar *hud_flight_climb = nullptr;
Cvar *hud_flight_tape_x = nullptr;
Cvar *hud_flight_tape_y = nullptr;
Cvar *hud_flight_tape_height = nullptr;
Cvar *hud_reticle = nullptr;
Cvar *hud_reticle_size = nullptr;
Cvar *hud_reticle_alpha = nullptr;
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

const ddgr_color kFlight = GR_RGB(150, 255, 190); // pale green, like a collimated HUD
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

// Shapes collect as triangles and go to the renderer one colour at a time.
std::vector<float> g_batch;
ddgr_color g_batch_colour = 0;
float g_batch_alpha = 0.0f;

void FlushShapes() {
  const int bitmap = WhiteBitmap();
  if (g_batch.empty() || bitmap < 0) {
    g_batch.clear();
    return;
  }
  rend_SetOverlayType(OT_NONE);
  rend_SetZBufferState(0);
  rend_SetColorModel(CM_MONO);
  rend_SetAlphaType(AT_CONSTANT_TEXTURE);
  rend_SetLighting(LS_FLAT_GOURAUD); // takes its colour from the flat colour
  rend_SetTextureType(TT_LINEAR);
  rend_SetFlatColor(g_batch_colour);
  rend_SetAlphaValue(static_cast<uint8_t>(std::clamp(g_batch_alpha, 0.0f, 1.0f) * 255.0f));
  rend_DrawTriangles2D(bitmap, g_batch.data(), static_cast<int>(g_batch.size() / 2));
  g_batch.clear();
}

void UseColour(ddgr_color colour, float alpha) {
  if (colour != g_batch_colour || alpha != g_batch_alpha) {
    FlushShapes();
    g_batch_colour = colour;
    g_batch_alpha = alpha;
  }
}

void AddTriangle(Point2 a, Point2 b, Point2 c) {
  g_batch.insert(g_batch.end(), {a.x, a.y, b.x, b.y, c.x, c.y});
}

void BeginShapes() { StartFrame(0, 0, Max_window_w, Max_window_h, false); }

void EndShapes() {
  FlushShapes();
  rend_SetLighting(LS_NONE);
  rend_SetAlphaType(AT_ALWAYS);
  rend_SetZBufferState(1);
  EndFrame();
}

void FillShape(const std::vector<Point2> &shape, ddgr_color colour, float alpha) {
  UseColour(colour, alpha);
  for (size_t i = 1; i + 1 < shape.size(); ++i)
    AddTriangle(shape[0], shape[i], shape[i + 1]);
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

// A line with some width: rend_DrawLine has neither width nor alpha.
void Stroke(Point2 a, Point2 b, float half_width, float alpha) {
  const float dx = b.x - a.x, dy = b.y - a.y;
  const float length = std::sqrt(dx * dx + dy * dy);
  if (length < 0.5f)
    return;
  const float nx = -dy / length * half_width, ny = dx / length * half_width;
  UseColour(kFlight, alpha);
  const Point2 p0{a.x + nx, a.y + ny}, p1{b.x + nx, b.y + ny}, p2{b.x - nx, b.y - ny}, p3{a.x - nx, a.y - ny};
  AddTriangle(p0, p1, p2);
  AddTriangle(p0, p2, p3);
}

Point2 Lerp(Point2 a, Point2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

ViewCamera WorldCamera(const matrix &orient) {
  ViewCamera camera{};
  const vector *axes[3] = {&orient.rvec, &orient.uvec, &orient.fvec};
  float *rows[3] = {camera.right, camera.up, camera.forward};
  for (int i = 0; i < 3; ++i) {
    rows[i][0] = axes[i]->x();
    rows[i][1] = axes[i]->y();
    rows[i][2] = axes[i]->z();
  }
  camera.cx = Game_window_x + Game_window_w / 2.0f;
  camera.cy = Game_window_y + Game_window_h / 2.0f;
  ViewCameraScale(&camera, static_cast<float>(Game_window_w), static_cast<float>(Game_window_h), Render_zoom);
  return camera;
}

float g_heading_x = 0.0f, g_heading_z = 1.0f; // the nose's ground direction, held near vertical
float g_velocity[3] = {};                     // smoothed, for the flight path marker

float g_range = 0.0f;                         // smoothed, for the range tape

// Text goes out once, after the shapes: every flush changes render state.
struct HudText {
  float x, y;
  int align; // -1 left, 0 centre, 1 right
  char text[16];
};
std::vector<HudText> g_text;

void AddText(float x, float y, int align, const char *text) {
  HudText item{x, y, align, ""};
  snprintf(item.text, sizeof(item.text), "%s", text);
  g_text.push_back(item);
}

void FlushText(float alpha) {
  FlushShapes();
  if (g_text.empty())
    return;
  grtext_SetFont(HUD_FONT);
  grtext_SetColor(kFlight);
  grtext_SetAlpha(static_cast<uint8_t>(alpha * 255.0f));
  for (const HudText &item : g_text) {
    const int width = grtext_GetTextLineWidth(item.text);
    const int x = static_cast<int>(item.x) - (item.align == 0 ? width / 2 : item.align > 0 ? width : 0);
    grtext_Printf(x, static_cast<int>(item.y), "%s", item.text);
  }
  grtext_Flush();
  g_text.clear();
}

void DrawHorizon(const ViewCamera &camera, float half_width, float alpha) {
  LadderBar bar;
  if (!ComputeLadderBar(camera, g_heading_x, g_heading_z, 0.0f, 3.0f, hud_flight_horizon_deg->GetFloat(), &bar))
    return;
  Stroke(bar.outer_left, bar.inner_left, half_width, alpha);
  Stroke(bar.inner_right, bar.outer_right, half_width, alpha);
}

void DrawLadder(const ViewCamera &camera, float half_width, float alpha) {
  const int step = std::max(5, hud_flight_ladder_step->GetInt());
  const float limit = hud_flight_ladder_limit->GetFloat() * Game_window_h;
  for (int mark = -90 + step; mark < 90; mark += step) {
    if (mark == 0)
      continue;
    LadderBar bar;
    if (!ComputeLadderBar(camera, g_heading_x, g_heading_z, static_cast<float>(mark), 3.0f, 9.0f, &bar))
      continue;
    // Only the middle of the view: further out it runs into the tape and the frame.
    const float mid_x = (bar.inner_left.x + bar.inner_right.x) / 2.0f - camera.cx;
    const float mid_y = (bar.inner_left.y + bar.inner_right.y) / 2.0f - camera.cy;
    if (std::sqrt(mid_x * mid_x + mid_y * mid_y) > limit)
      continue;
    if (mark > 0) {
      Stroke(bar.outer_left, bar.inner_left, half_width, alpha);
      Stroke(bar.inner_right, bar.outer_right, half_width, alpha);
    } else { // dashed below the horizon, as on a real ladder
      Stroke(bar.outer_left, Lerp(bar.outer_left, bar.inner_left, 0.4f), half_width, alpha);
      Stroke(Lerp(bar.outer_left, bar.inner_left, 0.6f), bar.inner_left, half_width, alpha);
      Stroke(bar.inner_right, Lerp(bar.inner_right, bar.outer_right, 0.4f), half_width, alpha);
      Stroke(Lerp(bar.inner_right, bar.outer_right, 0.6f), bar.outer_right, half_width, alpha);
    }
    char label[8];
    snprintf(label, sizeof(label), "%d", std::abs(mark));
    AddText(bar.outer_right.x + 4.0f, bar.outer_right.y - 6.0f, -1, label);
  }
}

void DrawFlightMarker(const ViewCamera &camera, const ScreenRect &rect, float half_width, float alpha) {
  const float strength = FlightMarkerAlpha(g_velocity, hud_flight_fpm_speed->GetFloat());
  FlightMarker marker;
  if (strength <= 0.0f || !ComputeFlightMarker(camera, g_velocity, rect, &marker))
    return;
  const float radius = 7.0f * Scale();
  const float a = alpha * strength * (marker.clamped ? 0.5f : 1.0f); // on the edge: only a direction
  const Point2 c = marker.position;
  constexpr int kSides = 10;
  for (int i = 0; i < kSides; ++i) {
    const float t0 = 6.2831853f * i / kSides, t1 = 6.2831853f * (i + 1) / kSides;
    Stroke({c.x + radius * std::cos(t0), c.y + radius * std::sin(t0)}, {c.x + radius * std::cos(t1), c.y + radius * std::sin(t1)},
           half_width, a);
  }
  if (marker.reversed) { // a cross through it: this is where you came from
    const float r = radius * 0.7f;
    Stroke({c.x - r, c.y - r}, {c.x + r, c.y + r}, half_width, a);
    Stroke({c.x - r, c.y + r}, {c.x + r, c.y - r}, half_width, a);
  } else { // wings and a fin
    Stroke({c.x - radius * 2.2f, c.y}, {c.x - radius, c.y}, half_width, a);
    Stroke({c.x + radius, c.y}, {c.x + radius * 2.2f, c.y}, half_width, a);
    Stroke({c.x, c.y - radius * 1.8f}, {c.x, c.y - radius}, half_width, a);
  }
}

void DrawRollArc(const ViewCamera &camera, const matrix &orient, float half_width, float alpha) {
  constexpr float kLimit = 60.0f * 3.14159265f / 180.0f;
  const float scale = Scale();
  const float radius = hud_flight_roll_radius->GetFloat() * Game_window_h;
  auto on_arc = [&](float angle, float r) { return Point2{camera.cx + std::sin(angle) * r, camera.cy - std::cos(angle) * r}; };
  constexpr int kSteps = 24;
  for (int i = 0; i < kSteps; ++i) {
    const float a0 = -kLimit + 2.0f * kLimit * i / kSteps, a1 = -kLimit + 2.0f * kLimit * (i + 1) / kSteps;
    Stroke(on_arc(a0, radius), on_arc(a1, radius), half_width, alpha);
  }
  for (int mark : {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60}) {
    const float angle = mark * 3.14159265f / 180.0f;
    const float length = (mark % 30 == 0 ? 9.0f : 5.0f) * scale;
    Stroke(on_arc(angle, radius), on_arc(angle, radius + length), half_width, alpha);
  }
  bool pinned = false;
  const float angle = RollPointer(std::atan2(orient.rvec.y(), orient.uvec.y()), kLimit, &pinned);
  const float dir_x = std::sin(angle), dir_y = -std::cos(angle);
  const Point2 centre{camera.cx, camera.cy};
  FillShape(Wedge(centre, dir_x, dir_y, radius - 9.0f * scale, 4.0f * scale, radius - 2.0f * scale), kFlight,
            pinned ? alpha * 0.5f : alpha); // pinned at the end: past the scale
}

void DrawHeadingTape(float half_width, float alpha) {
  const float heading = HeadingDegrees(g_heading_x, g_heading_z);
  const float scale = Scale();
  const float cx = Game_window_x + Game_window_w / 2.0f;
  const float y = Game_window_y + hud_flight_heading_y->GetFloat() * Game_window_h;
  const float half = hud_flight_heading_width->GetFloat() * Game_window_w;
  const float span = hud_flight_heading_span->GetFloat();
  Stroke({cx - half, y}, {cx + half, y}, half_width, alpha);

  const float label_y = y - 6.0f * scale - grtext_GetTextHeight("0") - 1.0f;
  for (const HeadingMark &mark : HeadingMarks(heading, span, 5)) {
    const float x = cx + mark.offset * half;
    const bool major = mark.degrees % 10 == 0;
    Stroke({x, y}, {x, y - (major ? 6.0f : 3.0f) * scale}, half_width, alpha);
    if (major) {
      char label[8];
      HeadingLabel(mark.degrees, label, sizeof(label));
      AddText(x, label_y, 0, label);
    }
  }
  FillShape({{cx, y + 2.0f * scale}, {cx + 4.0f * scale, y + 8.0f * scale}, {cx - 4.0f * scale, y + 8.0f * scale}}, kFlight, alpha);

  char bearing[8];
  snprintf(bearing, sizeof(bearing), "%03d", static_cast<int>(heading + 0.5f) % 360);
  AddText(cx, y + 9.0f * scale, 0, bearing);
}

// A vertical tape. side is -1 on the left of the view, 1 on the right: ticks
// and labels go outward, the reading sits in a box on the inner side.
void DrawTape(float x, float side, float value, float span, int step, int label_step, const char *caption, const char *reading,
              float half_width, float alpha) {
  const float scale = Scale();
  const float cy = Game_window_y + hud_flight_tape_y->GetFloat() * Game_window_h;
  const float half = hud_flight_tape_height->GetFloat() * Game_window_h;
  const float text_h = static_cast<float>(grtext_GetTextHeight("0"));
  Stroke({x, cy - half}, {x, cy + half}, half_width, alpha);
  for (const TapeMark &mark : TapeMarks(value, span, step)) {
    const float y = cy - mark.offset * half;
    const bool major = mark.value % label_step == 0;
    Stroke({x, y}, {x + side * (major ? 7.0f : 4.0f) * scale, y}, half_width, alpha);
    if (major) {
      char label[8];
      snprintf(label, sizeof(label), "%d", mark.value);
      AddText(x + side * 9.0f * scale, y - text_h / 2.0f, side < 0 ? 1 : -1, label);
    }
  }
  // The reading, in a box against the tape.
  const float bw = 24.0f * scale, bh = text_h / 2.0f + 3.0f * scale;
  const float near_x = x - side * 5.0f * scale, far_x = near_x - side * bw;
  Stroke({near_x, cy - bh}, {far_x, cy - bh}, half_width, alpha);
  Stroke({near_x, cy + bh}, {far_x, cy + bh}, half_width, alpha);
  Stroke({far_x, cy - bh}, {far_x, cy + bh}, half_width, alpha);
  Stroke({near_x, cy - bh}, {x, cy}, half_width, alpha);
  Stroke({near_x, cy + bh}, {x, cy}, half_width, alpha);
  AddText((near_x + far_x) / 2.0f, cy - text_h / 2.0f, 0, reading);
  AddText(x, cy - half - text_h - 3.0f * scale, 0, caption);
}

// One ray along the nose: walls, terrain and closed doors, not robots. Negative if clear.
float RangeAhead(const matrix &orient) {
  const float limit = hud_flight_range_max->GetFloat();
  vector start = Player_object->pos;
  vector end = start + orient.fvec * limit;
  fvi_query query{};
  fvi_info hit{};
  query.p0 = &start;
  query.p1 = &end;
  query.startroom = Player_object->roomnum;
  query.rad = 0.0f;
  query.thisobjnum = OBJNUM(Player_object);
  query.ignore_obj_list = nullptr;
  query.flags = FQ_CHECK_OBJS | FQ_ONLY_DOOR_OBJ | FQ_NO_RELINK;
  if (fvi_FindIntersection(&query, &hit) == HIT_NONE)
    return -1.0f;
  const vector gap = hit.hit_pnt - start;
  return std::sqrt(gap.x() * gap.x() + gap.y() * gap.y() + gap.z() * gap.z());
}

void DrawTapes(const matrix &orient, float half_width, float alpha) {
  const float left = Game_window_x + hud_flight_tape_x->GetFloat() * Game_window_w;
  const float right = Game_window_x + (1.0f - hud_flight_tape_x->GetFloat()) * Game_window_w;
  const float text_h = static_cast<float>(grtext_GetTextHeight("0"));
  char reading[16];
  if (hud_flight_speed->GetBool()) {
    const float speed = std::sqrt(g_velocity[0] * g_velocity[0] + g_velocity[1] * g_velocity[1] + g_velocity[2] * g_velocity[2]);
    snprintf(reading, sizeof(reading), "%d", static_cast<int>(std::lround(speed)));
    DrawTape(left, -1.0f, speed, 40.0f, 10, 20, "SPD", reading, half_width, alpha);
  }
  if (hud_flight_range->GetBool()) {
    const float limit = hud_flight_range_max->GetFloat();
    // The display is smoothed, so the ray need not run every frame.
    static int frames_left = 0;
    static float range = -1.0f;
    if (--frames_left <= 0) {
      range = RangeAhead(orient);
      frames_left = hud_flight_range_every->GetInt();
    }
    SmoothValue(&g_range, range < 0.0f ? limit : range, Frametime, hud_flight_range_smooth->GetFloat());
    if (range < 0.0f)
      snprintf(reading, sizeof(reading), "---");
    else
      snprintf(reading, sizeof(reading), "%d", static_cast<int>(std::lround(g_range)));
    DrawTape(right, 1.0f, g_range, 100.0f, 25, 50, "RNG", reading, half_width, alpha);
  }
  if (hud_flight_climb->GetBool()) {
    ClimbLabel(g_velocity[1], reading, sizeof(reading));
    const float y = Game_window_y + (hud_flight_tape_y->GetFloat() + hud_flight_tape_height->GetFloat()) * Game_window_h + 4.0f * Scale();
    AddText(right, y, 0, reading);
    AddText(right, y + text_h + 1.0f, 0, "VS");
  }
}

void DrawFlightHud(const matrix &orient) {
  const ViewCamera camera = WorldCamera(orient);
  const ScreenRect rect{static_cast<float>(Game_window_x), static_cast<float>(Game_window_y),
                        static_cast<float>(Game_window_x + Game_window_w), static_cast<float>(Game_window_y + Game_window_h)};
  const float half_width = std::max(0.6f, 0.45f * Scale());
  const float alpha = hud_flight_alpha->GetFloat();
  NoseHeading(camera.forward, &g_heading_x, &g_heading_z);
  const vector &v = Player_object->mtype.phys_info.velocity;
  const float velocity[3] = {v.x(), v.y(), v.z()};
  SmoothVelocity(g_velocity, velocity, Frametime, hud_flight_fpm_smooth->GetFloat());
  if (hud_flight_horizon->GetBool())
    DrawHorizon(camera, half_width, alpha);
  if (hud_flight_fpm->GetBool())
    DrawFlightMarker(camera, rect, half_width * 1.2f, alpha);
  if (hud_flight_roll->GetBool())
    DrawRollArc(camera, orient, half_width, alpha);
  if (hud_flight_ladder->GetBool())
    DrawLadder(camera, half_width, alpha);
  if (hud_flight_heading->GetBool())
    DrawHeadingTape(half_width, alpha);
  grtext_SetFont(HUD_FONT); // text heights above depend on it
  DrawTapes(orient, half_width, alpha);
  FlushText(alpha);
}

void ReticleBox(Point2 centre, float half_w, float half_h, ddgr_color colour, float alpha) {
  FillShape({{centre.x - half_w, centre.y - half_h},
             {centre.x + half_w, centre.y - half_h},
             {centre.x + half_w, centre.y + half_h},
             {centre.x - half_w, centre.y + half_h}},
            colour, alpha);
}

// Primaries are bars either side, missiles are pips underneath. Bright is
// ready to fire; dim is a slot that is not.
void DrawReticleBanks(Point2 c, float radius, float unit, float alpha, const ReticleBank &primary, const ReticleBank &secondary) {
  auto strength = [&](bool ready) { return ready ? alpha : alpha * 0.25f; };
  const struct {
    int slot;
    float x;
  } bars[] = {{kSlotLeft2, -1.75f}, {kSlotLeft1, -1.4f}, {kSlotRight1, 1.4f}, {kSlotRight2, 1.75f}};
  for (const auto &bar : bars) {
    if (primary.exists[bar.slot])
      ReticleBox({c.x + bar.x * radius, c.y}, unit, 4.0f * unit, kFlight, strength(primary.ready[bar.slot]));
  }
  if (primary.exists[kSlotCentre])
    ReticleBox({c.x, c.y - 1.4f * radius}, 3.0f * unit, unit, kFlight, strength(primary.ready[kSlotCentre]));
  const struct {
    int slot;
    float x;
  } pips[] = {{kSlotLeft2, -0.9f}, {kSlotLeft1, -0.45f}, {kSlotCentre, 0.0f}, {kSlotRight1, 0.45f}, {kSlotRight2, 0.9f}};
  for (const auto &pip : pips) {
    if (secondary.exists[pip.slot])
      ReticleBox({c.x + pip.x * radius, c.y + 1.45f * radius}, 2.0f * unit, 2.0f * unit, kFlight, strength(secondary.ready[pip.slot]));
  }
}

bool Flying() { return Flight_ContextActive() && Player_object != nullptr && Viewer_object == Player_object; }

} // namespace

void OrientHud_Init() {
  const uint32_t flags = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  CvarRegistry &cv = Cvars();
  hud_aids = cv.RegisterBool("hud_aids", true, "Master switch for every orientation aid", flags);
  hud_flight = cv.RegisterBool("hud_flight", true, "Flight HUD on the canopy in cockpit view", flags);
  hud_flight_fullscreen = cv.RegisterBool("hud_flight_fullscreen", false, "Flight HUD in the full-screen HUD view too", flags);
  hud_flight_horizon = cv.RegisterBool("hud_flight_horizon", true, "Flight HUD: horizon line", flags);
  hud_flight_horizon_deg = cv.RegisterFloat("hud_flight_horizon_deg", 20.0f, 6.0f, 60.0f, "Flight HUD: horizon bar, degrees each side of the nose", flags);
  hud_flight_ladder = cv.RegisterBool("hud_flight_ladder", true, "Flight HUD: pitch ladder", flags);
  hud_flight_ladder_step = cv.RegisterInt("hud_flight_ladder_step", 10, 5, 30, "Flight HUD: degrees between ladder marks", flags);
  hud_flight_ladder_limit = cv.RegisterFloat("hud_flight_ladder_limit", 0.3f, 0.1f, 0.6f, "Flight HUD: ladder marks shown this far from the centre, as a fraction of the view height", flags);
  hud_flight_fpm = cv.RegisterBool("hud_flight_fpm", true, "Flight HUD: flight path marker, where the ship is going", flags);
  hud_flight_fpm_speed = cv.RegisterFloat("hud_flight_fpm_speed", 6.0f, 0.0f, 30.0f, "Flight path marker: hidden below this speed, full at twice it", flags);
  hud_flight_fpm_smooth = cv.RegisterFloat("hud_flight_fpm_smooth", 0.2f, 0.0f, 1.0f, "Flight path marker: smoothing time in seconds", flags);
  hud_flight_alpha = cv.RegisterFloat("hud_flight_alpha", 0.85f, 0.2f, 1.0f, "Flight HUD brightness", flags);
  hud_flight_roll = cv.RegisterBool("hud_flight_roll", true, "Flight HUD: roll arc", flags);
  hud_flight_roll_radius = cv.RegisterFloat("hud_flight_roll_radius", 0.34f, 0.15f, 0.48f, "Roll arc radius, as a fraction of the view height", flags);
  hud_flight_heading = cv.RegisterBool("hud_flight_heading", true, "Flight HUD: heading tape", flags);
  hud_flight_heading_y = cv.RegisterFloat("hud_flight_heading_y", 0.1f, 0.03f, 0.5f, "Heading tape height, 0 = top of the view", flags);
  hud_flight_heading_width = cv.RegisterFloat("hud_flight_heading_width", 0.2f, 0.05f, 0.45f, "Heading tape half-width, as a fraction of the view width", flags);
  hud_flight_heading_span = cv.RegisterFloat("hud_flight_heading_span", 30.0f, 10.0f, 90.0f, "Heading tape: degrees shown each side of the centre", flags);
  hud_flight_speed = cv.RegisterBool("hud_flight_speed", true, "Flight HUD: speed tape", flags);
  hud_flight_range = cv.RegisterBool("hud_flight_range", true, "Flight HUD: range to the wall ahead", flags);
  hud_flight_range_max = cv.RegisterFloat("hud_flight_range_max", 400.0f, 100.0f, 2000.0f, "Range tape: furthest distance measured", flags);
  hud_flight_range_smooth = cv.RegisterFloat("hud_flight_range_smooth", 0.1f, 0.0f, 1.0f, "Range tape: smoothing time in seconds", flags);
  hud_flight_range_every = cv.RegisterInt("hud_flight_range_every", 4, 1, 30, "Range tape: frames between measurements", flags);
  hud_flight_climb = cv.RegisterBool("hud_flight_climb", true, "Flight HUD: climb rate", flags);
  hud_flight_tape_x = cv.RegisterFloat("hud_flight_tape_x", 0.17f, 0.03f, 0.4f, "Tapes: distance from the side of the view, as a fraction of its width", flags);
  hud_flight_tape_y = cv.RegisterFloat("hud_flight_tape_y", 0.4f, 0.2f, 0.7f, "Tapes: centre height, 0 = top of the view", flags);
  hud_flight_tape_height = cv.RegisterFloat("hud_flight_tape_height", 0.18f, 0.08f, 0.3f, "Tapes: half-height, as a fraction of the view height", flags);
  hud_reticle = cv.RegisterInt("hud_reticle", kReticleGunsight, kReticleStock, kReticleStyles - 1,
                               "Reticle: 0 stock, 1 gunsight, 2 brackets, 3 chevrons, 4 cross", flags);
  hud_reticle_size = cv.RegisterFloat("hud_reticle_size", 1.0f, 0.5f, 2.0f, "Reticle size", flags);
  hud_reticle_alpha = cv.RegisterFloat("hud_reticle_alpha", 0.9f, 0.3f, 1.0f, "Reticle brightness", flags);
  hud_uptick = cv.RegisterBool("hud_uptick", false, "Wedge on the reticle pointing to world up; fades out when upright", flags);
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

bool OrientHud_RenderReticle(float cx, float cy, unsigned primary_exists, unsigned primary_ready, unsigned secondary_exists,
                             unsigned secondary_ready) {
  const int style = hud_reticle->GetInt();
  if (IsClassic() || style <= kReticleStock || style >= kReticleStyles)
    return false;
  const float scale = Scale() * hud_reticle_size->GetFloat();
  const float radius = 18.0f * scale;
  const float alpha = hud_reticle_alpha->GetFloat();
  const float half_width = std::max(0.6f, 0.5f * scale);
  const float unit = std::max(1.0f, 0.75f * scale);
  const Point2 c{cx, cy};
  BeginShapes();
  for (const ReticleLine &line : ReticleLines(style))
    Stroke({c.x + line.x1 * radius, c.y + line.y1 * radius}, {c.x + line.x2 * radius, c.y + line.y2 * radius}, half_width, alpha);
  if (ReticleHasDot(style))
    ReticleBox(c, unit, unit, kFlight, alpha);
  DrawReticleBanks(c, radius, unit, alpha, DecodeReticleMask(primary_exists, primary_ready),
                   DecodeReticleMask(secondary_exists, secondary_ready));
  EndShapes();
  return true;
}

void OrientHud_RenderCanopy() {
  if (IsClassic() || !hud_aids->GetBool() || !hud_flight->GetBool() || CockpitState() != COCKPIT_STATE_FUNCTIONAL || !Flying())
    return;
  BeginShapes();
  DrawFlightHud(Player_object->orient);
  EndShapes();
}

void OrientHud_Render() {
  if (IsClassic() || !hud_aids->GetBool())
    return;
  const bool tick = hud_uptick->GetBool(), ball = hud_attitude->GetBool(), gizmo = hud_gizmo->GetBool();
  const bool flight = hud_flight_fullscreen->GetBool() && GetHUDMode() == HUD_FULLSCREEN;
  if ((!tick && !ball && !gizmo && !flight) || !Flying())
    return;
  const matrix &orient = Player_object->orient;
  BeginShapes();
  if (flight)
    DrawFlightHud(orient);
  if (tick)
    DrawUpTick(orient);
  if (ball)
    DrawAttitude(orient);
  if (gizmo)
    DrawGizmo(orient);
  EndShapes();
}

} // namespace tartarus
