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

// Pure part of raw_mouse (unit-tested). See raw_mouse.h.

#include <cmath>

#include "raw_mouse.h"

namespace tartarus {

int ScaleCounts(int counts, float scale, float *remainder) {
  if (!std::isfinite(scale) || scale < 0.0f)
    scale = 1.0f;
  const float exact = counts * scale + *remainder;
  const float whole = std::trunc(exact);
  *remainder = exact - whole;
  return static_cast<int>(whole);
}

} // namespace tartarus
