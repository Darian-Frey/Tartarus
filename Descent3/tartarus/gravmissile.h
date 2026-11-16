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

// The gravity missile's launcher (F-021, D-019): its own key (gm_fire_key, G),
// its own ammunition, refilled to gm_level_ammo at the start of each level
// (owner, 2026-11-15) and kept in a file beside each save (AV-003). It fires
// the GravMissile of weapons.txt (D-007); gravwell.cpp makes the well. Nothing
// in Classic Mode or multiplayer.

#pragma once

#include <filesystem>
#include <string>

namespace tartarus {

/// Registers the gm_ cvars. Called from Tartarus_Init().
void GravMissile_Init();

/// The fire key. True if it was used.
bool GravMissile_HandleKey(int key);

/// The ammunition readout, from RenderOverlays().
void GravMissile_Render();

/// A level has started: refill to gm_level_ammo; a new game first empties it.
void GravMissile_LevelStart();

/// Beside a save: the ammunition count.
void GravMissile_Save(const std::filesystem::path &save_path);
void GravMissile_Load(const std::filesystem::path &save_path);

/// The give cheat: true if name (lower case, no spaces) is a tactical weapon,
/// which is then filled to its maximum.
bool GravMissile_Give(const std::string &squashed_name);

/// give ammo / give all: fills it.
void GravMissile_Fill();

} // namespace tartarus
