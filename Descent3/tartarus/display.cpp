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

// Display settings that survive restarts. See display.h.
//
// Upstream saves the resolution as an index ("RS_resolution") into a list that
// is rebuilt every launch from the display's modes (or a fallback list if SDL
// enumeration fails), so the same number can mean a different size next time
// (BUG-004). We also store "RS_width"/"RS_height" and restore by size.
// Upstream also saves the -windowed / -fullscreen override as the user's
// preference (BUG-005); we keep those flags session-only.

#include "display.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include "appdatabase.h"
#include "args.h"
#include "config.h"
#include "descent.h"
#include "classic.h"
#include "cvar.h"
#include "display_prefs.h"
#include "game.h"
#include "log.h"

extern float Render_FOV_setting; // config.cpp
extern float Render_FOV;         // GameLoop.cpp
extern int Tartarus_bar_fill_mode;          // renderer/HardwareOpenGL.cpp (TARTARUS hook)
extern float Tartarus_bar_fill_brightness;  // renderer/HardwareOpenGL.cpp (TARTARUS hook)

namespace tartarus {

namespace {
bool g_stored_fullscreen = true;
Cvar *r_fov = nullptr;
bool g_syncing_fov = false;

void SyncFovCvar() {
  if (r_fov == nullptr)
    return;
  g_syncing_fov = true;
  Cvars().Set("r_fov", std::to_string(Render_FOV_setting), CvarSource::Code);
  g_syncing_fov = false;
}
} // namespace

int Display_FovMin() { return IsClassic() ? static_cast<int>(D3_DEFAULT_FOV) : kFovAbsMin; }
int Display_FovMax() { return IsClassic() ? 90 : kFovAbsMax; }

void Display_SetFov(float degrees) {
  const float fov = std::clamp(std::round(degrees), static_cast<float>(Display_FovMin()),
                               static_cast<float>(Display_FovMax()));
  if (Render_FOV == Render_FOV_setting) // not zoomed or in a liquid effect: apply now
    Render_FOV = fov;
  Render_FOV_setting = fov;
  SyncFovCvar();
}

void Display_Init() {
  g_stored_fullscreen = Game_fullscreen;

  r_fov = Cvars().RegisterFloat("r_fov", D3_DEFAULT_FOV, kFovAbsMin, kFovAbsMax,
                                "Field of view in degrees (60-120; Classic Mode 72-90). Saved with the video settings");
  r_fov->AddChangeCallback([](const Cvar &cv) {
    if (!g_syncing_fov)
      Display_SetFov(cv.GetFloat());
  });
  // LoadGameSettings() clamped RS_fov to the widest range; apply this mode's.
  Display_SetFov(Render_FOV_setting);
  if (Cvar *classic = Cvars().Find("classic"))
    classic->AddChangeCallback([](const Cvar &) { Display_SetFov(Render_FOV_setting); });

  // Bars around frames narrower than the window (menus are 640x480): blurred fill (F-060).
  Cvar *fill = Cvars().RegisterInt("r_bar_fill", 1, 0, 2,
                                   "Bars around 4:3 screens: 0 black, 1 blurred copy in menus, 2 blurred copy always",
                                   CVAR_ARCHIVE);
  Cvar *bright = Cvars().RegisterFloat("r_bar_fill_brightness", 0.35f, 0.0f, 1.0f,
                                       "Brightness of the blurred bar fill (0 black, 1 full)", CVAR_ARCHIVE);
  fill->AddChangeCallback([](const Cvar &cv) { Tartarus_bar_fill_mode = cv.GetInt(); });
  bright->AddChangeCallback([](const Cvar &cv) { Tartarus_bar_fill_brightness = cv.GetFloat(); });
  Tartarus_bar_fill_mode = fill->GetInt();
  Tartarus_bar_fill_brightness = bright->GetFloat();
}

int Display_RestoreResolution(const std::vector<std::pair<int, int>> &modes, int default_id) {
  std::optional<ScreenSize> saved;
  int w = 0, h = 0;
  if (Database != nullptr && Database->read_int("RS_width", &w) && Database->read_int("RS_height", &h) && w > 0 &&
      h > 0) {
    saved = ScreenSize{w, h};
  }
  const int id = ChooseResolutionIndex(modes, saved, default_id);
  if (saved && (id < 0 || id >= static_cast<int>(modes.size()) || modes[id] != *saved)) {
    LOG_WARNING << "[tartarus] saved resolution " << w << "x" << h
                << " is not offered by this display; using the desktop resolution";
  } else if (saved) {
    LOG_INFO << "[tartarus] restored resolution " << w << "x" << h;
  }
  return id;
}

void Display_SaveSettings(int width, int height, bool fullscreen_now) {
  if (Database == nullptr)
    return;
  Database->write("RS_width", width);
  Database->write("RS_height", height);
  const bool windowed_arg = FindArgChar("-windowed", 'w') != 0;
  const bool fullscreen_arg = FindArgChar("-fullscreen", 'f') != 0;
  Database->write("RS_fullscreen",
                  static_cast<int>(FullscreenToSave(fullscreen_now, g_stored_fullscreen, windowed_arg, fullscreen_arg)));
}

} // namespace tartarus
