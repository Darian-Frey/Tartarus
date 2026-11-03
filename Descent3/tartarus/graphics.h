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

// Graphics options (F-068, SPEC.md §11): VSync (fixes BUG-010), frame-rate
// cap, trilinear and anisotropic filtering, depth-buffer precision, MSAA. Display
// options: available in Classic Mode too (D-013). The renderer reads them
// through Tartarus_* globals that a pre-flip callback refreshes every frame.

#pragma once

namespace tartarus {

/// Registers the r_* graphics cvars and installs the renderer callback.
/// Called from Tartarus_Init().
void Graphics_Init();

/// Asks for the game frame to be rebuilt at the next frame (as r_restart).
void Graphics_RequestRestart();

/// Per-frame (from tartarus::Frame()): carries out a pending r_restart.
void Graphics_Frame();

} // namespace tartarus
