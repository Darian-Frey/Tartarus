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

#include "ai_react.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "AIMain.h"
#include "aistruct.h"
#include "cheats.h"
#include "classic.h"
#include "console.h"
#include "cvar.h"
#include "fireball.h"
#include "game.h"
#include "log.h"
#include "object.h"
#include "objinfo.h"
#include "player.h"
#include "viseffect.h"

namespace tartarus {

namespace {

Cvar *ai_decloak_noise = nullptr;

struct Stun {
  int handle;
  float until; // Gametime
  float next_spark;
};

std::vector<Stun> g_stuns;

bool Usable() { return !IsClassic() && !(Game_mode & GM_MULTI); }

object *Live(int handle) {
  object *o = ObjGet(handle);
  return o != nullptr && o->type == OBJ_ROBOT && o->control_type == CT_AI && o->ai_info != nullptr ? o : nullptr;
}

std::vector<Stun>::iterator Find(int handle) {
  return std::find_if(g_stuns.begin(), g_stuns.end(), [handle](const Stun &s) { return s.handle == handle; });
}

void Wake(object *o) {
  if (o != nullptr)
    o->ai_info->flags &= ~AIF_DISABLED;
}

std::filesystem::path StatePath(std::filesystem::path save_path) {
  save_path.replace_extension(".tstate");
  return save_path;
}

void StunCommand(const CommandArgs &a, ConsoleCore &c) {
  if (!Usable() || Player_object == nullptr) {
    c.Print("stun: needs a single-player level in Tartarus mode");
    return;
  }
  const float seconds = a.size() > 1 ? std::strtof(a[1].c_str(), nullptr) : 5.0f;
  const float radius = a.size() > 2 ? std::strtof(a[2].c_str(), nullptr) : 150.0f;
  Cheats_MarkCheater();
  int n = 0;
  for (int i = 0; i <= Highest_object_index; ++i) {
    object &o = Objects[i];
    if (o.type == OBJ_ROBOT && vm_VectorDistance(&o.pos, &Player_object->pos) <= radius && AiReact_Stun(&o, seconds))
      ++n;
  }
  c.Print("stun: " + std::to_string(n) + " robot(s) for " + std::to_string(seconds).substr(0, 4) + " s");
  LOG_INFO.printf("[tartarus] stun: %d robot(s) within %.0f for %.1f s", n, radius, seconds);
}

} // namespace

void AiReact_Init() {
  ai_decloak_noise = Cvars().RegisterFloat("ai_decloak_noise", 120.0f, 0.0f, 1000.0f,
                                           "Robots within this distance hear the cloak drop and come to look; 0 off",
                                           CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED);
  Console().AddCommand({"stun", "stun [seconds] [radius]", "Stun robots near you (default 5 s, 150 units)", CMD_CHEAT,
                        [](const CommandArgs &a, ConsoleCore &c) { StunCommand(a, c); }});
}

bool AiReact_Stun(object *robot, float seconds) {
  if (!Usable() || robot == nullptr || !(seconds > 0.0f) || Live(robot->handle) != robot || IS_GUIDEBOT(robot) ||
      (robot->flags & (OF_DEAD | OF_DYING | OF_DESTROYED)))
    return false;
  auto it = Find(robot->handle);
  if (it == g_stuns.end()) {
    if (robot->ai_info->flags & AIF_DISABLED)
      return false; // a script switched it off: not ours to wake
    robot->ai_info->flags |= AIF_DISABLED;
    // The AI no longer sets its thrust, so stop it pushing.
    robot->mtype.phys_info.thrust = vector{};
    robot->mtype.phys_info.rotthrust = vector{};
    g_stuns.push_back({robot->handle, Gametime + seconds, Gametime});
  } else {
    it->until = std::max(it->until, Gametime + seconds);
  }
  return true;
}

bool AiReact_IsStunned(const object *robot) { return robot != nullptr && Find(robot->handle) != g_stuns.end(); }

void AiReact_Frame() {
  if (g_stuns.empty())
    return;
  if (!Usable()) { // Classic switched on mid-stun: wake everything at once
    for (const Stun &s : g_stuns)
      Wake(Live(s.handle));
    g_stuns.clear();
    return;
  }
  for (Stun &s : g_stuns) {
    object *o = Live(s.handle);
    if (o == nullptr) {
      s.until = 0.0f;
      continue;
    }
    if (Gametime >= s.until) {
      Wake(o);
      LOG_INFO.printf("[tartarus] robot %d wakes", OBJNUM(o));
      continue;
    }
    if (Gametime >= s.next_spark) {
      CreateRandomSparks(3, &o->pos, o->roomnum, -1, 1);
      s.next_spark = Gametime + 0.35f;
    }
  }
  g_stuns.erase(std::remove_if(g_stuns.begin(), g_stuns.end(), [](const Stun &s) { return Gametime >= s.until; }),
                g_stuns.end());
}

void AiReact_Decloaked() {
  if (!Usable() || Player_object == nullptr || ai_decloak_noise == nullptr)
    return;
  const float dist = ai_decloak_noise->GetFloat();
  if (!(dist > 0.0f))
    return;
  ain_hear hear;
  hear.f_directly_player = true;
  hear.hostile_level = 0.5f;
  hear.curiosity_level = 1.0f;
  hear.max_dist = dist;
  AINotify(Player_object, AIN_HEAR_NOISE, &hear);
}

void AiReact_Reset() { g_stuns.clear(); }

void AiReact_Save(const std::filesystem::path &save_path) {
  std::ofstream out(StatePath(save_path), std::ios::app);
  for (const Stun &s : g_stuns)
    out << "stun " << s.handle << " " << std::max(0.0f, s.until - Gametime) << "\n";
}

void AiReact_Load(const std::filesystem::path &save_path) {
  g_stuns.clear();
  std::ifstream in(StatePath(save_path));
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream words(line);
    std::string key;
    int handle = 0;
    float left = 0.0f;
    if (words >> key >> handle >> left && key == "stun") {
      object *o = Live(handle);
      if (o != nullptr && left > 0.0f) // the save kept AIF_DISABLED set; we wake it on time
        g_stuns.push_back({handle, Gametime + left, Gametime});
    }
  }
  if (!g_stuns.empty())
    LOG_INFO.printf("[tartarus] %d stunned robot(s) restored from the save", static_cast<int>(g_stuns.size()));
}

} // namespace tartarus
