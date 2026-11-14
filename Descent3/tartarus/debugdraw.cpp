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

// Debug overlays (F-013). See debugdraw.h.

#include "debugdraw.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "cheats.h"
#include "classic.h"
#include "cvar.h"
#include "debug_core.h"
#include "game.h"
#include "gamefont.h"
#include "gameloop.h"
#include "gamesequence.h"
#include "grtext.h"
#include "object.h"
#include "objinfo.h"
#include "player.h"
#include "renderer.h"
#include "weapon.h"

namespace tartarus {

namespace {

Cvar *r_showaxes = nullptr;
Cvar *r_showobjinfo = nullptr;
Cvar *r_showcollide = nullptr;
Cvar *r_showaxes_size = nullptr;
Cvar *r_showaxes_ahead = nullptr;
Cvar *r_show_range = nullptr;
Cvar *r_show_labels = nullptr;

bool g_any = false; // any overlay on: the only test made each frame while they are all off

void UpdateAny() { g_any = r_showaxes->GetBool() || r_showobjinfo->GetBool() || r_showcollide->GetBool(); }

const ddgr_color kAxis[3] = {GR_RGB(255, 80, 80), GR_RGB(80, 255, 80), GR_RGB(90, 140, 255)};
const ddgr_color kRobot = GR_RGB(255, 210, 60);
const ddgr_color kOther = GR_RGB(90, 230, 230);
const ddgr_color kLabel = GR_RGB(235, 235, 235);

void ToArray(const vector &v, float out[3]) {
  out[0] = v.x();
  out[1] = v.y();
  out[2] = v.z();
}

ViewCamera MainCamera(const object &viewer) {
  ViewCamera camera{};
  ToArray(viewer.orient.rvec, camera.right);
  ToArray(viewer.orient.uvec, camera.up);
  ToArray(viewer.orient.fvec, camera.forward);
  camera.cx = Game_window_x + Game_window_w / 2.0f;
  camera.cy = Game_window_y + Game_window_h / 2.0f;
  ViewCameraScale(&camera, static_cast<float>(Game_window_w), static_cast<float>(Game_window_h), Render_zoom);
  return camera;
}

const ScreenRect &View() {
  static ScreenRect rect;
  rect = {static_cast<float>(Game_window_x), static_cast<float>(Game_window_y),
          static_cast<float>(Game_window_x + Game_window_w - 1), static_cast<float>(Game_window_y + Game_window_h - 1)};
  return rect;
}

void Line(Point2 a, Point2 b, ddgr_color colour) {
  if (!ClipLine(View(), &a, &b))
    return;
  rend_SetFlatColor(colour);
  rend_DrawLine(static_cast<int>(a.x), static_cast<int>(a.y), static_cast<int>(b.x), static_cast<int>(b.y));
}

void WorldLine(const ViewCamera &camera, const float eye[3], const float a[3], const float b[3], ddgr_color colour) {
  Point2 pa, pb;
  if (ProjectSegment(camera, eye, a, b, &pa, &pb))
    Line(pa, pb, colour);
}

// World X, Y and Z from the player: ahead of the eye in cockpit view, where a
// line through the eye would shrink to a point.
void DrawAxes(const ViewCamera &camera, const float eye[3], const object &viewer) {
  vector anchor = Player_object->pos;
  if (&viewer == Player_object)
    anchor += Player_object->orient.fvec * r_showaxes_ahead->GetFloat();
  float origin[3];
  ToArray(anchor, origin);
  const float size = r_showaxes_size->GetFloat();
  for (int axis = 0; axis < 3; ++axis) {
    float tip[3] = {origin[0], origin[1], origin[2]};
    tip[axis] += size;
    WorldLine(camera, eye, origin, tip, kAxis[axis]);
  }
}

// The object types drawn, by name; nullptr for the rest. (Object_type_names
// is only defined in the editor.)
const char *TypeName(const object &obj) {
  switch (obj.type) {
  case OBJ_ROBOT:
    return "ROBOT";
  case OBJ_POWERUP:
    return "POWERUP";
  case OBJ_CLUTTER:
    return "CLUTTER";
  case OBJ_BUILDING:
    return "BUILDING";
  case OBJ_WEAPON:
    return "WEAPON";
  case OBJ_PLAYER:
    return "PLAYER";
  default:
    return nullptr;
  }
}

const char *ObjectName(const object &obj) {
  if (IS_GENERIC(obj.type))
    return Object_info[obj.id].name;
  if (obj.type == OBJ_WEAPON)
    return Weapons[obj.id].name;
  return "";
}

struct Nearby {
  int objnum;
  float depth;
  Point2 at;
};

void DrawObjects(const ViewCamera &camera, const float eye[3], const object &viewer) {
  const float range = r_show_range->GetFloat();
  std::vector<Nearby> found;
  for (int i = 0; i <= Highest_object_index; ++i) {
    const object &obj = Objects[i];
    if (&obj == &viewer || TypeName(obj) == nullptr || (obj.flags & OF_DEAD))
      continue;
    float pos[3];
    ToArray(obj.pos, pos);
    Nearby n{i, 0.0f, {}};
    if (!ProjectPoint(camera, eye, pos, &n.at, &n.depth) || n.depth > range)
      continue;
    found.push_back(n);
  }

  if (r_showcollide->GetBool()) {
    for (const Nearby &n : found) {
      const object &obj = Objects[n.objnum];
      const float r = ProjectedRadius(camera, n.depth, obj.size);
      if (r < 1.0f)
        continue;
      // The sphere's outline: its silhouette is a circle on screen, near enough.
      const int segments = std::clamp(static_cast<int>(r / 3.0f), 12, 48);
      const ddgr_color colour = obj.type == OBJ_ROBOT ? kRobot : kOther;
      Point2 prev{n.at.x + r, n.at.y};
      for (int s = 1; s <= segments; ++s) {
        const float a = 6.2831853f * s / segments;
        const Point2 next{n.at.x + r * std::cos(a), n.at.y + r * std::sin(a)};
        Line(prev, next, colour);
        prev = next;
      }
    }
  }

  if (r_showobjinfo->GetBool()) {
    // The nearest first, up to the label limit.
    std::sort(found.begin(), found.end(), [](const Nearby &a, const Nearby &b) { return a.depth < b.depth; });
    const size_t limit = static_cast<size_t>(r_show_labels->GetInt());
    const ScreenRect &rect = View();
    grtext_SetFont(SMALL_FONT);
    grtext_SetColor(kLabel);
    grtext_SetAlpha(220);
    size_t shown = 0; // only labels on screen count towards the limit
    for (size_t k = 0; k < found.size() && shown < limit; ++k) {
      const Nearby &n = found[k];
      if (n.at.x < rect.x1 || n.at.x > rect.x2 || n.at.y < rect.y1 || n.at.y > rect.y2)
        continue;
      ++shown;
      const object &obj = Objects[n.objnum];
      char text[96];
      std::snprintf(text, sizeof(text), "%s %s #%d h%X %.0f", TypeName(obj), ObjectName(obj), n.objnum,
                    static_cast<unsigned>(obj.handle), n.depth);
      grtext_Printf(static_cast<int>(n.at.x) + 4, static_cast<int>(n.at.y) + 2, "%s", text);
    }
    grtext_Flush();
  }
}

} // namespace

void DebugDraw_Init() {
  CvarRegistry &cv = Cvars();
  // Not archived: debugging aids start off every session. CHEAT: unlocked by HOURGLASS.
  r_showaxes = cv.RegisterBool("r_showaxes", false,
                               "World X (red), Y (green) and Z (blue) at the player (cheat: HOURGLASS)", CVAR_CHEAT);
  r_showobjinfo = cv.RegisterBool("r_showobjinfo", false,
                                  "Label nearby objects: type, name, number, handle, distance (cheat: HOURGLASS)",
                                  CVAR_CHEAT);
  r_showcollide = cv.RegisterBool("r_showcollide", false, "Draw nearby objects' collision spheres (cheat: HOURGLASS)",
                                  CVAR_CHEAT);
  r_showaxes_size =
      cv.RegisterFloat("r_showaxes_size", 10.0f, 1.0f, 200.0f, "Length of the r_showaxes lines, in units");
  r_showaxes_ahead = cv.RegisterFloat("r_showaxes_ahead", 40.0f, 0.0f, 200.0f,
                                      "In cockpit view, how far ahead of the ship r_showaxes draws, in units");
  r_show_range = cv.RegisterFloat("r_show_range", 300.0f, 10.0f, 2000.0f,
                                  "Distance out to which r_showobjinfo and r_showcollide draw, in units");
  r_show_labels = cv.RegisterInt("r_show_labels", 24, 1, 200, "Most r_showobjinfo labels, nearest first");
  for (Cvar *c : {r_showaxes, r_showobjinfo, r_showcollide})
    c->AddChangeCallback([](const Cvar &) { UpdateAny(); });
  UpdateAny();
}

void DebugDraw_Render() {
  if (!g_any) // AV-009: nothing else runs while every overlay is off
    return;
  if (IsClassic() || (Game_mode & GM_MULTI) || GetFunctionMode() != GAME_MODE || Player_object == nullptr ||
      Viewer_object == nullptr || Game_interface_mode != GAME_INTERFACE)
    return;
  if (!IsCheater)
    Cheats_MarkCheater(); // also after a new game clears it while an overlay is still on

  const object &viewer = *Viewer_object;
  const ViewCamera camera = MainCamera(viewer);
  float eye[3];
  ToArray(viewer.pos, eye);

  StartFrame(0, 0, Max_window_w, Max_window_h, false);
  rend_SetAlphaType(AT_ALWAYS);
  if (r_showaxes->GetBool())
    DrawAxes(camera, eye, viewer);
  if (r_showobjinfo->GetBool() || r_showcollide->GetBool())
    DrawObjects(camera, eye, viewer);
  EndFrame();
}

} // namespace tartarus
