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

#include "weapons_sidecar.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "cfile.h"
#include "console.h"
#include "log.h"
#include "polymodel.h"
#include "sidecar_core.h"
#include "soundload.h"
#include "tartarus_weapons_default.h"
#include "weapon.h"

namespace tartarus {

namespace {

std::vector<SidecarWeapon> g_weapons;
bool g_command_added = false;

void Problem(const std::string &where, const std::string &text) {
  LOG_WARNING << "[tartarus] " << where << ": " << text;
}

std::vector<std::string> GameWeaponNames() {
  std::vector<std::string> names;
  for (int i = 0; i < MAX_WEAPONS; ++i)
    if (Weapons[i].used)
      names.emplace_back(Weapons[i].name);
  return names;
}

int FreeSlots() {
  int free = 0;
  for (int i = MAX_STATIC_WEAPONS; i < MAX_WEAPONS; ++i)
    free += Weapons[i].used == 0 ? 1 : 0;
  return free;
}

bool Number(const SidecarKey &k, size_t i, float *out, const std::string &where) {
  if (i < k.values.size() && SidecarNumber(k.values[i], out))
    return true;
  Problem(where, "line " + std::to_string(k.line) + ": " + k.name + " needs a number");
  return false;
}

// Applies one key to the new weapon or its side-table entry. Unknown keys are
// reported, so a typing mistake does not pass silently.
void Apply(const SidecarKey &k, weapon *w, SidecarWeapon *side, const std::string &where) {
  float v = 0.0f;
  const std::string &n = k.name;
  if (n == "base" || n == "class") {
    // handled before
  } else if (n == "speed") {
    if (Number(k, 0, &v, where))
      w->phys_info.velocity.z() = v;
  } else if (n == "lifetime") {
    if (Number(k, 0, &v, where))
      w->life_time = v;
  } else if (n == "mass") {
    if (Number(k, 0, &v, where))
      w->phys_info.mass = v;
  } else if (n == "damage") {
    if (Number(k, 0, &v, where))
      w->player_damage = w->generic_damage = v;
  } else if (n == "impact_size") {
    if (Number(k, 0, &v, where))
      w->impact_size = v;
  } else if (n == "impact_damage") {
    if (Number(k, 0, &v, where))
      w->impact_player_damage = w->impact_generic_damage = v;
  } else if (n == "tint") {
    float r, g, b;
    if (Number(k, 0, &r, where) && Number(k, 1, &g, where) && Number(k, 2, &b, where)) {
      w->lighting_info.red_light1 = w->lighting_info.red_light2 = r;
      w->lighting_info.green_light1 = w->lighting_info.green_light2 = g;
      w->lighting_info.blue_light1 = w->lighting_info.blue_light2 = b;
    }
  } else if (n == "fire_sound" || n == "impact_sound") {
    const int id = FindSoundName(k.values[0].c_str());
    if (id < 0)
      Problem(where, "line " + std::to_string(k.line) + ": no sound called \"" + k.values[0] + "\"; kept the base's");
    else
      w->sounds[n == "fire_sound" ? WSI_FIRE : WSI_IMPACT_WALL] = static_cast<int16_t>(id);
  } else if (n == "model") {
    int id = FindPolyModelName(k.values[0]);
    if (id < 0)
      id = LoadPolyModel(k.values[0], 1);
    if (id < 0)
      Problem(where, "line " + std::to_string(k.line) + ": no model called \"" + k.values[0] + "\"; kept the base's");
    else {
      w->fire_image_handle = static_cast<int16_t>(id);
      w->flags &= ~(WF_IMAGE_BITMAP | WF_IMAGE_VCLIP);
    }
  } else if (n == "fire_delay") {
    if (Number(k, 0, &v, where))
      side->fire_delay = v;
  } else if (n == "ammo_pickup") {
    if (Number(k, 0, &v, where))
      side->ammo_pickup = static_cast<int>(v);
  } else if (n == "ammo_max") {
    if (Number(k, 0, &v, where))
      side->ammo_max = static_cast<int>(v);
  } else if (n == "flags") {
    for (const std::string &f : k.values)
      side->flags |= TartarusFlagBit(f);
  } else {
    Problem(where, "line " + std::to_string(k.line) + ": unknown key " + n + " (ignored)");
  }
}

// A stock weapon, for picking a base: its flags, physics and spawns.
std::string StockLine(int i) {
  const weapon &w = Weapons[i];
  const bool model = !(w.flags & WF_IMAGE_BITMAP) && !(w.flags & WF_IMAGE_VCLIP) && w.fire_image_handle >= 0;
  char line[320];
  std::snprintf(line, sizeof(line),
                "%.40s (#%d): flags 0x%x, phys 0x%x, speed %.0f, life %.1f, mass %.2f, drag %.2f, damage %.0f, "
                "blast %.0f/%.0f, spawn %d x%d, robot %d, %s %.40s",
                w.name, i, static_cast<unsigned>(w.flags), static_cast<unsigned>(w.phys_info.flags),
                w.phys_info.velocity.z(), w.life_time, w.phys_info.mass, w.phys_info.drag, w.generic_damage,
                w.impact_size, w.impact_generic_damage, w.spawn_handle, w.spawn_count, w.robot_spawn_handle,
                model ? "model" : "image", model ? Poly_models[w.fire_image_handle].name : "");
  return line;
}

void WeaponInfo(const CommandArgs &a, ConsoleCore &c) {
  if (a.size() > 1 && a[1] == "stock") { // every stock weapon, also in the log
    for (int i = 0; i < MAX_WEAPONS; ++i) {
      if (!Weapons[i].used || WeaponSidecar_ByIndex(i) != nullptr)
        continue;
      const std::string line = StockLine(i);
      c.Print(line);
      LOG_INFO << "[tartarus] weaponinfo: " << line;
    }
    return;
  }
  if (g_weapons.empty()) {
    c.Print("weaponinfo: no sidecar weapons loaded");
    return;
  }
  for (const SidecarWeapon &s : g_weapons) {
    if (a.size() > 1 && !SameWeaponName(a[1], s.name))
      continue;
    const weapon &w = Weapons[s.index];
    char line[240];
    std::snprintf(line, sizeof(line),
                  "%s (#%d, from %s)%s: speed %.0f, lifetime %.1f, mass %.1f, damage %.0f, fire delay %.1f, "
                  "ammo %d a pickup, %d at most, flags 0x%x",
                  s.name.c_str(), s.index, s.base.c_str(), s.tactical ? " tactical" : "", w.phys_info.velocity.z(),
                  w.life_time, w.phys_info.mass, w.generic_damage, s.fire_delay, s.ammo_pickup, s.ammo_max,
                  static_cast<unsigned>(s.flags));
    c.Print(line);
  }
}

} // namespace

void WeaponSidecar_Load() {
  if (!g_command_added) {
    g_command_added = true;
    Console().AddCommand({"weaponinfo", "weaponinfo [name|stock]", "List the Tartarus weapons from weapons.txt (D-007), or every stock weapon",
                          CMD_NONE, [](const CommandArgs &a, ConsoleCore &c) { WeaponInfo(a, c); }});
  }
  if (!g_weapons.empty())
    return; // once: the weapons stay for the whole run

  std::string text = kTartarusDefaultWeapons, source = "weapons.txt (built in)";
  const std::filesystem::path override_path = cf_GetWritableBaseDirectory() / "weapons.txt";
  std::error_code ec;
  if (std::filesystem::is_regular_file(override_path, ec)) {
    std::ifstream in(override_path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    text = buffer.str();
    source = "weapons.txt";
    LOG_INFO << "[tartarus] weapons: reading " << override_path.string() << " instead of the built-in weapons.txt";
  }

  const SidecarParse parse = ParseSidecar(text);
  for (const std::string &e : parse.errors)
    Problem(source, e);
  const std::vector<std::string> problems = CheckSidecar(parse, GameWeaponNames());
  for (const std::string &p : problems)
    Problem(source, p);

  for (const SidecarBlock &block : parse.weapons) {
    const std::string where = source + ": weapon \"" + block.name + "\"";
    bool bad = false;
    for (const std::string &p : problems)
      bad = bad || p.find("weapon \"" + block.name + "\":") != std::string::npos;
    if (bad) {
      Problem(where, "not loaded");
      continue;
    }
    if (FreeSlots() == 0) { // AllocWeapon() would stop on Int3()
      Problem(where, "not loaded: all " + std::to_string(MAX_WEAPONS) + " weapon slots are in use (AV-010)");
      continue;
    }
    const int base = FindWeaponName(block.Find("base")->values[0].c_str());
    const int index = AllocWeapon();
    if (base < 0 || index < 0) {
      Problem(where, "not loaded");
      continue;
    }
    Weapons[index] = Weapons[base];
    Weapons[index].used = 1;
    std::snprintf(Weapons[index].name, sizeof(Weapons[index].name), "%s", block.name.c_str());

    SidecarWeapon side;
    side.index = index;
    side.name = block.name;
    side.base = Weapons[base].name;
    const SidecarKey *cls = block.Find("class");
    side.tactical = cls != nullptr && cls->values[0] == "tactical";
    for (const SidecarKey &k : block.keys)
      Apply(k, &Weapons[index], &side, where);
    g_weapons.push_back(side);
    LOG_INFO << "[tartarus] " << source << ": weapon \"" << side.name << "\" loaded as #" << index << ", from "
             << side.base;
  }
}

const SidecarWeapon *WeaponSidecar_Find(const std::string &name) {
  for (const SidecarWeapon &s : g_weapons)
    if (SameWeaponName(s.name, name))
      return &s;
  return nullptr;
}

const SidecarWeapon *WeaponSidecar_ByIndex(int index) {
  for (const SidecarWeapon &s : g_weapons)
    if (s.index == index)
      return &s;
  return nullptr;
}

const std::vector<SidecarWeapon> &WeaponSidecar_All() { return g_weapons; }

} // namespace tartarus
