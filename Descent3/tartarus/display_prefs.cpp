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

// Display-setting persistence rules. See display_prefs.h.

#include "display_prefs.h"

#include <algorithm>

namespace tartarus {

int ChooseResolutionIndex(const std::vector<ScreenSize> &modes, std::optional<ScreenSize> saved, int default_id) {
  if (modes.empty())
    return 0;
  if (saved) {
    auto it = std::find(modes.begin(), modes.end(), *saved);
    if (it != modes.end())
      return static_cast<int>(it - modes.begin());
  }
  return std::clamp(default_id, 0, static_cast<int>(modes.size()) - 1);
}

bool FullscreenToSave(bool now, bool stored_pref, bool arg_windowed, bool arg_fullscreen) {
  if (arg_windowed && !now)
    return stored_pref; // still windowed only because of -windowed
  if (arg_fullscreen && now)
    return stored_pref; // still fullscreen only because of -fullscreen
  return now;           // no flag, or the user changed it in Options this session
}

} // namespace tartarus
