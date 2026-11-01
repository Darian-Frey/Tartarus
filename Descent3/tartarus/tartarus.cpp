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
#include "cheats.h"
#include "classic.h"
#include "console.h"
#include "display.h"
#include "cvar.h"
#include "log.h"
#include "version.h"

namespace tartarus {

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
  Console_Init();
  Cheats_Init();
}

bool HandleKey(int key) { return Console_HandleKey(key) || Cheats_HandleKey(key); }

void Frame() { Cheats_Frame(); }

float TimeScale() { return Cheats_TimeScale(); }

void RenderOverlays() {
  Cheats_Render();
  Console_Render();
}

} // namespace tartarus
