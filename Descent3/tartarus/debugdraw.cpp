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
#include <cstdlib>
#include <vector>

#include "AIMain.h"
#include "aistruct.h"
#include "cheats.h"
#include "classic.h"
#include "console.h"
#include "cvar.h"
#include "debug_core.h"
#include "findintersection.h"
#include "game.h"
#include "gamefont.h"
#include "gameloop.h"
#include "gamepath.h"
#include "gamesequence.h"
#include "grtext.h"
#include "log.h"
#include "object.h"
#include "objinfo.h"
#include "player.h"
#include "renderer.h"
#include "room.h"
#include "terrain.h"
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
Cvar *r_showpath = nullptr;

bool g_any = false; // any overlay on: the only test made each frame while they are all off

void UpdateAny() {
  g_any = r_showaxes->GetBool() || r_showobjinfo->GetBool() || r_showcollide->GetBool() || r_showpath->GetInt() > 0;
}

const ddgr_color kAxis[3] = {GR_RGB(255, 80, 80), GR_RGB(80, 255, 80), GR_RGB(90, 140, 255)};
const ddgr_color kRobot = GR_RGB(255, 210, 60);
const ddgr_color kOther = GR_RGB(90, 230, 230);
const ddgr_color kLabel = GR_RGB(235, 235, 235);
const ddgr_color kPathInside = GR_RGB(255, 80, 255);  // a leg between two rooms of the mine
const ddgr_color kPathOutside = GR_RGB(255, 150, 40); // a leg that touches the terrain
const ddgr_color kPathNext = GR_RGB(255, 255, 255);   // from the Guide-Bot to its next node
const ddgr_color kPathBlocked = GR_RGB(255, 40, 40);  // a leg that runs into a wall or the ground

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

// ---- The Guide-Bot's planned route (r_showpath) -------------------------------

struct PathNode {
  vector pos;
  int room; // a room number, or a terrain cell with ROOMNUM_CELLNUM_FLAG
};

object *GuideBot() {
  if (Player_num < 0 || Player_num >= MAX_PLAYERS)
    return nullptr;
  object *bot = ObjGet(Buddy_handle[Player_num]);
  return bot != nullptr && bot->ai_info != nullptr ? bot : nullptr;
}

/// The route as the AI holds it: dynamic paths (built by AIPathAllocPath) and
/// static level paths, joined in order. *current is the node it is flying to.
std::vector<PathNode> RouteOf(const object &bot, int *current) {
  std::vector<PathNode> nodes;
  *current = -1;
  const ai_path_info &aip = bot.ai_info->path;
  for (int p = 0; p < aip.num_paths && p < MAX_JOINED_PATHS; ++p) {
    const int id = aip.path_id[p];
    if (aip.path_type[p] == AIP_DYNAMIC && id < MAX_DYNAMIC_PATHS) {
      const ai_dynamic_path &dp = AIDynamicPath[id];
      for (int n = 0; n < dp.num_nodes && n < MAX_NODES; ++n) {
        if (p == aip.cur_path && n == aip.cur_node)
          *current = static_cast<int>(nodes.size());
        nodes.push_back({dp.pos[n], dp.roomnum[n]});
      }
    } else if (aip.path_type[p] == AIP_STATIC && id < MAX_GAME_PATHS && GamePaths[id].used) {
      const game_path &gp = GamePaths[id];
      const int start = aip.path_start_node[p], end = aip.path_end_node[p];
      const int step = start <= end ? 1 : -1;
      for (int n = start; n >= 0 && n < gp.num_nodes; n += step) {
        if (p == aip.cur_path && n == aip.cur_node)
          *current = static_cast<int>(nodes.size());
        nodes.push_back({gp.pathnodes[n].pos, gp.pathnodes[n].roomnum});
        if (n == end)
          break;
      }
    }
  }
  return nodes;
}

