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

// Orientation maths (F-040, SPEC.md §9). Pure standard C++, unit-tested; the
// drawing is in orient_hud.cpp.

#pragma once

namespace tartarus {

struct UpTick {
  float angle = 0; // radians from the top of the screen, clockwise
  float alpha = 0; // 0 hidden, 1 full
};

/// up_right and up_up are world up measured along the ship's right and up
/// axes. The tick fades in between fade_deg and three times that from upright,
/// and fades out when up points along the view, where its direction means
/// nothing.
UpTick ComputeUpTick(float up_right, float up_up, float fade_deg);

} // namespace tartarus
