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

// Mouse flight modes (F-067, SPEC.md §13, D-013, AV-013): virtual joystick.
//
// The mouse controller hands each batch of mouse counts to a filter (one hook
// in ddio/sdlcontroller.cpp). While virtual-joystick mode is active the filter
// keeps the counts as an offset from centre and hands upstream zero, so
// neither upstream mouse path (mouselook or mouse-as-axis) turns the ship.
// DoMovement() then adds the curved offset to the pitch/heading rates, the
// same path a joystick axis takes, so the flight model is untouched.
// CLASSIC_LOCKED: Classic Mode always gets the upstream mouse.

#pragma once

struct game_controls;

namespace tartarus {

/// Registers the m_* cvars and installs the mouse filter. Called from
/// Tartarus_Init().
void MouseFlight_Init();

/// True when virtual-joystick mode is in charge of the mouse.
bool MouseFlight_Active();

/// Called from DoMovement() after the controller movement: adds the stick
/// output to `controls`.
void MouseFlight_Apply(game_controls *controls);

/// Per-frame reset checks (AV-013). Called from tartarus::Frame().
void MouseFlight_Frame();

/// True if `ctf_function` (a ctf*_BUTTON id) is bound to the recentre mouse
/// button while the virtual joystick is active: the button then only
/// recentres. DoControllerMovement-side callers zero that packet.
bool MouseFlight_ReservesButton(int ctf_function);

/// Centres the stick (recentre input, and every AV-013 trigger).
void MouseFlight_Recentre();

/// Offset indicator and dead-zone ring. Called from RenderOverlays().
void MouseFlight_Render();

} // namespace tartarus
