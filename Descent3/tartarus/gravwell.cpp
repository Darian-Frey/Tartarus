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

#include "gravwell.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "classic.h"
#include "cvar.h"
#include "damage.h"
#include "fireball.h"
#include "game.h"
#include "gravwell_core.h"
#include "log.h"
#include "object.h"
#include "objinfo.h"
#include "physics.h"
#include "player.h"
#include "polymodel.h"
#include "sidecar_core.h"
#include "viseffect.h"
#include "weapon.h"
#include "weapons_sidecar.h"

namespace tartarus {

namespace {

Cvar *gw_radius = nullptr;
Cvar *gw_time = nullptr;
Cvar *gw_force = nullptr;
Cvar *gw_mass_ref = nullptr;
Cvar *gw_mass_min_scale = nullptr;
Cvar *gw_immune_mass = nullptr;
Cvar *gw_self = nullptr;
Cvar *gw_self_radius = nullptr;
Cvar *gw_pull_weapons = nullptr;

constexpr size_t kMaxWells = 8;

struct Well {
  vector pos;
  int room;
  float life_left;
  int parent_handle;
};

std::vector<Well> g_wells;

bool Usable() { return !IsClassic() && !(Game_mode & GM_MULTI); }

WellSettings Settings() {
  WellSettings s;
  s.radius = gw_radius->GetFloat();
  s.time = gw_time->GetFloat();
  s.force = gw_force->GetFloat();
  s.mass_ref = gw_mass_ref->GetFloat();
  s.mass_min_scale = gw_mass_min_scale->GetFloat();
  s.immune_mass = gw_immune_mass->GetFloat();
  return s;
}

// What a well may move: robots (not the Guide-Bot), matter weapons in flight,
// and the player. Never doors, powerups or buildings.
bool Pullable(const object &o, int parent_handle) {
  if ((o.flags & OF_DEAD) || (o.movement_type != MT_PHYSICS && o.movement_type != MT_WALKING) ||
      o.mtype.phys_info.mass <= 0.0f)
    return false;
  if (o.type == OBJ_ROBOT)
    return !IS_GUIDEBOT(&o);
  if (o.type == OBJ_PLAYER)
    return &o == Player_object && gw_self->GetBool();
  if (o.type == OBJ_WEAPON)
    return gw_pull_weapons->GetBool() && (Weapons[o.id].flags & WF_MATTER_WEAPON) && o.handle != parent_handle &&
           WeaponSidecar_ByIndex(o.id) == nullptr; // not another gravity missile
  return false;
}

// Distance from the well's centre to the object's surface, as the stock field
// measures it: a polygon model by its wall sphere.
float SurfaceDistance(const object &o, const vector &centre) {
  if ((o.flags & OF_POLYGON_OBJECT) && o.type != OBJ_WEAPON) {
    const vector pos = o.pos + o.wall_sphere_offset;
    return vm_VectorDistance(&pos, &centre) - Poly_models[o.rtype.pobj_info.model_num].wall_size;
  }
  return vm_VectorDistance(&o.pos, &centre) - o.size;
}

void Strand(const Well &w, const object &o, float dist) {
  const int visnum = VisEffectCreate(VIS_FIREBALL, LIGHTNING_BOLT_INDEX, w.room, const_cast<vector *>(&w.pos));
  if (visnum < 0)
    return;
  vis_effect *vis = &VisEffects[visnum];
  vis->lifeleft = vis->lifetime = Frametime;
  vis->end_pos = o.pos;
  vis->custom_handle = static_cast<int>(&o - Objects);
  vis->flags = VF_USES_LIFELEFT | VF_NO_Z_ADJUST;
  vis->size = std::max(1.0f, dist);
  vis->velocity.x() = 1;
  vis->velocity.y() = 0.25f;
}

void Pull(Well &w, const WellSettings &s) {
  const float self_radius = gw_self_radius->GetFloat();
  for (int i = 0; i <= Highest_object_index; ++i) {
    object &o = Objects[i];
    if (!Pullable(o, w.parent_handle))
      continue;
    const float dist = SurfaceDistance(o, w.pos);
    WellSettings use = s;
    if (o.type == OBJ_PLAYER)
      use.radius = std::min(s.radius, self_radius);
    const float accel = WellAcceleration(dist, w.life_left, o.mtype.phys_info.mass, use);
    const float dv = WellDeltaV(accel, Frametime);
    if (dv <= 0.0f)
      continue;
    vector dir = w.pos - o.pos;
    const float len = vm_NormalizeVector(&dir);
    if (!(len > 0.01f) || !std::isfinite(len))
      continue;
    vector push = dir * (dv * o.mtype.phys_info.mass); // phys_apply_force adds push / mass to velocity
    phys_apply_force(&o, &push);
    if (&o == Player_object)
      AddToShakeMagnitude(8.0f * (1.0f - std::max(0.0f, dist) / std::max(1.0f, use.radius)));
    else
      Strand(w, o, std::max(0.0f, dist));
  }
}

} // namespace

void Gravwell_Init() {
  const uint32_t flags = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  CvarRegistry &cv = Cvars();
  gw_radius = cv.RegisterFloat("gw_radius", 60.0f, 5.0f, 400.0f, "Gravity well: reach, to an object's surface", flags);
  gw_time = cv.RegisterFloat("gw_time", 1.5f, 0.1f, 10.0f, "Gravity well: seconds it lasts", flags);
  gw_force = cv.RegisterFloat("gw_force", 420.0f, 0.0f, 5000.0f, "Gravity well: peak pull, units a second squared", flags);
  gw_mass_ref = cv.RegisterFloat("gw_mass_ref", 4.0f, 0.1f, 100.0f, "Gravity well: objects up to this mass get the full pull", flags);
  gw_mass_min_scale = cv.RegisterFloat("gw_mass_min_scale", 0.15f, 0.0f, 1.0f, "Gravity well: share of the pull the heaviest still get", flags);
  gw_immune_mass = cv.RegisterFloat("gw_immune_mass", 40.0f, 0.0f, 1000.0f, "Gravity well: this heavy or more ignores it; 0 none", flags);
  gw_self = cv.RegisterBool("gw_self", true, "Gravity well: pulls the player too", flags);
  gw_self_radius = cv.RegisterFloat("gw_self_radius", 40.0f, 0.0f, 400.0f, "Gravity well: reach on the player", flags);
  gw_pull_weapons = cv.RegisterBool("gw_pull_weapons", true, "Gravity well: bends missiles in flight", flags);
}

void Gravwell_Reset() { g_wells.clear(); }

void Gravwell_WeaponExploded(const object *weapon_obj, const vector *point) {
  if (!Usable() || weapon_obj == nullptr || weapon_obj->type != OBJ_WEAPON)
    return;
  const SidecarWeapon *side = WeaponSidecar_ByIndex(weapon_obj->id);
  if (side == nullptr || !(side->flags & TWF_GRAVWELL))
    return;
  if (g_wells.size() >= kMaxWells)
    g_wells.erase(g_wells.begin());
  Well w{point != nullptr ? *point : weapon_obj->pos, weapon_obj->roomnum, gw_time->GetFloat(), weapon_obj->handle};
  // A glow at the centre for the life of the well, as the stock field has.
  const int visnum = VisEffectCreate(VIS_FIREBALL, SUN_CORONA_INDEX, w.room, &w.pos);
  if (visnum >= 0) {
    vis_effect *vis = &VisEffects[visnum];
    vis->size = gw_radius->GetFloat() / 4.0f;
    vis->lifeleft = vis->lifetime = w.life_left;
    vis->flags = VF_USES_LIFELEFT | VF_WINDSHIELD_EFFECT;
  }
  CreateRandomSparks(6, &w.pos, w.room, -1, 2);
  LOG_INFO.printf("[tartarus] gravity well in room %d at %.0f %.0f %.0f", w.room, w.pos.x(), w.pos.y(), w.pos.z());
  g_wells.push_back(w);
}

void Gravwell_Frame() {
  if (g_wells.empty())
    return;
  if (!Usable() || Player_object == nullptr) {
    g_wells.clear();
    return;
  }
  if (Frametime <= 0.0f)
    return;
  const WellSettings s = Settings();
  for (Well &w : g_wells) {
    Pull(w, s);
    w.life_left -= Frametime;
  }
  g_wells.erase(std::remove_if(g_wells.begin(), g_wells.end(), [](const Well &w) { return w.life_left <= 0.0f; }),
                g_wells.end());
}

} // namespace tartarus
