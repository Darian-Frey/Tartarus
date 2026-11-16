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

#include "launcher.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "binds.h"
#include "classic.h"
#include "cloak.h"
#include "cvar.h"
#include "emp.h"
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
Cvar *gm_cycle_key = nullptr;
Cvar *gm_level_ammo = nullptr;
Cvar *emp_level_ammo = nullptr;
Cvar *hud_gravmissile = nullptr;
Cvar *hud_gravmissile_y = nullptr;

std::map<std::string, int> g_ammo;        // by weapon name
std::map<std::string, float> g_last_fire; // by weapon name
std::string g_selected;
int g_last_level = 0;
std::string g_last_mission;

bool Usable() {
  return !IsClassic() && !(Game_mode & GM_MULTI) && Player_object != nullptr && Player_object->type == OBJ_PLAYER;
}

std::vector<const SidecarWeapon *> Tactical() {
  std::vector<const SidecarWeapon *> list;
  for (const SidecarWeapon &w : WeaponSidecar_All())
    if (w.tactical)
      list.push_back(&w);
  return list;
}

const SidecarWeapon *Selected() {
  const std::vector<const SidecarWeapon *> list = Tactical();
  for (const SidecarWeapon *w : list)
    if (w->name == g_selected)
      return w;
  return list.empty() ? nullptr : list.front();
}

int Max(const SidecarWeapon &w) { return std::max(1, w.ammo_max); }
int &Ammo(const SidecarWeapon &w) { return g_ammo[w.name]; }

int LevelAmmo(const SidecarWeapon &w) {
  if (w.flags & TWF_GRAVWELL)
    return gm_level_ammo->GetInt();
  if (w.flags & TWF_EMP)
    return emp_level_ammo->GetInt();
  return 0;
}

// "GravMissile" -> "GRAV MISSILE", "EMPMine" -> "EMP MINE".
std::string Display(const std::string &name) {
  std::string out;
  for (size_t i = 0; i < name.size(); ++i) {
    const char c = name[i];
    const bool upper = std::isupper(static_cast<unsigned char>(c)) != 0;
    const bool next_lower = i + 1 < name.size() && std::islower(static_cast<unsigned char>(name[i + 1])) != 0;
    const bool prev_lower = i > 0 && std::islower(static_cast<unsigned char>(name[i - 1])) != 0;
    if (i > 0 && upper && (prev_lower || next_lower) && out.back() != ' ')
      out.push_back(' ');
    out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
  }
  return out;
}

std::string Squash(const std::string &name) {
  std::string out;
  for (char c : name)
    if (c != ' ' && c != '_' && c != '-')
      out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  return out;
}

std::filesystem::path StatePath(std::filesystem::path save_path) {
  save_path.replace_extension(".tstate");
  return save_path;
}

bool KeyIs(const Cvar *bind, int key) {
  const std::optional<int> bound = Binds_ParseKey(bind->GetString());
  return bound.has_value() && key == *bound;
}

void Fire() {
  const SidecarWeapon *w = Selected();
  if (w == nullptr) {
    AddHUDMessage("No tactical weapons in weapons.txt");
    return;
  }
  float &last = g_last_fire.try_emplace(w->name, -1000.0f).first->second;
  if (Gametime - last < w->fire_delay)
    return;
  int &ammo = Ammo(*w);
  if (ammo <= 0) {
    AddHUDMessage("No %s left", Display(w->name).c_str());
    return;
  }
  const bool drop = Weapons[w->index].phys_info.velocity.z() < 1.0f; // mines are dropped behind
  vector dir = Player_object->orient.fvec * (drop ? -1.0f : 1.0f);
  vector pos = Player_object->pos + dir * (Player_object->size * (drop ? 1.2f : 0.8f));
  const int objnum = CreateAndFireWeapon(&pos, &dir, Player_object, w->index);
  if (objnum < 0)
    return; // no room there
  --ammo;
  last = Gametime;
  const int sound = Weapons[w->index].sounds[WSI_FIRE];
  if (sound != SOUND_NONE_INDEX)
    Sound_system.Play3dSound(sound, SND_PRIORITY_HIGHEST, Player_object);
  if (w->flags & TWF_EMP)
    Emp_MineDropped(&Objects[objnum]);
  Cloak_WeaponFired();
}

void Cycle() {
  const std::vector<const SidecarWeapon *> list = Tactical();
  if (list.empty())
    return;
  const SidecarWeapon *current = Selected();
  size_t at = 0;
  for (size_t i = 0; i < list.size(); ++i)
    if (list[i] == current)
      at = i;
  for (size_t step = 1; step <= list.size(); ++step) {
    const SidecarWeapon *next = list[(at + step) % list.size()];
    if (Ammo(*next) > 0 || step == list.size()) {
      g_selected = next->name;
      AddHUDMessage("%s selected", Display(next->name).c_str());
      Sound_system.Play2dSound(SOUND_CHANGE_SECONDARY);
      return;
    }
  }
}

} // namespace

