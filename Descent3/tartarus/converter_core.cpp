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

#include "converter_core.h"

#include <algorithm>

namespace tartarus {

ConvertResult ConvertStep(float source, float destination, float max, float rate, float ratio, float floor, float dt) {
  if (dt <= 0.0f || rate <= 0.0f || ratio <= 0.0f)
    return {source, destination};
  float take = rate * dt;
  take = std::min(take, source - floor);            // keep the floor
  take = std::min(take, (max - destination) / ratio); // only what fits
  if (take <= 0.0f)
    return {source, destination};
  return {source - take, destination + take * ratio};
}

} // namespace tartarus
