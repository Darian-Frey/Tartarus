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

// Which surfaces the ship's scanner has reached (F-050). Pure standard C++,
// unit-tested; the rays are in map_scan.cpp. A side table: no upstream struct
// changes (AV-003).

#pragma once

#include <cstddef>
#include <vector>

namespace tartarus {

class ScanTable {
public:
  void Clear();
  /// True the first time this surface is marked.
  bool Mark(int room, int face);
  bool IsScanned(int room, int face) const;
  size_t ScannedInRoom(int room) const;
  size_t Total() const { return total_; }

private:
  std::vector<std::vector<bool>> rooms_;
  size_t total_ = 0;
};

/// Whether a surface can be scanned from a point: within range, and facing it.
bool FaceInReach(const float centre[3], const float normal[3], const float from[3], float range);

} // namespace tartarus
