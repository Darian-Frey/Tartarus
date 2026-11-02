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

// Tartarus start-up. See tartarus.h.

#include "tartarus.h"

#include "args.h"
#include "cfile.h"
#include "binds.h"
#include "cheats.h"
#include "ddio.h"
#include "descent.h"
#include "game.h"
#include "gameloop.h"
#include "gamesequence.h"
#include "classic.h"
#include "console.h"
#include "display.h"
#include "graphics.h"
#include "mouse_flight.h"
#include "raw_mouse.h"
#include "small_views.h"
#include "cvar.h"
#include "log.h"
#include "version.h"

namespace tartarus {

namespace {
Cvar *con_pause = nullptr;
bool g_we_paused = false; // the console/cheat menu called PauseGame() and owes a ResumeGame()
} // namespace

void Tartarus_Init() {
  LOG_INFO << "[tartarus] " << BuildString() << ", " << UpstreamString();

  CvarPolicy policy;
  policy.is_classic = [] { return IsClassic(); };
  policy.cheat_allowed = [](const Cvar &) {
#if defined(_DEBUG)
    return true; // Debug builds treat every cheat as unlocked (SPEC.md §4)
#else
    return false; // no CHEAT-flagged cvars exist yet; map each to its cheat row (F-012) when one does
#endif
  };
  policy.log = [](const std::string &msg) { LOG_WARNING << "[tartarus] " << msg; };
  policy.config_path = cf_GetWritableBaseDirectory() / "tartarus.cfg";
  Cvars().SetPolicy(policy);

  if (!Cvars().LoadConfig()) {
    LOG_WARNING << "[tartarus] settings could not be read; using defaults";
  }
  LOG_INFO << "[tartarus] settings file " << policy.config_path;

  Classic_Init(FindArg("-classic") != 0, FindArg("-tartarus") != 0);
  LOG_INFO << "[tartarus] " << (IsClassic() ? "Classic Mode" : "Tartarus mode")
           << (IsClassicForcedByCommandLine() ? " (from the command line)" : "");

  Display_Init();
  Graphics_Init();
  Console_Init();
  Cheats_Init();
  Binds_Init();
  con_pause = Cvars().RegisterBool("con_pause", true,
                                   "Pause the game while the console or cheat menu is open (single-player)", CVAR_ARCHIVE);
  MouseFlight_Init();
  RawMouse_Init();
  SmallViews_Init();
}

bool SkipCinematics() {
  static const bool skip = FindArg("-skipcinematics") != 0;
  return skip;
}

bool SkipBriefing() {
  static const bool skip = FindArg("-skipbriefing") != 0;
  return skip;
}

namespace {

// Pause while the console or cheat menu is open (IMP-004): single-player, in a level only.
void UpdatePause() {
  const bool overlay = Console_IsOpen() || Cheats_MenuIsOpen();
  const bool want = overlay && con_pause != nullptr && con_pause->GetBool() && !(Game_mode & GM_MULTI) &&
                    GetFunctionMode() == GAME_MODE && Game_interface_mode == GAME_INTERFACE;
  if (want && !g_we_paused && !Game_paused) {
    PauseGame();
    g_we_paused = true;
  } else if (!want && g_we_paused) {
    g_we_paused = false;
    if (Game_paused)
      ResumeGame();
  }
}

} // namespace

bool HandleKey(int key) {
  const bool used = Console_HandleKey(key) || Cheats_HandleKey(key) || Binds_HandleKey(key);
  UpdatePause();
  return used;
}

void PausedFrame() {
  if (!g_we_paused)
    return; // paused by something else (pause key, menus): not ours to touch
  if (Descent != nullptr && Descent->active()) {
    int key;
    while ((key = ddio_KeyInKey()) != 0)
      HandleKey(key); // keys the overlays do not use are dropped, as while typing unpaused
  }
  UpdatePause();
}

void Frame() {
  UpdatePause();
  Cheats_Frame();
  MouseFlight_Frame();
  RawMouse_Frame();
}

float TimeScale() { return Cheats_TimeScale(); }

void RenderOverlays() {
  MouseFlight_Render();
  Cheats_Render();
  Console_Render();
}

} // namespace tartarus
