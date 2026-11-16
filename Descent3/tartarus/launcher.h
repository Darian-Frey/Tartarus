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

// The tactical launcher (D-020, D-019): every `class tactical` weapon of
// weapons.txt, outside the ten secondary slots. gm_fire_key (G) fires the
// selected one and gm_cycle_key (Y) selects the next one you carry. A weapon
// with launch speed 0 (the EMP mine) is dropped behind the ship, the rest fly
// ahead. Ammunition is a Tartarus count per weapon, refilled at each level
// start (gm_level_ammo for the gravity missile, emp_level_ammo for the EMP
// mine) and kept in the .tstate file beside each save (AV-003). Nothing in
// Classic Mode or multiplayer.

#pragma once

#include <filesystem>
#include <string>

namespace tartarus {

/// Registers the gm_ cvars. Called from Tartarus_Init().
void Launcher_Init();

/// The fire and cycle keys. True if one was used.
bool Launcher_HandleKey(int key);

/// The readout under the reticle, from RenderOverlays().
void Launcher_Render();

/// A level has started: refill each weapon to its level amount; a new game first empties them.
void Launcher_LevelStart();

/// Beside a save: the counts and the selected weapon (writes the .tstate file).
void Launcher_Save(const std::filesystem::path &save_path);
void Launcher_Load(const std::filesystem::path &save_path);

/// The give cheat: true if name (lower case, no spaces) is a tactical weapon, which is then filled.
bool Launcher_Give(const std::string &squashed_name);

/// give ammo / give all and the all-weapons cheats: fills every tactical weapon. Nothing in Classic Mode.
void Launcher_Fill();

} // namespace tartarus
