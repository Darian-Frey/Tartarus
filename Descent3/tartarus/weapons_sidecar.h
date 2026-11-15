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

// Weapon sidecar loader (D-007, SPEC.md §5, AV-010). After the game's tables
// load, each block of weapons.txt becomes a new weapon: a copy of its stock
// base with the listed changes, in a free slot from AllocWeapon(). Stock
// weapons are never changed. The file is built into the game
// (data/tartarus/weapons.txt); weapons.txt in the writable folder, if there,
// is read instead. What the weapon struct has no place for (class, fire delay,
// ammunition, Tartarus flags) goes into a side table here (AV-003).

#pragma once

#include <cstdint>
#include <string>

namespace tartarus {

struct SidecarWeapon {
  int index = -1;          // in Weapons[]
  std::string name;
  std::string base;
  bool tactical = false;   // own key and ammunition (D-019)
  uint32_t flags = 0;      // TWF_* bits
  float fire_delay = 1.0f; // seconds between shots
  int ammo_pickup = 1;
  int ammo_max = 1;
};

/// Hook after mng_LoadTableFiles() in init.cpp: loads the sidecar weapons.
/// Registers the weaponinfo console command the first time.
void WeaponSidecar_Load();

/// A loaded sidecar weapon by name (any case) or by Weapons[] index; nullptr if none.
const SidecarWeapon *WeaponSidecar_Find(const std::string &name);
const SidecarWeapon *WeaponSidecar_ByIndex(int index);

} // namespace tartarus
