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

#include "quicksave.h"

#include <cstdio>
#include <optional>
#include <string>
#include <system_error>

#include "binds.h"
#include "classic.h"
#include "cvar.h"
#include "deck_save.h"
#include "descent.h"
#include "game.h"
#include "gamecinematics.h"
#include "gameloop.h"
#include "gamesave.h"
#include "gamesequence.h"
#include "hud.h"
#include "log.h"
#include "Mission.h"
#include "mouse_flight.h"
#include "player.h"
#include "stringtable.h"

extern bool Menu_interface_mode; // GameLoop.cpp

namespace tartarus {

namespace {

Cvar *g_quicksave_key = nullptr;
Cvar *g_quickload_key = nullptr;
Cvar *g_checkpoints = nullptr;

constexpr float kAutosaveDelay = 1.0f; // seconds of flight before the autosave, so its picture shows the level

bool g_autosave_due = false;
float g_flown = 0.0f;

bool Usable() {
  return !IsClassic() && !(Game_mode & GM_MULTI) && GetFunctionMode() == GAME_MODE && !Cinematic_inuse &&
         Player_object != nullptr && Player_object->type == OBJ_PLAYER;
}

bool Alive() { return !(Players[Player_num].flags & (PLAYER_FLAGS_DYING | PLAYER_FLAGS_DEAD)); }

// The level's name, cut to fit; the Load screen tags the row QUICK or AUTO.
std::string Description() {
  std::string desc = Level_info.name[0] ? Level_info.name : "Level";
  if (desc.size() > GAMESAVE_DESCLEN)
    desc.resize(GAMESAVE_DESCLEN);
  return desc;
}

bool Save(int slot, const char *what) {
  std::error_code ec;
  std::filesystem::create_directories(SaveDeck_SlotPath(slot).parent_path(), ec);
  const std::string desc = Description();
  if (!SaveGameState(SaveDeck_SlotPath(slot), desc.c_str())) {
    LOG_WARNING.printf("[tartarus] %s failed", what);
    return false;
  }
  LOG_INFO.printf("[tartarus] %s: %s", what, desc.c_str());
  return true;
}

bool Is(const Cvar *bind, int key) {
  const std::optional<int> bound = Binds_ParseKey(bind->GetString());
  return bound.has_value() && key == *bound;
}

} // namespace

void Quicksave_Init() {
  const uint32_t flags = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  CvarRegistry &cv = Cvars();
  g_quicksave_key = cv.RegisterString("g_quicksave_key", "f9", "Quicksave: saves at once to its own file", flags);
  g_quickload_key = cv.RegisterString("g_quickload_key", "f6", "Quickload: loads the quicksave at once", flags);
  g_checkpoints = cv.RegisterBool("g_checkpoints", true, "Autosave a moment after each level starts", flags);
}

bool Quicksave_HandleKey(int key) {
  const bool save = Is(g_quicksave_key, key);
  const bool load = !save && Is(g_quickload_key, key);
  if ((!save && !load) || !Usable())
    return false;
  if (save && !Flight_ContextActive())
    return false;
  // Quickload also works while the ship is blowing up, when it is most wanted.
  if (load && (Game_paused || Menu_interface_mode || Game_interface_mode != GAME_INTERFACE))
    return false;
  if (save) {
    if (!Alive())
      AddHUDMessage("Cannot quicksave now");
    else if (Save(kQuickSlot, "Quicksave"))
      AddHUDMessage("Quicksaved");
    else
      AddHUDMessage("%s", TXT_SAVEGAMEFAILED);
    return true;
  }
  std::error_code ec;
  char desc[GAMESAVE_DESCLEN + 1];
  const std::filesystem::path path = SaveDeck_SlotPath(kQuickSlot);
  if (!std::filesystem::exists(path, ec) || !GetGameStateInfo(path, desc)) {
    AddHUDMessage("No quicksave yet");
    return true;
  }
  SaveDeck_RequestLoad(kQuickSlot); // the load screen takes it without showing the list
  Game_interface_mode = GAME_LOAD_INTERFACE;
  return true;
}

void Quicksave_LevelStart() {
  g_autosave_due = true;
  g_flown = 0.0f;
}

void Quicksave_Loaded() { g_autosave_due = false; }

void Quicksave_Frame() {
  if (!g_autosave_due)
    return;
  if (!g_checkpoints->GetBool() || !Usable()) {
    if (IsClassic() || (Game_mode & GM_MULTI))
      g_autosave_due = false;
    return;
  }
  if (!Flight_ContextActive() || !Alive())
    return;
  g_flown += Frametime;
  if (g_flown < kAutosaveDelay)
    return;
  g_autosave_due = false;
  if (Save(kAutoSlot, "Autosave"))
    AddHUDMessage("Autosaved");
}

} // namespace tartarus
