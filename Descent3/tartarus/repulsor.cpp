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

#include "repulsor.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "binds.h"
#include "classic.h"
#include "cloak.h"
#include "cvar.h"
#include "damage.h"
#include "findintersection.h"
#include "fireball.h"
#include "game.h"
#include "gameevent.h"
#include "gamefont.h"
#include "gravwell.h"
#include "gravwell_core.h"
#include "grtext.h"
#include "hlsoundlib.h"
#include "hud.h"
#include "log.h"
#include "mouse_flight.h"
#include "object.h"
#include "objinfo.h"
#include "physics.h"
#include "player.h"
#include "renderer.h"
#include "sounds.h"
#include "viseffect.h"
#include "weapon.h"
#include "weapons_sidecar.h"

namespace tartarus {

namespace {

Cvar *rp_key = nullptr;
Cvar *rp_radius = nullptr;
Cvar *rp_force = nullptr;
Cvar *rp_cost = nullptr;
Cvar *rp_cooldown = nullptr;
Cvar *rp_deflect = nullptr;
Cvar *rp_self = nullptr;
Cvar *hud_repulsor = nullptr;
Cvar *hud_repulsor_y = nullptr;

float g_last_pulse = -1000.0f;

bool Usable() {
  return !IsClassic() && !(Game_mode & GM_MULTI) && Player_object != nullptr && Player_object->type == OBJ_PLAYER;
}

float Left() { return std::max(0.0f, rp_cooldown->GetFloat() - (Gametime - g_last_pulse)); }

// Nothing solid (wall, terrain, closed door) between the ship and p.
bool InSight(vector p) {
  vector start = Player_object->pos;
  fvi_query query{};
  fvi_info hit{};
  query.p0 = &start;
  query.p1 = &p;
  query.startroom = Player_object->roomnum;
  query.rad = 0.0f;
  query.thisobjnum = OBJNUM(Player_object);
  query.ignore_obj_list = nullptr;
  query.flags = FQ_CHECK_OBJS | FQ_ONLY_DOOR_OBJ | FQ_NO_RELINK;
  return fvi_FindIntersection(&query, &hit) == HIT_NONE;
}

// Distance to a wall along dir, or -1 if none within reach.
float WallDistance(const vector &dir, float reach) {
  vector start = Player_object->pos;
  vector end = start + dir * reach;
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
  return vm_VectorDistance(&hit.hit_pnt, &start);
}

void Strand(const object &o, float dist) {
  vector from = Player_object->pos;
  const int visnum = VisEffectCreate(VIS_FIREBALL, LIGHTNING_BOLT_INDEX, Player_object->roomnum, &from);
  if (visnum < 0)
    return;
  vis_effect *vis = &VisEffects[visnum];
  vis->lifeleft = vis->lifetime = 0.25f;
  vis->end_pos = o.pos;
  vis->custom_handle = static_cast<int>(&o - Objects);
  vis->flags = VF_USES_LIFELEFT | VF_NO_Z_ADJUST;
  vis->size = std::max(1.0f, dist);
  vis->velocity.x() = 1;
  vis->velocity.y() = 0.4f;
}

void Push(object &o, const vector &dv) {
  if (!std::isfinite(dv.x()) || !std::isfinite(dv.y()) || !std::isfinite(dv.z()))
    return;
  vector force = dv * o.mtype.phys_info.mass; // phys_apply_force adds force / mass to velocity
  phys_apply_force(&o, &force);
}

// What the pulse moves: robots but not the Guide-Bot, and missiles (matter
// weapons) that are not the player's own.
bool Pushable(const object &o) {
  if ((o.flags & OF_DEAD) || (o.movement_type != MT_PHYSICS && o.movement_type != MT_WALKING) ||
      o.mtype.phys_info.mass <= 0.0f)
    return false;
  if (o.type == OBJ_ROBOT)
    return !IS_GUIDEBOT(&o);
  if (o.type == OBJ_WEAPON)
    return rp_deflect->GetBool() && (Weapons[o.id].flags & WF_MATTER_WEAPON) &&
           ObjGetUltimateParent(const_cast<object *>(&o)) != Player_object && WeaponSidecar_ByIndex(o.id) == nullptr;
  return false;
}

// Robots shoved, missiles turned. Returns how many were moved.
int PushOthers(const WellSettings &s) {
  int moved = 0;
  for (int i = 0; i <= Highest_object_index; ++i) {
    object &o = Objects[i];
    if (&o == Player_object || !Pushable(o))
      continue;
    const float dist = Gravwell_SurfaceDistance(o, Player_object->pos);
    if (dist >= s.radius)
      continue;
    vector out = o.pos - Player_object->pos;
    const float len = vm_NormalizeVector(&out);
    if (!(len > 0.01f) || !std::isfinite(len) || !InSight(o.pos))
      continue;
    float dv = 0.0f;
    if (o.type == OBJ_WEAPON)
      dv = DeflectDeltaV(vm_DotProduct(&o.mtype.phys_info.velocity, &out)); // closing missiles go back the way they came
    else
      dv = PulseDeltaV(dist, o.mtype.phys_info.mass, s);
    if (dv <= 0.0f)
      continue;
    Push(o, out * dv);
    Strand(o, std::max(0.0f, dist));
    ++moved;
  }
  return moved;
}

// Near a wall the pulse pushes off it: rays along the ship's three axes and
// the eight corners between them; the ship goes away from the walls hit,
// harder the closer the nearest one is (F-022, wall-launch).
float PushSelf(const WellSettings &s) {
  const float scale = rp_self->GetFloat();
  if (scale <= 0.0f)
    return 0.0f;
  const matrix &m = Player_object->orient;
  const float reach = s.radius + Player_object->size;
  vector away{};
  float strongest = 0.0f;
  for (int x = -1; x <= 1; ++x)
    for (int y = -1; y <= 1; ++y)
      for (int z = -1; z <= 1; ++z) {
        const int axes = (x != 0) + (y != 0) + (z != 0);
        if (axes != 1 && axes != 3)
          continue; // the six faces and eight corners
        vector dir = m.rvec * static_cast<float>(x) + m.uvec * static_cast<float>(y) + m.fvec * static_cast<float>(z);
        vm_NormalizeVector(&dir);
        const float d = WallDistance(dir, reach);
        if (d < 0.0f)
          continue;
        const float fall = std::clamp(1.0f - std::max(0.0f, d - Player_object->size) / s.radius, 0.0f, 1.0f);
        away -= dir * fall;
        strongest = std::max(strongest, fall);
      }
  if (strongest <= 0.0f || vm_NormalizeVector(&away) < 0.01f)
    return 0.0f;
  const float dv = s.force * scale * strongest;
  Push(*Player_object, away * dv);
  return dv;
}

void Pulse() {
  const float cost = rp_cost->GetFloat();
  if (Left() > 0.0f) {
    AddHUDMessage("Repulsor recharging: %.0f s", std::ceil(Left()));
    return;
  }
  float &energy = Players[Player_num].energy;
  if (energy < cost) {
    AddHUDMessage("Repulsor needs %.0f energy", cost);
    return;
  }
  energy -= cost;
  g_last_pulse = Gametime;

  WellSettings s = Gravwell_Settings(); // the mass settings are shared with the well
  s.radius = rp_radius->GetFloat();
  s.force = rp_force->GetFloat();
  const int moved = PushOthers(s);
  const float self = PushSelf(s);

  vector pos = Player_object->pos;
  CreateRandomSparks(12, &pos, Player_object->roomnum, -1, 2);
  float flash[4] = {0.45f, 0.65f, 1.0f, 0.35f};
  CreateNewEvent(RENDER_EVENT, 0, 0, &flash, sizeof(flash), DrawAlphaEvent);
  AddToShakeMagnitude(12.0f);
  Sound_system.Play3dSound(SOUND_FORCEFIELD_BOUNCE, SND_PRIORITY_HIGHEST, Player_object);
  Cloak_WeaponFired();
  LOG_INFO.printf("[tartarus] repulsor: %d moved, ship pushed off walls at %.0f", moved, self);
}

} // namespace

void Repulsor_Init() {
  const uint32_t flags = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  CvarRegistry &cv = Cvars();
  rp_key = cv.RegisterString("rp_key", "v", "Repulsor: the key that fires it", flags);
  rp_radius = cv.RegisterFloat("rp_radius", 35.0f, 5.0f, 200.0f, "Repulsor: reach, to an object's surface", flags);
  rp_force = cv.RegisterFloat("rp_force", 150.0f, 0.0f, 2000.0f, "Repulsor: speed given at contact, units a second", flags);
  rp_cost = cv.RegisterFloat("rp_cost", 20.0f, 0.0f, 200.0f, "Repulsor: energy per pulse", flags);
  rp_cooldown = cv.RegisterFloat("rp_cooldown", 4.0f, 0.0f, 60.0f, "Repulsor: seconds between pulses", flags);
  rp_deflect = cv.RegisterBool("rp_deflect", true, "Repulsor: turns missiles closing on you round", flags);
  rp_self = cv.RegisterFloat("rp_self", 0.4f, 0.0f, 2.0f, "Repulsor: share of the shove that launches you off a wall; 0 off", flags);
  hud_repulsor = cv.RegisterInt("hud_repulsor", 1, 0, 2, "Repulsor readout: 0 off, 1 while recharging, 2 always", flags);
  hud_repulsor_y = cv.RegisterFloat("hud_repulsor_y", 0.735f, 0.05f, 0.95f, "Repulsor readout: height on the view", flags);
}

bool Repulsor_HandleKey(int key) {
  if (!Usable() || !Flight_ContextActive())
    return false;
  const std::optional<int> bound = Binds_ParseKey(rp_key->GetString());
  if (!bound.has_value() || key != *bound)
    return false;
  if (!(Players[Player_num].flags & (PLAYER_FLAGS_DYING | PLAYER_FLAGS_DEAD)))
    Pulse();
  return true;
}

void Repulsor_Reset() { g_last_pulse = -1000.0f; }

void Repulsor_Render() {
  const int mode = hud_repulsor->GetInt();
  if (!Usable() || mode == 0 || !Flight_ContextActive())
    return;
  const float left = Left();
  if (mode == 1 && left <= 0.0f)
    return;
  char text[48];
  if (left > 0.0f)
    std::snprintf(text, sizeof(text), "REPULSOR %.0f", std::ceil(left));
  else
    std::snprintf(text, sizeof(text), "REPULSOR READY");
  const int cx = Game_window_x + Game_window_w / 2;
  const int y = Game_window_y + static_cast<int>(Game_window_h * hud_repulsor_y->GetFloat());
  StartFrame(0, 0, Max_window_w, Max_window_h, false);
  grtext_SetFont(SMALL_FONT);
  grtext_SetColor(left > 0.0f ? GR_RGB(150, 160, 180) : GR_RGB(120, 190, 255));
  grtext_SetAlpha(220);
  grtext_Printf(cx - grtext_GetTextLineWidth(text) / 2, y, "%s", text);
  grtext_Flush();
  EndFrame();
}

} // namespace tartarus
