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
#include <cstdint>
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

  /// The table as bytes for a .amap file, stamped with a fingerprint of the level.
  std::vector<uint8_t> Serialize(uint32_t fingerprint) const;
  /// Replaces the table from Serialize()'s bytes. False, leaving it empty, if
  /// they are damaged or from another level. Loaded surfaces are not "new".
  bool Deserialize(const std::vector<uint8_t> &bytes, uint32_t fingerprint);

  /// The map has opened: what was scanned since it last opened is now "new".
  void OpenMap();
  bool IsNew(int room, int face) const;

private:
  uint16_t Get(int room, int face) const;

  std::vector<std::vector<uint16_t>> rooms_; // 0 not scanned, otherwise the map opening it arrived before
  size_t total_ = 0;
  uint16_t generation_ = 1;
  uint16_t new_generation_ = 0;
};

/// Map symbols. Lines are in symbol radii, x right, y up.
enum MapSymbol { kSymbolKey, kSymbolDoor, kSymbolLockedDoor, kSymbolGoal, kSymbolPowerup, kSymbolMarker, kSymbolRing, kMapSymbols };

struct SymbolLine {
  float x1, y1, x2, y2;
};

std::vector<SymbolLine> MapSymbolLines(int symbol);

/// True if an object's name says it is a key: keys have no flag of their own.
bool IsKeyName(const char *name);

/// True for objects levels use as hidden triggers, such as "Invisiblepowerup",
/// which the map must not give away.
bool IsHiddenName(const char *name);

/// Whether a surface can be scanned from a point: within range, and facing it.
bool FaceInReach(const float centre[3], const float normal[3], const float from[3], float range);

} // namespace tartarus