/// True if a straight flight from a (in room) to b hits a wall, the terrain or a closed door.
bool LegBlocked(vector a, int room, vector b) {
  fvi_query query{};
  fvi_info hit{};
  query.p0 = &a;
  query.p1 = &b;
  query.startroom = room;
  query.rad = 0.0f;
  query.thisobjnum = -1;
  query.ignore_obj_list = nullptr;
  query.flags = FQ_CHECK_OBJS | FQ_ONLY_DOOR_OBJ | FQ_NO_RELINK;
  return fvi_FindIntersection(&query, &hit) != HIT_NONE;
}

void RoomLabel(int room, char *text, size_t size) {
  if (ROOMNUM_OUTSIDE(room))
    std::snprintf(text, size, "T%d", TERRAIN_REGION(room)); // terrain, by region
  else
    std::snprintf(text, size, "R%d", room);
}

void DrawRoute(const ViewCamera &camera, const float eye[3]) {
  const object *bot = GuideBot();
  if (bot == nullptr)
    return;
  int current = -1;
  const std::vector<PathNode> nodes = RouteOf(*bot, &current);
  float a[3], b[3];
  for (size_t i = 0; i + 1 < nodes.size(); ++i) {
    ToArray(nodes[i].pos, a);
    ToArray(nodes[i + 1].pos, b);
    const bool outside = ROOMNUM_OUTSIDE(nodes[i].room) || ROOMNUM_OUTSIDE(nodes[i + 1].room);
    const bool blocked = LegBlocked(nodes[i].pos, nodes[i].room, nodes[i + 1].pos);
    WorldLine(camera, eye, a, b, blocked ? kPathBlocked : outside ? kPathOutside : kPathInside);
  }
  if (current >= 0) {
    const PathNode &next = nodes[static_cast<size_t>(current)];
    ToArray(bot->pos, a);
    ToArray(next.pos, b);
    WorldLine(camera, eye, a, b, LegBlocked(bot->pos, bot->roomnum, next.pos) ? kPathBlocked : kPathNext);
  }
  const bool labels = r_showpath->GetInt() >= 2;
  if (labels) {
    grtext_SetFont(SMALL_FONT);
    grtext_SetColor(kLabel);
    grtext_SetAlpha(220);
  }
  for (size_t i = 0; i < nodes.size(); ++i) {
    ToArray(nodes[i].pos, a);
    Point2 at;
    if (!ProjectPoint(camera, eye, a, &at, nullptr))
      continue;
    const int half = static_cast<int>(i) == current ? 5 : 2;
    const ddgr_color colour = ROOMNUM_OUTSIDE(nodes[i].room) ? kPathOutside : kPathInside;
    Line({at.x - half, at.y - half}, {at.x + half, at.y - half}, colour);
    Line({at.x + half, at.y - half}, {at.x + half, at.y + half}, colour);
    Line({at.x + half, at.y + half}, {at.x - half, at.y + half}, colour);
    Line({at.x - half, at.y + half}, {at.x - half, at.y - half}, colour);
    if (labels) {
      char text[24];
      RoomLabel(nodes[i].room, text, sizeof(text));
      grtext_Printf(static_cast<int>(at.x) + 6, static_cast<int>(at.y) - 6, "%d %s", static_cast<int>(i), text);
    }
  }
  if (labels)
    grtext_Flush();
}

