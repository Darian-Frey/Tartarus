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

// The repulsor (F-022, SPEC.md §7, D-009): a one-off shove on rp_key (V),
// centred on the ship, for rp_cost energy every rp_cooldown seconds. It pushes
// robots (not the Guide-Bot) out to rp_radius with the gravity well's shape
// and mass scaling, turns missiles closing on the ship round, and, near a wall,
// launches the ship off it. Every push goes through phys_apply_force() and
// nothing behind a wall is touched (AV-006). Nothing in Classic Mode or
// multiplayer.

#pragma once

namespace tartarus {

/// Registers the rp_ cvars. Called from Tartarus_Init().
void Repulsor_Init();

/// The repulsor key. True if it was used.
bool Repulsor_HandleKey(int key);

/// The recharge readout, from RenderOverlays().
void Repulsor_Render();

/// A new level or a load: ready at once.
void Repulsor_Reset();

} // namespace tartarus
