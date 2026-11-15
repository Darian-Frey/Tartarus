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

// The weapon sidecar's text format (D-007, SPEC.md §5), parsed without engine
// headers so it can be unit-tested. weapons_sidecar.cpp turns the result into
// game weapons.
//
//   weapon "GravMissile" {      // a block per weapon
//     base   "Concussion"       // stock weapon copied first (required)
//     class  tactical           // own key and ammunition (D-019)
//     speed  70                 // any other key: name, then values
//     flags  TWF_GRAVWELL
//   }
//
// Comments run from // or # to the end of the line. Values are bare words or
// "quoted strings".

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tartarus {

struct SidecarKey {
  std::string name;
  std::vector<std::string> values;
  int line = 0;
};

struct SidecarBlock {
  std::string name;
  int line = 0;
  std::vector<SidecarKey> keys;

  /// The first key with this name, or nullptr.
  const SidecarKey *Find(const std::string &key) const;
};

struct SidecarParse {
  std::vector<SidecarBlock> weapons;
  std::vector<std::string> errors; // "line N: what is wrong"
};

/// Parses the whole file. Blocks with errors are left out; the rest still load.
SidecarParse ParseSidecar(const std::string &text);

/// Tartarus weapon flags (SPEC §5), kept in a side table, never in weapon.flags.
enum TartarusWeaponFlag : uint32_t {
  TWF_GRAVWELL = 1u << 0,
  TWF_FLAK = 1u << 1,
  TWF_RICOCHET = 1u << 2,
  TWF_EMP = 1u << 3,
  TWF_DECOY = 1u << 4,
  TWF_TETHER = 1u << 5,
};

/// "TWF_GRAVWELL" to its bit, or 0 for an unknown name.
uint32_t TartarusFlagBit(const std::string &name);

/// Checks a parsed file against the names already in the game (AV-010): every
/// weapon needs a base that exists, a supported class, and a name not taken by
/// the game or an earlier block. Returns the problems; blocks with none are safe.
std::vector<std::string> CheckSidecar(const SidecarParse &parse, const std::vector<std::string> &existing_names);

/// Case-insensitive, as the engine compares weapon names (stricmp).
bool SameWeaponName(const std::string &a, const std::string &b);

/// A value as a number; false if it is not one.
bool SidecarNumber(const std::string &value, float *out);

} // namespace tartarus
