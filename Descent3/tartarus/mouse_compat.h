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

// Menu-pointer scale shared with the UI library (BUG-006). Compatibility fix:
// active in Classic Mode too.
//
// Since upstream 5b18e22e the Linux mouse driver moves the pointer by the
// summed motion since the last read, which is far too fast for a pointer. The
// UI library compensates with a virtual mouse space 20x the screen and divides
// positions by 20 (kDefaultMouseScale in ui/UISystem.cpp). TelCom (briefings,
// ship select, goals) reads the driver directly, so it applies the same factor
// through this constant. Goes away when F-067 reworks the driver (BUG-006 (b)).

#pragma once

namespace tartarus {

/// Must equal kDefaultMouseScale in ui/UISystem.cpp.
inline constexpr int kUiMouseScale = 20;

} // namespace tartarus
