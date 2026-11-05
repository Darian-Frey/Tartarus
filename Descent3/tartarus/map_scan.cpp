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
#include <string>
#include <vector>

#include "classic.h"
#include "console.h"
#include "cvar.h"
#include "findintersection.h"
#include "game.h"
#include "gamesequence.h"
#include "mouse_flight.h"
#include "object.h"
#include "room.h"
#include "scan_core.h"

namespace tartarus {

namespace {

Cvar *map_scan = nullptr;
Cvar *map_scan_range = nullptr;
Cvar *map_scan_rays = nullptr;
Cvar *map_scan_dim = nullptr;

constexpr int kChecksPerFrame = 400; // cheap range and facing tests, to find the next ray
constexpr int kRoomDepth = 2;        // rooms through this many portals; tunnels are many small rooms
constexpr float kMoveToRescan = 8.0f;

ScanTable g_table;
std::vector<int> g_rooms; // rooms in reach of the current one
int g_for_room = -1;
size_t g_room_cursor = 0;
int g_face_cursor = 0;
bool g_found_this_pass = false;
bool g_idle = false;   // a whole pass found nothing new: wait for the ship to move
vector g_idle_from{};

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

} // namespace

void MapScan_Init() {
  const uint32_t flags = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  CvarRegistry &cv = Cvars();
  map_scan = cv.RegisterBool("map_scan", true, "Laser scanner builds the automap as you fly", flags);
  map_scan_range = cv.RegisterFloat("map_scan_range", 200.0f, 50.0f, 1000.0f, "Scanner range", flags);
  map_scan_rays = cv.RegisterInt("map_scan_rays", 6, 1, 64, "Scanner rays a frame: more fills the map faster and costs more", flags);
  map_scan_dim = cv.RegisterFloat("map_scan_dim", 0.15f, 0.0f, 1.0f, "Automap brightness of surfaces seen but not scanned", flags);
  Console().AddCommand({"map_scan_status", "map_scan_status", "Show how many surfaces the scanner has reached", CMD_NONE,
                        [](const CommandArgs &, ConsoleCore &c) {
                          int total = 0;
                          for (int i = 0; i <= Highest_room_index; ++i)
                            total += Rooms[i].used ? Rooms[i].num_faces : 0;
                          c.Print("scanned " + std::to_string(g_table.Total()) + " of " + std::to_string(total) + " surfaces");
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

float MapScan_Brightness(int room, int face) {
  if (!Active())
    return 1.0f;
  return g_table.IsScanned(room, face) ? 1.0f : map_scan_dim->GetFloat();
}

} // namespace tartarus
