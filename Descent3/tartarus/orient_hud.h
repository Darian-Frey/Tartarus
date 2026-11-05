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

// Orientation aids on the HUD (F-040 - F-042, F-046, D-017, SPEC.md §9): the
// flight HUD on the canopy, and the optional reticle up-tick, attitude ball and
// axis gizmo. CLASSIC_LOCKED, and off in Classic Mode.

#pragma once

namespace tartarus {

/// Registers the hud_aids, hud_flight*, hud_uptick*, hud_attitude* and
/// hud_gizmo* cvars. Called from Tartarus_Init().
void OrientHud_Init();

/// Draws the aids. Called from RenderOverlays().
void OrientHud_Render();

/// Draws the reticle at cx, cy, from upstream's slot masks. False in Classic
/// Mode or with the stock style: the caller draws the stock reticle.
bool OrientHud_RenderReticle(float cx, float cy, unsigned primary_exists, unsigned primary_ready, unsigned secondary_exists,
                             unsigned secondary_ready);

/// Draws the flight HUD. Called from RenderHUDFrame() just before the cockpit
/// model, which then covers it wherever the frame is: that puts it on the glass.
void OrientHud_RenderCanopy();

} // namespace tartarus
