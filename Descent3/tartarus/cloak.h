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

// Player-triggered cloak (F-020, SPEC.md §6, AV-007): C turns the engine's own
// invisibility on (MakeObjectInvisible), at an energy cost and drain, for at
// most cloak_max_time. Firing, bumping a robot or a big hit breaks it into a
// longer cooldown. Robots already treat a cloaked player as unseen
// (AIDetermineObjVisLevel). Tartarus mode, single player; nothing in Classic
// Mode (AV-001).

#pragma once

struct object;

namespace tartarus {

/// Registers the cloak_ cvars. Called from Tartarus_Init().
void Cloak_Init();

/// The cloak key (cloak_key). True if it was used.
bool Cloak_HandleKey(int key);

/// Per frame, from Frame(): drain, timeout, firing, cooldown, resets.
void Cloak_Frame();

/// The HUD gauge, from RenderOverlays().
void Cloak_Render();

/// A Tartarus weapon was fired (the gravity missile goes through
/// CreateAndFireWeapon(), which the battery check does not see): breaks the cloak.
void Cloak_WeaponFired();

/// Hook in ApplyDamageToPlayer() (damage.cpp): a hit of at least
/// cloak_break_damage breaks the cloak.
void Cloak_PlayerDamaged(const object *player, float damage);

/// Hook in collide_generic_and_player() (physics/collide.cpp): touching a robot
/// breaks the cloak, and with cloak_bump_alerts the robot knows where you are.
void Cloak_Bumped(object *robot, const object *player);

} // namespace tartarus
