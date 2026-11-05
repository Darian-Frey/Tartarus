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

// The ship's laser scanner (F-050): a few rays a frame reach surfaces near the
// ship, and the automap shows those brightly. Tartarus mode, single player.

#pragma once

namespace tartarus {

/// Registers the map_scan* cvars and commands. Called from Tartarus_Init().
void MapScan_Init();

/// Forgets every surface. Called when a level starts.
void MapScan_Reset();

/// Fires this frame's rays. Called from Frame().
void MapScan_Frame();

/// How bright the automap draws a surface: 1 scanned, dim if only seen. Always
/// 1 in Classic Mode and multiplayer, where the map is stock.
float MapScan_Brightness(int room, int face);

} // namespace tartarus