void Launcher_Init() {
  const uint32_t flags = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  CvarRegistry &cv = Cvars();
  gm_fire_key = cv.RegisterString("gm_fire_key", "g", "Tactical launcher: fires the selected weapon", flags);
  gm_cycle_key = cv.RegisterString("gm_cycle_key", "y", "Tactical launcher: selects the next weapon you carry", flags);
  gm_level_ammo = cv.RegisterInt("gm_level_ammo", 3, 0, 99, "Gravity missile: each level starts with at least this many", flags);
  emp_level_ammo = cv.RegisterInt("emp_level_ammo", 2, 0, 99, "EMP mine: each level starts with at least this many", flags);
  hud_gravmissile = cv.RegisterInt("hud_gravmissile", 1, 0, 2, "Tactical weapon readout: 0 off, 1 while you have any, 2 always", flags);
  hud_gravmissile_y = cv.RegisterFloat("hud_gravmissile_y", 0.665f, 0.05f, 0.95f, "Tactical weapon readout: height on the view", flags);
}

bool Launcher_HandleKey(int key) {
  const bool fire = KeyIs(gm_fire_key, key);
  const bool cycle = !fire && KeyIs(gm_cycle_key, key);
  if ((!fire && !cycle) || !Usable() || !Flight_ContextActive())
    return false;
  if (Players[Player_num].flags & (PLAYER_FLAGS_DYING | PLAYER_FLAGS_DEAD))
    return true;
  if (fire)
    Fire();
  else
    Cycle();
  return true;
}

void Launcher_LevelStart() {
  const std::string mission = Current_mission.filename != nullptr ? Current_mission.filename : "";
  const int level = Current_mission.cur_level;
  if (mission != g_last_mission || level <= g_last_level)
    g_ammo.clear(); // a new game, or the same level again
  g_last_mission = mission;
  g_last_level = level;
  for (const SidecarWeapon *w : Tactical()) {
    int &ammo = Ammo(*w);
    ammo = std::min(std::max(ammo, LevelAmmo(*w)), Max(*w));
  }
  g_last_fire.clear();
}

void Launcher_Save(const std::filesystem::path &save_path) {
  std::ofstream out(StatePath(save_path), std::ios::trunc);
  for (const SidecarWeapon *w : Tactical())
    out << "ammo " << w->name << " " << Ammo(*w) << "\n";
  if (const SidecarWeapon *w = Selected())
    out << "selected " << w->name << "\n";
}

void Launcher_Load(const std::filesystem::path &save_path) {
  std::ifstream in(StatePath(save_path));
  if (in) {
    g_ammo.clear();
    std::string line;
    while (std::getline(in, line)) {
      std::istringstream words(line);
      std::string key, name;
      int count = 0;
      if (!(words >> key))
        continue;
      if (key == "gravmissile" && words >> count) { // saves from before the launcher (F-021)
        name = "GravMissile";
      } else if (key == "ammo" && words >> name >> count) {
      } else if (key == "selected" && words >> name) {
        g_selected = name;
        continue;
      } else {
        continue;
      }
      if (const SidecarWeapon *w = WeaponSidecar_Find(name); w != nullptr && w->tactical)
        g_ammo[w->name] = std::clamp(count, 0, Max(*w));
    }
  }
  // No file: saved before Tartarus weapons; keep the level-start refill.
  g_last_fire.clear();
  g_last_mission = Current_mission.filename != nullptr ? Current_mission.filename : "";
  g_last_level = Current_mission.cur_level;
}

bool Launcher_Give(const std::string &squashed_name) {
  for (const SidecarWeapon *w : Tactical()) {
    if (Squash(w->name) == squashed_name || Squash(Display(w->name)) == squashed_name) {
      Ammo(*w) = Max(*w);
      return true;
    }
  }
  return false;
}

void Launcher_Fill() {
  if (IsClassic())
    return;
  for (const SidecarWeapon *w : Tactical())
    Ammo(*w) = Max(*w);
}

void Launcher_Render() {
  const SidecarWeapon *w = Selected();
  if (!Usable() || hud_gravmissile->GetInt() == 0 || !Flight_ContextActive() || w == nullptr)
    return;
  int carried = 0;
  for (const SidecarWeapon *t : Tactical())
    carried += Ammo(*t);
  if (hud_gravmissile->GetInt() == 1 && carried <= 0)
    return;
  const int ammo = Ammo(*w);
  char text[64];
  std::snprintf(text, sizeof(text), "%s %d", Display(w->name).c_str(), ammo);
  const int cx = Game_window_x + Game_window_w / 2;
  const int y = Game_window_y + static_cast<int>(Game_window_h * hud_gravmissile_y->GetFloat());
  StartFrame(0, 0, Max_window_w, Max_window_h, false);
  grtext_SetFont(SMALL_FONT);
  grtext_SetColor(ammo > 0 ? GR_RGB(190, 140, 255) : GR_RGB(150, 120, 170));
  grtext_SetAlpha(220);
  grtext_Printf(cx - grtext_GetTextLineWidth(text) / 2, y, "%s", text);
  grtext_Flush();
  EndFrame();
}

} // namespace tartarus
