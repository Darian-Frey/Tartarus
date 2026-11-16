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

// A timing probe for F-061 (D-012): the flighttest console command flies a
// fixed manoeuvre timed by game time (thrust, a turn, a pitch, then coasting)
// and logs where the ship ends up, so runs at different frame rates can be
// compared. Input at a phase boundary is weighted by the share of the frame
// inside the phase, so the comparison measures the simulation, not when the
// frames happen to fall. Diagnostics only: nothing runs unless asked, and
// never in Classic Mode or multiplayer.

#pragma once

struct game_controls;

namespace tartarus {

/// Registers the flighttest command. Called from Tartarus_Init().
void FlightTest_Init();

/// DoMovement() (Controls.cpp): replaces the movement input while a test runs.
void FlightTest_Apply(game_controls *controls);

/// The flight phases, in game seconds from the start, and the input in each.
struct FlightPhase {
  float start, end;
  float forward, heading, pitch;
};

/// The share of the frame [t, t + dt] that lies inside [start, end].
float PhaseShare(float t, float dt, float start, float end);

} // namespace tartarus
