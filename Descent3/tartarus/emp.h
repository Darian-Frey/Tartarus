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

// The EMP mine (F-026, F-070, D-020). A weapons.txt weapon with TWF_EMP,
// dropped behind the ship by the tactical launcher, sits where it is dropped.
// After emp_arm_time it goes off when a robot comes within emp_trigger_radius
// of it (in sight), or when something hits it. The pulse does no damage: every
// robot within emp_radius in sight of it is stunned for emp_time
// (ai_react.cpp), bosses (emp_immune_mass) excepted. Nothing in Classic Mode
// or multiplayer.

#pragma once

#include "vecmat.h"

struct object;

namespace tartarus {

/// Registers the emp_ cvars. Called from Tartarus_Init().
void Emp_Init();

/// The launcher has dropped a mine.
void Emp_MineDropped(const object *mine);

/// Per frame, from Frame(): sets off armed mines with a robot near.
void Emp_Frame();

/// From Tartarus_WeaponExploded() (DoWeaponExploded hook): a TWF_EMP weapon went off.
void Emp_WeaponExploded(const object *weapon_obj, const vector *point);

/// A new level or a load: the mine list is rebuilt from the objects.
void Emp_Reset();

} // namespace tartarus
