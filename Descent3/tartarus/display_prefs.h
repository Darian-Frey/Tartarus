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

// Display-setting persistence rules (BUG-004, BUG-005). Pure standard C++;
// the engine glue is display.cpp.

#pragma once

#include <optional>
#include <utility>
#include <vector>

namespace tartarus {

using ScreenSize = std::pair<int, int>; // width, height

/// Index of `saved` in `modes`, or `default_id` (clamped into range) when there
/// is no saved size or this display does not offer it.
int ChooseResolutionIndex(const std::vector<ScreenSize> &modes, std::optional<ScreenSize> saved, int default_id);

/// The fullscreen value to write to the settings. -windowed / -fullscreen are
/// session-only: if the current value is still what the flag forced, keep the
/// stored preference; if the user changed it in Options, save the change.
bool FullscreenToSave(bool now, bool stored_pref, bool arg_windowed, bool arg_fullscreen);

} // namespace tartarus
