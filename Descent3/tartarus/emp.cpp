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

#include "emp.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "ai_react.h"
#include "cheats.h"
#include "classic.h"
#include "console.h"
#include "cvar.h"
#include "findintersection.h"
#include "fireball.h"
#include "game.h"
#include "gravwell.h"
#include "hlsoundlib.h"
#include "log.h"
#include "object.h"
#include "objinfo.h"
#include "player.h"
#include "sidecar_core.h"
#include "sounds.h"
#include "viseffect.h"
#include "weapon.h"
#include "weapons_sidecar.h"

namespace tartarus {

namespace {

Cvar *emp_radius = nullptr;
Cvar *emp_time = nullptr;
Cvar *emp_trigger_radius = nullptr;
Cvar *emp_arm_time = nullptr;
Cvar *emp_immune_mass = nullptr;

struct Mine {
  int handle;
  float armed_at; // Gametime
};

std::vector<Mine> g_mines;

bool Usable() { return !IsClassic() && !(Game_mode & GM_MULTI); }

bool IsEmp(const object &o) {
  if (o.type != OBJ_WEAPON)
    return false;
  const SidecarWeapon *side = WeaponSidecar_ByIndex(o.id);
  return side != nullptr && (side->flags & TWF_EMP);
}

// Nothing solid (wall, terrain, closed door) between a and b.
bool InSight(vector a, int room, vector b) {
  fvi_query query{};
  fvi_info hit{};
  query.p0 = &a;
  query.p1 = &b;
  query.startroom = room;
  query.rad = 0.0f;
  query.thisobjnum = -1;
  query.ignore_obj_list = nullptr;
  query.flags = FQ_CHECK_OBJS | FQ_ONLY_DOOR_OBJ | FQ_NO_RELINK;
  return fvi_FindIntersection(&query, &hit) == HIT_NONE;
}

bool Stunnable(const object &o) {
  if (o.type != OBJ_ROBOT || o.control_type != CT_AI || (o.flags & OF_DEAD) || IS_GUIDEBOT(&o))
    return false;
  const float immune = emp_immune_mass->GetFloat();
  return !(immune > 0.0f && o.mtype.phys_info.mass >= immune);
}

void Strand(vector centre, int room, const object &o, float dist) {
  const int visnum = VisEffectCreate(VIS_FIREBALL, LIGHTNING_BOLT_INDEX, room, &centre);
  if (visnum < 0)
    return;
  vis_effect *vis = &VisEffects[visnum];
  vis->lifeleft = vis->lifetime = 0.4f;
  vis->end_pos = o.pos;
  vis->custom_handle = static_cast<int>(&o - Objects);
  vis->flags = VF_USES_LIFELEFT | VF_NO_Z_ADJUST;
  vis->size = std::max(1.0f, dist);
  vis->velocity.x() = 1;
  vis->velocity.y() = 0.6f;
}

void Pulse(vector centre, int room) {
  const float radius = emp_radius->GetFloat(), seconds = emp_time->GetFloat();
  int stunned = 0;
  for (int i = 0; i <= Highest_object_index; ++i) {
    object &o = Objects[i];
    if (!Stunnable(o))
      continue;
    const float dist = Gravwell_SurfaceDistance(o, centre);
    if (dist > radius || !InSight(centre, room, o.pos))
      continue;
    if (AiReact_Stun(&o, seconds)) {
      Strand(centre, room, o, std::max(0.0f, dist));
      ++stunned;
    }
  }
  CreateBlastRing(&centre, BLAST_RING_INDEX, 0.6f, radius, room, 1);
  CreateRandomSparks(16, &centre, room, -1, 2);
  pos_state where{};
  matrix orient = IDENTITY_MATRIX;
  where.position = &centre;
  where.orient = &orient;
  where.roomnum = room;
  Sound_system.Play3dSound(SOUND_LIGHTNING, SND_PRIORITY_HIGHEST, &where);
  LOG_INFO.printf("[tartarus] EMP in room %d: %d robot(s) stunned", room, stunned);
}

void Forget(int handle) {
  g_mines.erase(std::remove_if(g_mines.begin(), g_mines.end(), [handle](const Mine &m) { return m.handle == handle; }),
                g_mines.end());
}

} // namespace

void Emp_Init() {
  const uint32_t flags = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  CvarRegistry &cv = Cvars();
  emp_radius = cv.RegisterFloat("emp_radius", 60.0f, 5.0f, 400.0f, "EMP mine: robots this close (to their surface) are stunned", flags);
  emp_time = cv.RegisterFloat("emp_time", 4.0f, 0.1f, 30.0f, "EMP mine: seconds a robot stays stunned", flags);
  emp_trigger_radius = cv.RegisterFloat("emp_trigger_radius", 25.0f, 1.0f, 200.0f, "EMP mine: goes off when a robot comes this close", flags);
  emp_arm_time = cv.RegisterFloat("emp_arm_time", 0.75f, 0.0f, 10.0f, "EMP mine: seconds after dropping before it can go off", flags);
  emp_immune_mass = cv.RegisterFloat("emp_immune_mass", 40.0f, 0.0f, 1000.0f, "EMP mine: robots this heavy or more (bosses) are not stunned; 0 none", flags);
  Console().AddCommand({"emp", "emp", "Send an EMP pulse from your ship (test; HOURGLASS)", CMD_CHEAT,
                        [](const CommandArgs &, ConsoleCore &c) {
                          if (!Usable() || Player_object == nullptr) {
                            c.Print("emp: needs a single-player level in Tartarus mode");
                            return;
                          }
                          Cheats_MarkCheater();
                          Pulse(Player_object->pos, Player_object->roomnum);
                        }});
}

void Emp_MineDropped(const object *mine) {
  if (!Usable() || mine == nullptr)
    return;
  g_mines.push_back({mine->handle, Gametime + emp_arm_time->GetFloat()});
}

void Emp_Frame() {
  if (g_mines.empty())
    return;
  if (!Usable()) {
    g_mines.clear();
    return;
  }
  const float trigger = emp_trigger_radius->GetFloat();
  std::vector<int> fire;
  for (const Mine &m : g_mines) {
    object *mine = ObjGet(m.handle);
    if (mine == nullptr || mine->type != OBJ_WEAPON || (mine->flags & OF_DEAD)) {
      fire.push_back(-m.handle - 1); // gone: forget it
      continue;
    }
    if (Gametime < m.armed_at)
      continue;
    for (int i = 0; i <= Highest_object_index; ++i) {
      const object &o = Objects[i];
      if (Stunnable(o) && Gravwell_SurfaceDistance(o, mine->pos) <= trigger && InSight(mine->pos, mine->roomnum, o.pos)) {
        fire.push_back(m.handle);
        break;
      }
    }
  }
  for (int h : fire) {
    if (h < 0) {
      Forget(-h - 1);
      continue;
    }
    object *mine = ObjGet(h);
    Forget(h);
    if (mine == nullptr)
      continue;
    Pulse(mine->pos, mine->roomnum);
    SetObjectDeadFlag(mine); // gone without the base weapon's blast
  }
}

void Emp_WeaponExploded(const object *weapon_obj, const vector *point) {
  if (!Usable() || weapon_obj == nullptr || !IsEmp(*weapon_obj))
    return;
  Forget(weapon_obj->handle);
  Pulse(point != nullptr ? *point : weapon_obj->pos, weapon_obj->roomnum);
}

void Emp_Reset() {
  g_mines.clear();
  if (!Usable())
    return;
  // Mines lying in a loaded level are objects in the save: watch them again, armed.
  for (int i = 0; i <= Highest_object_index; ++i)
    if (IsEmp(Objects[i]) && !(Objects[i].flags & OF_DEAD))
      g_mines.push_back({Objects[i].handle, Gametime});
}

} // namespace tartarus
