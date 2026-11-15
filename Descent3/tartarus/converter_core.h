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

// Energy <-> shield converter arithmetic (F-030, SPEC.md §8), without engine
// headers so it can be unit-tested.

#pragma once

namespace tartarus {

struct ConvertResult {
  float source;
  float destination;
};

/// One frame of conversion: takes rate * dt from source (never below floor),
/// adds that times ratio to destination (never above max), and takes only what
/// the destination has room for. Nothing changes if dt, rate or ratio is not
/// positive.
ConvertResult ConvertStep(float source, float destination, float max, float rate, float ratio, float floor, float dt);

} // namespace tartarus
