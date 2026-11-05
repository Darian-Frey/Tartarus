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

// Scanner table. See scan_core.h.

#include "scan_core.h"

namespace tartarus {

void ScanTable::Clear() {
  rooms_.clear();
  total_ = 0;
  generation_ = 1;
  new_generation_ = 0;
}

uint16_t ScanTable::Get(int room, int face) const {
  if (room < 0 || face < 0 || static_cast<size_t>(room) >= rooms_.size())
    return 0;
  const std::vector<uint16_t> &faces = rooms_[static_cast<size_t>(room)];
  return static_cast<size_t>(face) < faces.size() ? faces[static_cast<size_t>(face)] : 0;
}

bool ScanTable::Mark(int room, int face) {
  if (room < 0 || face < 0)
    return false;
  if (static_cast<size_t>(room) >= rooms_.size())
    rooms_.resize(static_cast<size_t>(room) + 1);
  std::vector<uint16_t> &faces = rooms_[static_cast<size_t>(room)];
  if (static_cast<size_t>(face) >= faces.size())
    faces.resize(static_cast<size_t>(face) + 1, 0);
  if (faces[static_cast<size_t>(face)] != 0)
    return false;
  faces[static_cast<size_t>(face)] = generation_;
  ++total_;
  return true;
}

bool ScanTable::IsScanned(int room, int face) const { return Get(room, face) != 0; }

size_t ScanTable::ScannedInRoom(int room) const {
  if (room < 0 || static_cast<size_t>(room) >= rooms_.size())
    return 0;
  size_t count = 0;
  for (uint16_t generation : rooms_[static_cast<size_t>(room)])
    count += generation != 0 ? 1 : 0;
  return count;
}

void ScanTable::OpenMap() {
  new_generation_ = generation_;
  ++generation_;
}

bool ScanTable::IsNew(int room, int face) const {
  const uint16_t generation = Get(room, face);
  return generation != 0 && generation == new_generation_;
}

bool FaceInReach(const float centre[3], const float normal[3], const float from[3], float range) {
  const float dx = centre[0] - from[0], dy = centre[1] - from[1], dz = centre[2] - from[2];
  if (dx * dx + dy * dy + dz * dz > range * range)
    return false;
  return dx * normal[0] + dy * normal[1] + dz * normal[2] < 0.0f; // its front is toward us
}

} // namespace tartarus
