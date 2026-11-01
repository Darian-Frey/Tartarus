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

#include <optional>

#include "appdatabase.h"
#include "args.h"
#include "config.h"
#include "descent.h"
#include "display_prefs.h"
#include "log.h"

namespace tartarus {

namespace {
bool g_stored_fullscreen = true;
} // namespace

void Display_Init() { g_stored_fullscreen = Game_fullscreen; }

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
