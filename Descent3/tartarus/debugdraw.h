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

// Debug overlays (F-013, SPEC.md §3): world axes, object labels and collision
// spheres on the main view, for tuning the Tartarus weapons. The r_show*
// cvars are cheats, unlocked by HOURGLASS (Debug builds: always); using one
// marks the pilot a cheater. Nothing in Classic Mode or multiplayer, and no
// work at all while they are off (AV-009).

#pragma once

namespace tartarus {

/// Registers the r_show* cvars. Called from Tartarus_Init().
void DebugDraw_Init();

/// Draws whichever overlays are on. Called from RenderOverlays().
void DebugDraw_Render();

} // namespace tartarus