/// pathdump: the Guide-Bot's route as text, to the console and the log.
void DumpRoute(ConsoleCore &c) {
  const object *bot = GuideBot();
  if (GetFunctionMode() != GAME_MODE || bot == nullptr) {
    c.Print("pathdump: no Guide-Bot out in this level");
    return;
  }
  int current = -1;
  const std::vector<PathNode> nodes = RouteOf(*bot, &current);
  char line[160], room[24];
  RoomLabel(bot->roomnum, room, sizeof(room));
  const bool to_next_blocked =
      current >= 0 && LegBlocked(bot->pos, bot->roomnum, nodes[static_cast<size_t>(current)].pos);
  std::snprintf(line, sizeof(line), "Guide-Bot in %s at %.0f %.0f %.0f; %d nodes, next %d%s", room, bot->pos.x(),
                bot->pos.y(), bot->pos.z(), static_cast<int>(nodes.size()), current,
                to_next_blocked ? "; BLOCKED from here to next" : "");
  c.Print(line);
  LOG_INFO << "[tartarus] pathdump: " << line;
  if (ROOMNUM_OUTSIDE(bot->roomnum) && current >= 0) {
    // Over the terrain: how high the ground is under the bot and under its next node.
    vector under_bot = bot->pos, under_next = nodes[static_cast<size_t>(current)].pos;
    std::snprintf(line, sizeof(line), "ground under the Guide-Bot %.0f, under its next node %.0f",
                  GetTerrainGroundPoint(&under_bot), GetTerrainGroundPoint(&under_next));
    c.Print(line);
    LOG_INFO << "[tartarus] pathdump: " << line;
  }
  for (size_t i = 0; i < nodes.size(); ++i) {
    RoomLabel(nodes[i].room, room, sizeof(room));
    const bool blocked = i + 1 < nodes.size() && LegBlocked(nodes[i].pos, nodes[i].room, nodes[i + 1].pos);
    std::snprintf(line, sizeof(line), "%2d %-6s %.0f %.0f %.0f%s", static_cast<int>(i), room, nodes[i].pos.x(),
                  nodes[i].pos.y(), nodes[i].pos.z(), blocked ? "  BLOCKED to next" : "");
    c.Print(line);
    LOG_INFO << "[tartarus] pathdump: " << line;
  }
}

/// roominfo <room>: a room's openings, for working out routes (F-072).
void RoomInfo(const CommandArgs &a, ConsoleCore &c) {
  if (GetFunctionMode() != GAME_MODE || a.size() < 2) {
    c.Print("usage: roominfo <room>, in a level");
    return;
  }
  const int r = std::atoi(a[1].c_str());
  if (r < 0 || r > Highest_room_index || !Rooms[r].used) {
    c.Print("roominfo: no such room");
    return;
  }
  const room &rm = Rooms[r];
  char line[200];
  std::snprintf(line, sizeof(line), "R%d%s: %d openings, path point %.0f %.0f %.0f", r,
                (rm.flags & RF_EXTERNAL) ? " (external)" : "", rm.num_portals, rm.path_pnt.x(), rm.path_pnt.y(),
                rm.path_pnt.z());
  c.Print(line);
  LOG_INFO << "[tartarus] roominfo: " << line;
  for (int i = 0; i < rm.num_portals; ++i) {
    const portal &p = rm.portals[i];
    vector pnt = p.path_pnt;
    const bool to_external = p.croom >= 0 && (Rooms[p.croom].flags & RF_EXTERNAL);
    char where[64] = "";
    const int cell = GetTerrainCellFromPos(&pnt);
    if (cell >= 0)
      std::snprintf(where, sizeof(where), ", over terrain region %d, ground %.0f", TERRAIN_REGION(cell),
                    GetTerrainGroundPoint(&pnt));
    std::snprintf(line, sizeof(line), "  %d -> R%d%s at %.0f %.0f %.0f%s", i, p.croom,
                  to_external ? " (external)" : "", pnt.x(), pnt.y(), pnt.z(), where);
    c.Print(line);
    LOG_INFO << "[tartarus] roominfo: " << line;
  }
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
  r_showpath = cv.RegisterInt("r_showpath", 0, 0, 2,
                              "The Guide-Bot's planned route: 1 legs and nodes, 2 with room numbers (cheat: HOURGLASS)",
                              CVAR_CHEAT);
  Console().AddCommand({"pathdump", "pathdump", "List the Guide-Bot's planned route (also logged)", CMD_NONE,
                        [](const CommandArgs &, ConsoleCore &c) { DumpRoute(c); }});
  Console().AddCommand({"roominfo", "roominfo <room>", "List a room's openings: where each leads (also logged)",
                        CMD_NONE, [](const CommandArgs &a, ConsoleCore &c) { RoomInfo(a, c); }});
  for (Cvar *c : {r_showaxes, r_showobjinfo, r_showcollide, r_showpath})
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
  if (r_showpath->GetInt() > 0)
    DrawRoute(camera, eye);
  EndFrame();
}

} // namespace tartarus
