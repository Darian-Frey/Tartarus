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
#include <cstdlib>
#include <utility>
#include <vector>

#include "3d.h"
#include "bitmap.h"
#include "classic.h"
#include "cockpit.h"
#include "cvar.h"
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
Cvar *hud_flight_fpm = nullptr;
Cvar *hud_flight_fpm_speed = nullptr;
Cvar *hud_flight_fpm_smooth = nullptr;
Cvar *hud_flight_alpha = nullptr;
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

// A line with some width: rend_DrawLine has neither width nor alpha.
void Stroke(Point2 a, Point2 b, float half_width, float alpha) {
  const float dx = b.x - a.x, dy = b.y - a.y;
  const float length = std::sqrt(dx * dx + dy * dy);
  if (length < 0.5f)
    return;
  const float nx = -dy / length * half_width, ny = dx / length * half_width;
  FillShape({{a.x + nx, a.y + ny}, {b.x + nx, b.y + ny}, {b.x - nx, b.y - ny}, {a.x - nx, a.y - ny}}, kFlight, alpha);
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

void DrawHorizon(const ViewCamera &camera, float half_width, float alpha) {
  LadderBar bar;
  if (!ComputeLadderBar(camera, g_heading_x, g_heading_z, 0.0f, 3.0f, hud_flight_horizon_deg->GetFloat(), &bar))
    return;
  Stroke(bar.outer_left, bar.inner_left, half_width, alpha);
  Stroke(bar.inner_right, bar.outer_right, half_width, alpha);
}

void DrawLadder(const ViewCamera &camera, float half_width, float alpha) {
  const int step = std::max(5, hud_flight_ladder_step->GetInt());
  std::vector<std::pair<Point2, int>> labels;
  for (int mark = -90 + step; mark < 90; mark += step) {
    if (mark == 0)
      continue;
    LadderBar bar;
    if (!ComputeLadderBar(camera, g_heading_x, g_heading_z, static_cast<float>(mark), 3.0f, 9.0f, &bar))
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
    labels.emplace_back(bar.outer_right, std::abs(mark));
  }
  grtext_SetFont(HUD_FONT);
  grtext_SetColor(kFlight);
  grtext_SetAlpha(static_cast<uint8_t>(alpha * 255.0f));
  for (const auto &[at, value] : labels) {
    if (at.x > Game_window_x && at.x < Game_window_x + Game_window_w - 24 && at.y > Game_window_y &&
        at.y < Game_window_y + Game_window_h - 16)
      grtext_Printf(static_cast<int>(at.x) + 4, static_cast<int>(at.y) - 6, "%d", value);
  }
  grtext_Flush();
}

void DrawFlightMarker(const ViewCamera &camera, const ScreenRect &rect, float half_width, float alpha) {
  const vector &v = Player_object->mtype.phys_info.velocity;
  const float velocity[3] = {v.x(), v.y(), v.z()};
  SmoothVelocity(g_velocity, velocity, Frametime, hud_flight_fpm_smooth->GetFloat());
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

void DrawFlightHud(const matrix &orient) {
  const ViewCamera camera = WorldCamera(orient);
  const ScreenRect rect{static_cast<float>(Game_window_x), static_cast<float>(Game_window_y),
                        static_cast<float>(Game_window_x + Game_window_w), static_cast<float>(Game_window_y + Game_window_h)};
  const float half_width = std::max(0.6f, 0.45f * Scale());
  const float alpha = hud_flight_alpha->GetFloat();
  NoseHeading(camera.forward, &g_heading_x, &g_heading_z);
  if (hud_flight_horizon->GetBool())
    DrawHorizon(camera, half_width, alpha);
  if (hud_flight_fpm->GetBool())
    DrawFlightMarker(camera, rect, half_width * 1.2f, alpha);
  if (hud_flight_ladder->GetBool())
    DrawLadder(camera, half_width, alpha);
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
  hud_flight_fpm = cv.RegisterBool("hud_flight_fpm", true, "Flight HUD: flight path marker, where the ship is going", flags);
  hud_flight_fpm_speed = cv.RegisterFloat("hud_flight_fpm_speed", 6.0f, 0.0f, 30.0f, "Flight path marker: hidden below this speed, full at twice it", flags);
  hud_flight_fpm_smooth = cv.RegisterFloat("hud_flight_fpm_smooth", 0.2f, 0.0f, 1.0f, "Flight path marker: smoothing time in seconds", flags);
  hud_flight_alpha = cv.RegisterFloat("hud_flight_alpha", 0.85f, 0.2f, 1.0f, "Flight HUD brightness", flags);
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
