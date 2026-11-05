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

// The ship's laser scanner. See map_scan.h.

#include "map_scan.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include "3d.h"
#include "TelComAutoMap.h"
#include "classic.h"
#include "console.h"
#include "controls.h"
#include "deck.h"
#include "cvar.h"
#include "doorway.h"
#include "findintersection.h"
#include "grdefs.h"
#include "levelgoal.h"
#include "log.h"
#include "renderer.h"
#include "game.h"
#include "gamesequence.h"
#include "marker.h"
#include "Mission.h"
#include "mouse_flight.h"
#include "object.h"
#include "objinfo.h"
#include "orient_hud.h"
#include "player.h"
#include "room.h"
#include "scan_core.h"
#include "stringtable.h"
#include "terrain.h"
#include "ddio.h"

extern bool Skip_render_game_frame; // GameLoop.cpp

namespace tartarus {

namespace {

Cvar *map_scan = nullptr;
Cvar *map_scan_range = nullptr;
Cvar *map_scan_rays = nullptr;
Cvar *map_scan_dim = nullptr;
Cvar *map_scan_flash = nullptr;
Cvar *map_scan_camera = nullptr;
Cvar *map_show_keys = nullptr;
Cvar *map_show_doors = nullptr;
Cvar *map_show_goals = nullptr;
Cvar *map_show_powerups = nullptr;

constexpr int kChecksPerFrame = 400; // cheap range and facing tests, to find the next ray
constexpr int kRoomDepth = 2;        // rooms through this many portals; tunnels are many small rooms
constexpr float kMoveToRescan = 8.0f;
constexpr float kLineLift = 0.004f;

ScanTable g_table;
std::vector<int> g_rooms; // rooms in reach of the current one
int g_for_room = -1;
size_t g_room_cursor = 0;
int g_face_cursor = 0;
bool g_found_this_pass = false;
float g_flash_start = -100.0f;
float g_map_time = 0.0f;
vector g_eye{};

std::vector<float> g_line_xyz, g_line_rgba; // edges kept until every surface is drawn
bool g_idle = false;   // a whole pass found nothing new: wait for the ship to move
vector g_idle_from{};

std::filesystem::path ScanPath(std::filesystem::path save_path) {
  save_path.replace_extension(".amap");
  return save_path;
}

// A .amap only fits the level it was made in.
uint32_t LevelFingerprint() {
  uint32_t hash = 2166136261u;
  for (int i = 0; i <= Highest_room_index; ++i) {
    hash = (hash ^ static_cast<uint32_t>(Rooms[i].used ? Rooms[i].num_faces : 0)) * 16777619u;
  }
  return hash;
}

bool Active() { return !IsClassic() && !(Game_mode & GM_MULTI) && map_scan != nullptr && map_scan->GetBool(); }

void CollectRooms(int start) {
  g_rooms.assign(1, start);
  size_t from = 0;
  for (int depth = 0; depth < kRoomDepth; ++depth) {
    const size_t end = g_rooms.size();
    for (; from < end; ++from) {
      const room &rp = Rooms[g_rooms[from]];
      for (int i = 0; i < rp.num_portals; ++i) {
        const int next = rp.portals[i].croom;
        if (next >= 0 && std::find(g_rooms.begin(), g_rooms.end(), next) == g_rooms.end())
          g_rooms.push_back(next);
      }
    }
  }
  g_for_room = start;
  g_room_cursor = 0;
  g_face_cursor = 0;
  g_found_this_pass = false;
  g_idle = false;
}

void FaceCentre(const room &rp, const face &fp, vector *centre) {
  *centre = vector{};
  for (int i = 0; i < fp.num_verts; ++i)
    *centre += rp.verts[fp.face_verts[i]];
  *centre /= static_cast<float>(fp.num_verts);
}

// True if nothing solid lies between the ship and just in front of the surface.
bool RayReaches(vector *start, const vector &centre, const vector &normal) {
  vector end = centre + normal * 1.0f;
  fvi_query query{};
  fvi_info hit{};
  query.p0 = start;
  query.p1 = &end;
  query.startroom = Player_object->roomnum;
  query.rad = 0.0f;
  query.thisobjnum = OBJNUM(Player_object);
  query.ignore_obj_list = nullptr;
  query.flags = FQ_CHECK_OBJS | FQ_ONLY_DOOR_OBJ | FQ_NO_RELINK;
  return fvi_FindIntersection(&query, &hit) == HIT_NONE;
}

// An arrow along the ship's nose, with a stalk toward its top.
void DrawShipMarker() {
  const object &ship = *Player_object;
  const matrix &o = ship.orient;
  // A steady size on screen, however far back the camera is.
  const vector gap = ship.pos - g_eye;
  const float size = std::max(8.0f, std::sqrt(gap.x() * gap.x() + gap.y() * gap.y() + gap.z() * gap.z()) * 0.05f);
  const vector nose = ship.pos + o.fvec * (size * 1.6f);
  const vector tail = ship.pos - o.fvec * size;
  const vector left = tail - o.rvec * size, right = tail + o.rvec * size;
  const vector top = ship.pos + o.uvec * (size * 1.4f);
  // A wedge with a raised spine, so it reads from any side.
  const vector lines[] = {nose, left, left, right, right, nose, nose, top, left, top, right, top};
  std::vector<float> xyz, rgba;
  for (const vector &v : lines) {
    xyz.insert(xyz.end(), {v.x(), v.y(), v.z()});
    rgba.insert(rgba.end(), {1.0f, 0.85f, 0.4f, 1.0f});
  }
  rend_DrawLines3D(xyz.data(), rgba.data(), static_cast<int>(xyz.size() / 3));
}

struct SymbolColour {
  float r, g, b;
};

void AddSymbol(std::vector<float> *xyz, std::vector<float> *rgba, const matrix &view, const vector &at, int symbol,
               SymbolColour colour, float scale = 1.0f) {
  const vector gap = at - g_eye;
  const float distance = std::sqrt(gap.x() * gap.x() + gap.y() * gap.y() + gap.z() * gap.z());
  const float size = std::max(2.0f, distance * 0.018f) * scale; // a steady size on screen
  for (const SymbolLine &line : MapSymbolLines(symbol)) {
    const vector a = at + view.rvec * (line.x1 * size) + view.uvec * (line.y1 * size);
    const vector b = at + view.rvec * (line.x2 * size) + view.uvec * (line.y2 * size);
    xyz->insert(xyz->end(), {a.x(), a.y(), a.z(), b.x(), b.y(), b.z()});
    for (int i = 0; i < 2; ++i)
      rgba->insert(rgba->end(), {colour.r, colour.g, colour.b, 1.0f});
  }
}

// Scanned, or next to a room that is: doors are rooms the rays rarely land in.
bool RoomKnown(int room_index, bool or_neighbour) {
  if (room_index < 0 || room_index > Highest_room_index || !Rooms[room_index].used)
    return false;
  if (g_table.ScannedInRoom(room_index) > 0)
    return true;
  if (!or_neighbour)
    return false;
  const room &rp = Rooms[room_index];
  for (int i = 0; i < rp.num_portals; ++i) {
    if (g_table.ScannedInRoom(rp.portals[i].croom) > 0)
      return true;
  }
  return false;
}

void RoomCentre(const room &rp, vector *centre) {
  *centre = vector{};
  for (int i = 0; i < rp.num_verts; ++i)
    *centre += rp.verts[i];
  *centre /= static_cast<float>(std::max(rp.num_verts, 1));
}

void AddGoals(std::vector<float> *xyz, std::vector<float> *rgba, const matrix &view) {
  constexpr SymbolColour kGoal{0.3f, 1.0f, 1.0f};
  for (int goal = 0; goal < Level_goals.GetNumGoals(); ++goal) {
    int status = 0;
    Level_goals.GoalStatus(goal, LO_GET_SPECIFIED, &status, false);
    if (!(status & LGF_ENABLED) || (status & (LGF_COMPLETED | LGF_FAILED | LGF_NOT_LOC_BASED)))
      continue;
    for (int item = 0; item < Level_goals.GoalGetNumItems(goal); ++item) {
      char type = 0;
      int handle = 0;
      bool done = false;
      Level_goals.GoalItemInfo(goal, item, LO_GET_SPECIFIED, &type, &handle, &done);
      if (done)
        continue;
      if (type == LIT_OBJECT) {
        const object *objp = ObjGet(handle);
        if (objp != nullptr && !ROOMNUM_OUTSIDE(objp->roomnum) && RoomKnown(objp->roomnum, true))
          AddSymbol(xyz, rgba, view, objp->pos, kSymbolGoal, kGoal);
      } else if (type == LIT_INTERNAL_ROOM && RoomKnown(handle, false)) {
        vector centre;
        RoomCentre(Rooms[handle], &centre);
        AddSymbol(xyz, rgba, view, centre, kSymbolGoal, kGoal);
      }
    }
  }
}

void DrawSymbols(const matrix &view) {
  constexpr SymbolColour kKey{1.0f, 0.9f, 0.2f}, kDoor{1.0f, 0.65f, 0.2f}, kLocked{1.0f, 0.25f, 0.2f};
  constexpr SymbolColour kPowerup{0.55f, 0.75f, 1.0f}, kMarker{1.0f, 0.35f, 1.0f}, kChosen{0.3f, 1.0f, 1.0f};
  std::vector<float> xyz, rgba;
  const bool keys = map_show_keys->GetBool(), doors = map_show_doors->GetBool(), powerups = map_show_powerups->GetBool();
  for (int i = 0; i <= Highest_object_index; ++i) {
    const object &obj = Objects[i];
    if (obj.type == OBJ_MARKER && obj.parent_handle == Player_object->handle) {
      AddSymbol(&xyz, &rgba, view, obj.pos, kSymbolMarker, kMarker);
      if (AM_current_marker == obj.id)
        AddSymbol(&xyz, &rgba, view, obj.pos, kSymbolRing, kChosen);
      continue;
    }
    if (ROOMNUM_OUTSIDE(obj.roomnum))
      continue;
    if (obj.type == OBJ_DOOR && doors && RoomKnown(obj.roomnum, true)) {
      const bool locked = DoorwayLocked(obj.handle);
      AddSymbol(&xyz, &rgba, view, obj.pos, locked ? kSymbolLockedDoor : kSymbolDoor, locked ? kLocked : kDoor);
    } else if (obj.type == OBJ_POWERUP && RoomKnown(obj.roomnum, false) && !IsHiddenName(Object_info[obj.id].name)) {
      const bool key = IsKeyName(Object_info[obj.id].name);
      if (key && keys)
        AddSymbol(&xyz, &rgba, view, obj.pos, kSymbolKey, kKey);
      else if (!key && powerups)
        AddSymbol(&xyz, &rgba, view, obj.pos, kSymbolPowerup, kPowerup, 0.7f);
    }
  }
  if (map_show_goals->GetBool())
    AddGoals(&xyz, &rgba, view);
  if (!xyz.empty())
    rend_DrawLines3D(xyz.data(), rgba.data(), static_cast<int>(xyz.size() / 3));
}

bool MapFrame(int left, int top, int right, int bottom) {
  const float now = timer_GetTime();
  const float frametime = std::clamp(now - g_map_time, 0.001f, 0.1f);
  g_map_time = now;
  const bool close = TCAMDeckFrame(left, top, right, bottom, frametime);
  OrientHud_DrawCompass(TCAMDeckHeading(), left, top, right);
  return close;
}

std::vector<std::string> MapLines() {
  std::vector<std::string> lines = {Level_info.name};
  if (AM_current_marker >= 0) {
    char label[64];
    std::snprintf(label, sizeof(label), TXT_VIEW_MARKER, AM_current_marker + 1);
    lines.push_back(std::string(label) + ": " + MarkerMessages[AM_current_marker]);
  }
  return lines;
}

void MapKey(int key) {
  switch (key & 0xFF) {
  case KEY_F1:
    AM_terrain = !AM_terrain;
    break;
  case KEY_F2:
    AM_center_on_player = 1;
    break;
  case KEY_F3:
    AM_realign = 1;
    break;
  case KEY_F5:
    Cvars().Set("map_show_keys", map_show_keys->GetBool() ? "0" : "1", CvarSource::User);
    break;
  case KEY_F6:
    Cvars().Set("map_show_doors", map_show_doors->GetBool() ? "0" : "1", CvarSource::User);
    break;
  case KEY_F7:
    Cvars().Set("map_show_goals", map_show_goals->GetBool() ? "0" : "1", CvarSource::User);
    break;
  case KEY_F8:
    Cvars().Set("map_show_powerups", map_show_powerups->GetBool() ? "0" : "1", CvarSource::User);
    break;
  default:
    if ((key & 0xFF) >= KEY_1 && (key & 0xFF) <= KEY_8) {
      const int index = (key & 0xFF) - KEY_1;
      if (index < Players[Player_num].num_markers)
        AM_current_marker = index;
    }
    break;
  }
}

} // namespace

void MapScan_Init() {
  const uint32_t flags = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  CvarRegistry &cv = Cvars();
  map_scan = cv.RegisterBool("map_scan", true, "Laser scanner builds the automap as you fly", flags);
  map_scan_range = cv.RegisterFloat("map_scan_range", 200.0f, 50.0f, 1000.0f, "Scanner range", flags);
  map_scan_rays = cv.RegisterInt("map_scan_rays", 6, 1, 64, "Scanner rays a frame: more fills the map faster and costs more", flags);
  map_scan_dim = cv.RegisterFloat("map_scan_dim", 0.0f, 0.0f, 1.0f, "Automap brightness of surfaces not scanned; 0 leaves them blank", flags);
  map_scan_flash = cv.RegisterFloat("map_scan_flash", 2.5f, 0.1f, 10.0f, "Seconds new surfaces flash when the map opens", flags);
  map_scan_camera = cv.RegisterFloat("map_scan_camera", 60.0f, 10.0f, 300.0f, "How far behind the ship the map opens", flags);
  map_show_keys = cv.RegisterBool("map_show_keys", true, "Automap: keys you have scanned", flags);
  map_show_doors = cv.RegisterBool("map_show_doors", true, "Automap: doors you have scanned", flags);
  map_show_goals = cv.RegisterBool("map_show_goals", true, "Automap: goals in rooms you have scanned", flags);
  map_show_powerups = cv.RegisterBool("map_show_powerups", false, "Automap: powerups you have scanned", flags);
  Console().AddCommand({"map_scan_status", "map_scan_status", "Show how many surfaces the scanner has reached", CMD_NONE,
                        [](const CommandArgs &, ConsoleCore &c) {
                          int total = 0;
                          for (int i = 0; i <= Highest_room_index; ++i)
                            total += Rooms[i].used ? Rooms[i].num_faces : 0;
                          const std::string text = "scanned " + std::to_string(g_table.Total()) + " of " + std::to_string(total) + " surfaces";
                          c.Print(text);
                          LOG_INFO << "[tartarus] " << text;
                        }});
}

void MapScan_Reset() {
  g_table.Clear();
  g_rooms.clear();
  g_for_room = -1;
}

void MapScan_Frame() {
  if (!Active() || !Flight_ContextActive() || Player_object == nullptr || ROOMNUM_OUTSIDE(Player_object->roomnum))
    return;
  if (Player_object->roomnum != g_for_room)
    CollectRooms(Player_object->roomnum);

  vector ship = Player_object->pos;
  if (g_idle) {
    const vector moved = ship - g_idle_from;
    if (moved.x() * moved.x() + moved.y() * moved.y() + moved.z() * moved.z() < kMoveToRescan * kMoveToRescan)
      return;
    g_idle = false;
  }
  const float from[3] = {ship.x(), ship.y(), ship.z()};
  const float range = map_scan_range->GetFloat();
  int rays = map_scan_rays->GetInt();
  for (int checks = 0; checks < kChecksPerFrame && rays > 0; ++checks) {
    const int room_index = g_rooms[g_room_cursor];
    const room &rp = Rooms[room_index];
    if (g_face_cursor >= rp.num_faces) {
      g_face_cursor = 0;
      g_room_cursor = (g_room_cursor + 1) % g_rooms.size();
      if (g_room_cursor == 0) { // a whole pass
        if (!g_found_this_pass) {
          g_idle = true;
          g_idle_from = ship;
          break;
        }
        g_found_this_pass = false;
      }
      continue;
    }
    const int face_index = g_face_cursor++;
    const face &fp = rp.faces[face_index];
    if (fp.portal_num != -1 || g_table.IsScanned(room_index, face_index))
      continue;
    vector centre;
    FaceCentre(rp, fp, &centre);
    const float c[3] = {centre.x(), centre.y(), centre.z()};
    const float n[3] = {fp.normal.x(), fp.normal.y(), fp.normal.z()};
    if (!FaceInReach(c, n, from, range))
      continue;
    --rays;
    if (RayReaches(&ship, centre, fp.normal) && g_table.Mark(room_index, face_index))
      g_found_this_pass = true;
  }
}

void MapScan_RevealRoom(int room_index) {
  if (room_index < 0 || room_index > Highest_room_index || !Rooms[room_index].used)
    return;
  for (int i = 0; i < Rooms[room_index].num_faces; ++i)
    g_table.Mark(room_index, i);
}

void MapScan_Save(const std::filesystem::path &save_path) {
  const std::filesystem::path path = ScanPath(save_path);
  if (!Active()) {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    return;
  }
  const std::vector<uint8_t> bytes = g_table.Serialize(LevelFingerprint());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void MapScan_Load(const std::filesystem::path &save_path) {
  MapScan_Reset();
  if (!Active())
    return;
  std::ifstream file(ScanPath(save_path), std::ios::binary);
  if (!file)
    return; // saved before the scanner, or in Classic Mode: nothing scanned yet
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (!g_table.Deserialize(bytes, LevelFingerprint()))
    LOG_WARNING << "Scanner: ignoring a .amap file that is damaged or from another level";
}

void MapScan_MapOpened() {
  g_table.OpenMap();
  g_flash_start = timer_GetTime();
}

bool MapScan_DrawFace(int room_index, int face_index, g3Point *points, const vector &eye) {
  const room &rp = Rooms[room_index];
  if (!Active())
    return false;
  g_eye = eye;
  const face &fp = rp.faces[face_index];
  // Buildings are only drawn with the terrain, which is not scanned: draw them with it.
  const bool outdoors = (rp.flags & RF_EXTERNAL) != 0;
  const bool scanned = outdoors || g_table.IsScanned(room_index, face_index);
  if (!scanned && map_scan_dim->GetFloat() <= 0.0f)
    return true; // nothing known here: blank

  // Near-black, so nearer walls hide further ones.
  g3Point buffer[MAX_VERTS_PER_FACE];
  g3Point *list[MAX_VERTS_PER_FACE];
  for (int i = 0; i < fp.num_verts; ++i) {
    buffer[i] = points[fp.face_verts[i]];
    buffer[i].p3_flags |= PF_RGBA;
    buffer[i].p3_r = 0.0f;
    buffer[i].p3_g = scanned ? 0.04f : 0.0f;
    buffer[i].p3_b = scanned ? 0.03f : 0.0f;
    buffer[i].p3_a = 1.0f;
    list[i] = &buffer[i];
  }
  g3_DrawPoly(fp.num_verts, list, 0);

  float flash = 0.0f;
  if (scanned && g_table.IsNew(room_index, face_index))
    flash = std::max(0.0f, 1.0f - (timer_GetTime() - g_flash_start) / map_scan_flash->GetFloat());
  const float level = outdoors ? 0.5f : scanned ? 1.0f : map_scan_dim->GetFloat();
  const float colour[4] = {(0.59f + 0.41f * flash) * level, level, (0.75f + 0.25f * flash) * level, 1.0f};
  for (int i = 0; i < fp.num_verts; ++i) {
    for (int end = 0; end < 2; ++end) {
      const vector &v = rp.verts[fp.face_verts[(i + end) % fp.num_verts]];
      // A little toward the eye, or lines fight the wall they lie on for depth.
      const vector near_eye = v + (eye - v) * kLineLift;
      g_line_xyz.insert(g_line_xyz.end(), {near_eye.x(), near_eye.y(), near_eye.z()});
      g_line_rgba.insert(g_line_rgba.end(), colour, colour + 4);
    }
  }
  return true;
}

void MapScan_FinishRooms(const matrix &view) {
  if (!g_line_xyz.empty()) {
    rend_DrawLines3D(g_line_xyz.data(), g_line_rgba.data(), static_cast<int>(g_line_xyz.size() / 3));
    g_line_xyz.clear();
    g_line_rgba.clear();
  }
  if (MapScan_DrawsShip()) {
    rend_SetZBufferState(0); // always visible, even through walls
    DrawShipMarker();
    DrawSymbols(view);
    rend_SetZBufferState(1);
  }
}

bool MapScan_DrawTerrain(const vector &eye) {
  if (!Active())
    return false;
  // A grid around the camera, every few cells.
  constexpr int kStep = 4;
  constexpr int kReachCells = 96;
  const int cx = static_cast<int>(eye.x() / TERRAIN_SIZE), cz = static_cast<int>(eye.z() / TERRAIN_SIZE);
  std::vector<float> xyz, rgba;
  auto height = [](int x, int z) { return Terrain_seg[z * TERRAIN_WIDTH + x].y; };
  auto line = [&](int x1, int z1, int x2, int z2) {
    xyz.insert(xyz.end(), {x1 * static_cast<float>(TERRAIN_SIZE), height(x1, z1), z1 * static_cast<float>(TERRAIN_SIZE),
                           x2 * static_cast<float>(TERRAIN_SIZE), height(x2, z2), z2 * static_cast<float>(TERRAIN_SIZE)});
    for (int i = 0; i < 2; ++i)
      rgba.insert(rgba.end(), {0.2f, 0.42f, 0.3f, 1.0f});
  };
  const int x1 = std::max(0, (cx - kReachCells) / kStep * kStep), x2 = std::min(TERRAIN_WIDTH - 1 - kStep, cx + kReachCells);
  const int z1 = std::max(0, (cz - kReachCells) / kStep * kStep), z2 = std::min(TERRAIN_DEPTH - 1 - kStep, cz + kReachCells);
  for (int z = z1; z <= z2; z += kStep) {
    for (int x = x1; x <= x2; x += kStep) {
      line(x, z, x + kStep, z);
      line(x, z, x, z + kStep);
    }
  }
  if (!xyz.empty())
    rend_DrawLines3D(xyz.data(), rgba.data(), static_cast<int>(xyz.size() / 3));
  return true;
}

bool MapScan_DrawsShip() { return Active() && Player_object != nullptr; }

bool MapScan_HidesOpenings() { return Active() && map_scan_dim->GetFloat() <= 0.0f; }

float MapScan_CameraBack(float stock) { return Active() ? map_scan_camera->GetFloat() : stock; }

bool MapScan_RunMap() {
  if (!Active())
    return false;
  const int old_mode = GetScreenMode();
  Skip_render_game_frame = true;
  SetScreenMode(SM_MENU);
  ResumeControls();
  TCAMDeckBegin();
  g_map_time = timer_GetTime();
  Deck_RunView(MapLines, "F1 TERRAIN  F2 CENTRE  F3 LEVEL  F5 KEYS  F6 DOORS  F7 GOALS  F8 POWERUPS  1-8 MARKERS  ESC CLOSE", MapFrame, MapKey);
  TCAMDeckEnd();
  SuspendControls();
  ddio_KeyFlush();
  SetScreenMode(old_mode);
  rend_SetZBufferState(1);
  rend_SetWrapType(WT_WRAP);
  Skip_render_game_frame = false;
  OrientHud_Reanchor();
  return true;
}

ddgr_color MapScan_Background(ddgr_color stock) { return Active() ? GR_BLACK : stock; }

} // namespace tartarus
