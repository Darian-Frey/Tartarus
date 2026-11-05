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

#include <filesystem>

#include "grdefs.h"
#include "vecmat_external.h"

struct g3Point;

namespace tartarus {

/// Registers the map_scan* cvars and commands. Called from Tartarus_Init().
void MapScan_Init();

/// Forgets every surface. Called when a level starts.
void MapScan_Reset();

/// Fires this frame's rays. Called from Frame().
void MapScan_Frame();

/// Counts a whole room as scanned: map pickups, level scripts and cheats that
/// reveal the map would otherwise show nothing.
void MapScan_RevealRoom(int room);

/// Saves the scan beside a saved game, as upstream's .cnt, so the save format
/// is untouched (AV-003). In Classic Mode it removes any old file instead, which
/// would otherwise be paired with the wrong save.
void MapScan_Save(const std::filesystem::path &save_path);

/// Replaces the scan with the one beside a saved game, or with nothing.
void MapScan_Load(const std::filesystem::path &save_path);

/// The automap has opened: what was scanned since last time flashes.
void MapScan_MapOpened();

/// Draws a surface in the scanner's look: fills it now and keeps its edges for
/// MapScan_FinishRooms(). points are the room's rotated vertices. False in
/// Classic Mode and multiplayer, and outdoors, where the caller draws it stock.
bool MapScan_DrawFace(int room, int face, g3Point *points, const vector &eye);

/// Draws the kept edges, after every surface is in the depth buffer, then the
/// ship and the symbols, which face the camera.
void MapScan_FinishRooms(const matrix &view);

/// Draws the terrain as a wire grid. False in Classic Mode and multiplayer,
/// where the caller draws it stock.
bool MapScan_DrawTerrain(const vector &eye);

/// Runs the map on its own full-resolution deck screen. False in Classic Mode
/// and multiplayer, where the caller opens it in TelCom.
bool MapScan_RunMap();

/// True if the scanner map draws the ship, in place of the stock model.
bool MapScan_DrawsShip();

/// True if openings to unknown rooms stay blank, rather than the stock pulse.
bool MapScan_HidesOpenings();

/// How far behind the ship the map opens: stock, or further for the scanner map.
float MapScan_CameraBack(float stock);

/// The map's background: black for a scan display, otherwise the stock colour.
ddgr_color MapScan_Background(ddgr_color stock);

} // namespace tartarus
