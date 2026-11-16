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

#include "gravmissile.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <optional>

#include "binds.h"
#include "classic.h"
#include "cloak.h"
#include "cvar.h"
#include "game.h"
#include "gamefont.h"
#include "grtext.h"
#include "hlsoundlib.h"
#include "hud.h"
#include "log.h"
#include "Mission.h"
#include "mouse_flight.h"
#include "object.h"
#include "player.h"
#include "renderer.h"
#include "sidecar_core.h"
#include "sounds.h"
#include "weapon.h"
#include "weapons_sidecar.h"

int CreateAndFireWeapon(vector *pos, vector *dir, object *parent, int weapon_num); // WeaponFire.cpp

namespace tartarus {

namespace {

Cvar *gm_fire_key = nullptr;
Cvar *gm_level_ammo = nullptr;
Cvar *hud_gravmissile = nullptr;
Cvar *hud_gravmissile_y = nullptr;

int g_ammo = 0;
float g_last_fire = -1000.0f;
int g_last_level = 0;
std::string g_last_mission;

bool Usable() { return !IsClassic() && !(Game_mode & GM_MULTI) && Player_object != nullptr && Player_object->type == OBJ_PLAYER; }

// The weapon the key fires: the first tactical gravity-well weapon in weapons.txt.
const SidecarWeapon *Missile() {
  const SidecarWeapon *w = WeaponSidecar_Find("GravMissile");
  return w != nullptr && w->tactical && (w->flags & TWF_GRAVWELL) ? w : nullptr;
}

int Max() {
  const SidecarWeapon *w = Missile();
  return w != nullptr ? std::max(1, w->ammo_max) : 0;
}

std::filesystem::path StatePath(std::filesystem::path save_path) {
  save_path.replace_extension(".tstate");
  return save_path;
}

void Fire() {
  const SidecarWeapon *w = Missile();
  if (w == nullptr) {
    AddHUDMessage("No gravity missile in weapons.txt");
    return;
  }
  if (Gametime - g_last_fire < w->fire_delay)
    return;
  if (g_ammo <= 0) {
    AddHUDMessage("No gravity missiles");
    return;
  }
  vector dir = Player_object->orient.fvec;
  vector pos = Player_object->pos + dir * (Player_object->size * 0.8f);
  if (CreateAndFireWeapon(&pos, &dir, Player_object, w->index) < 0)
    return; // no room in front of the ship
  --g_ammo;
  g_last_fire = Gametime;
  const int sound = Weapons[w->index].sounds[WSI_FIRE];
  if (sound != SOUND_NONE_INDEX)
    Sound_system.Play3dSound(sound, SND_PRIORITY_HIGHEST, Player_object);
  Cloak_WeaponFired();
}

} // namespace

void GravMissile_Init() {
  const uint32_t flags = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  CvarRegistry &cv = Cvars();
  gm_fire_key = cv.RegisterString("gm_fire_key", "g", "Gravity missile: the key that fires it", flags);
  gm_level_ammo = cv.RegisterInt("gm_level_ammo", 3, 0, 99, "Gravity missile: each level starts with at least this many", flags);
  hud_gravmissile = cv.RegisterInt("hud_gravmissile", 1, 0, 2, "Gravity missile count: 0 off, 1 while you have any, 2 always", flags);
  hud_gravmissile_y = cv.RegisterFloat("hud_gravmissile_y", 0.665f, 0.05f, 0.95f, "Gravity missile count: height on the view", flags);
}

bool GravMissile_HandleKey(int key) {
  if (!Usable() || !Flight_ContextActive())
    return false;
  const std::optional<int> bound = Binds_ParseKey(gm_fire_key->GetString());
  if (!bound.has_value() || key != *bound)
    return false;
  if (!(Players[Player_num].flags & (PLAYER_FLAGS_DYING | PLAYER_FLAGS_DEAD)))
    Fire();
  return true;
}

void GravMissile_LevelStart() {
  const std::string mission = Current_mission.filename != nullptr ? Current_mission.filename : "";
  const int level = Current_mission.cur_level;
  if (mission != g_last_mission || level <= g_last_level)
    g_ammo = 0; // a new game, or the same level again
  g_last_mission = mission;
  g_last_level = level;
  const int max = Max();
  g_ammo = std::min(std::max(g_ammo, gm_level_ammo->GetInt()), max);
  g_last_fire = -1000.0f;
}

void GravMissile_Save(const std::filesystem::path &save_path) {
  std::ofstream out(StatePath(save_path), std::ios::trunc);
  out << "gravmissile " << g_ammo << "\n";
}

void GravMissile_Load(const std::filesystem::path &save_path) {
  std::ifstream in(StatePath(save_path));
  std::string word;
  int count = 0;
  if (in >> word >> count && word == "gravmissile")
    g_ammo = std::clamp(count, 0, Max());
  // No file: saved before the gravity missile; keep the level-start refill.
  g_last_fire = -1000.0f;
  g_last_mission = Current_mission.filename != nullptr ? Current_mission.filename : "";
  g_last_level = Current_mission.cur_level;
}

bool GravMissile_Give(const std::string &squashed_name) {
  const SidecarWeapon *w = Missile();
  if (w == nullptr)
    return false;
  std::string name;
  for (char c : w->name)
    if (c != ' ' && c != '_' && c != '-')
      name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  if (squashed_name != name)
    return false;
  g_ammo = Max();
  return true;
}

void GravMissile_Fill() {
  if (!IsClassic())
    g_ammo = Max();
}

void GravMissile_Render() {
  if (!Usable() || hud_gravmissile->GetInt() == 0 || !Flight_ContextActive() || Missile() == nullptr)
    return;
  if (hud_gravmissile->GetInt() == 1 && g_ammo <= 0)
    return;
  char text[48];
  std::snprintf(text, sizeof(text), "GRAV MISSILES %d", g_ammo);
  const int cx = Game_window_x + Game_window_w / 2;
  const int y = Game_window_y + static_cast<int>(Game_window_h * hud_gravmissile_y->GetFloat());
  StartFrame(0, 0, Max_window_w, Max_window_h, false);
  grtext_SetFont(SMALL_FONT);
  grtext_SetColor(g_ammo > 0 ? GR_RGB(190, 140, 255) : GR_RGB(150, 120, 170));
  grtext_SetAlpha(220);
  grtext_Printf(cx - grtext_GetTextLineWidth(text) / 2, y, "%s", text);
  grtext_Flush();
  EndFrame();
}

} // namespace tartarus
