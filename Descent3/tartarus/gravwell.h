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

// Gravity wells (F-021, D-009, SPEC.md §7, AV-006). A weapon with the
// TWF_GRAVWELL flag (the GravMissile of weapons.txt) leaves a well where it
// explodes. For gw_time seconds it pulls nearby robots, matter weapons and,
// within gw_self_radius, the player toward its centre through
// phys_apply_force(): walls stop them as usual (never PF_DESTINATION_POS,
// D-009). Wells are a Tartarus side list run from Frame(), not fireball
// objects. Nothing in Classic Mode or multiplayer.

#pragma once

#include "gravwell_core.h"
#include "vecmat.h"

struct object;

namespace tartarus {

/// Registers the gw_ cvars. Called from Tartarus_Init().
void Gravwell_Init();

/// Per frame, from Frame(): pulls and draws every live well.
void Gravwell_Frame();

/// A new level or a load: no wells carried over.
void Gravwell_Reset();

/// Hook in DoWeaponExploded() (WeaponFire.cpp): a TWF_GRAVWELL weapon leaves a well.
void Gravwell_WeaponExploded(const object *weapon_obj, const vector *point);

/// The gw_ settings now; the repulsor shares the mass ones (F-022).
WellSettings Gravwell_Settings();

/// Distance from centre to an object's surface, as the stock gravity field and
/// concussive force measure it (a polygon model by its wall sphere).
float Gravwell_SurfaceDistance(const object &o, const vector &centre);

} // namespace tartarus
