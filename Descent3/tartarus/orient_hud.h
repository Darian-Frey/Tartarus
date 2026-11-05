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

// Orientation aids on the HUD (F-040 - F-042, D-010, SPEC.md §9): the reticle
// up-tick, the attitude ball and the axis gizmo. CLASSIC_LOCKED, and off in
// Classic Mode.

#pragma once

namespace tartarus {

/// Registers the hud_uptick*, hud_attitude* and hud_gizmo* cvars. Called from
/// Tartarus_Init().
void OrientHud_Init();

/// Draws the aids. Called from RenderOverlays().
void OrientHud_Render();

} // namespace tartarus
