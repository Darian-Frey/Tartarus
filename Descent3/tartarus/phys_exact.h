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

// Exact per-frame rotation (F-061, D-012). Stock PhysicsDoSimRot() turns an
// object each frame by (int16_t)(rotvel * frame_time) angle units, dropping
// the fraction: on average half a unit (1/131072 of a turn) is lost per frame
// and any turn slower than one unit a frame does not happen at all, so the
// same input turns the ship less the higher the frame rate (1.8 degrees over a
// short manoeuvre at 530 fps against 30, measured with flighttest). Stock
// also turns by the end-of-frame speed for the whole frame, which overshoots
// while the speed changes. With phys_exact_rot on, the frame's rotation is
// rebuilt from the unrounded angle of the average speed (RotationMeanSpeed()). The bank a turn gives (turnroll) is limited to a whole number of
// angle units a frame; the dropped fraction is carried to the next frame in a
// side table (AV-003). Off in Classic Mode and multiplayer.

#pragma once

#include "vecmat.h"

struct object;

namespace tartarus {

/// Registers phys_exact_rot. Called from Tartarus_Init().
void PhysExact_Init();

/// PhysicsDoSimRot() (physics.cpp), after the stock rotation matrix is built:
/// replaces it with the exact one when enabled: the frame's turn from the
/// average turn speed over the frame (rotvel0 at the start, rotvel1 at the
/// end, rotthrust the force between), not the end speed for the whole frame.
void PhysExact_Rotation(const object &obj, const vector &rotvel0, const vector &rotvel1, const vector &rotthrust,
                        float frame_time, matrix *rotmat);

/// set_object_turnroll() (physics.cpp): the frame's turn-bank step. exact is
/// max_turnroll_rate * Frametime, stock is that rounded down; returns stock
/// unless enabled, otherwise exact plus the fraction carried from the object's
/// last frame, rounded down, keeping the new fraction.
int PhysExact_TurnrollStep(const object *obj, float exact, int stock);

} // namespace tartarus
