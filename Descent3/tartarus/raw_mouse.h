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

// Raw mouse input (F-062, SPEC.md §11). SDL3's relative mode is already
// unaccelerated; what upstream adds is 20 Hz sampling: the controller reads
// the mouse at most every 50 ms (MOUSE_POLLING_TIME) and reuses the reading
// on the frames between, so movement reaches the ship late and in steps.
// m_raw reads it every frame for the stock mouse modes (off by default until
// m_sensitivity is tuned: per frame the same movement turns ~3-4x less); the
// virtual joystick (F-067) is always read every frame. m_sensitivity scales
// the counts the stock mouse paths see; m_accel switches SDL's system
// acceleration curve on (off by default, as upstream). CLASSIC_LOCKED;
// single-player only (D-008).

#pragma once

namespace tartarus {

void RawMouse_Init();

/// Per-frame: tells the controller whether to sample the mouse every frame.
void RawMouse_Frame();

/// Scales one batch of counts for the stock mouse paths (not the virtual
/// joystick, which has its own speed). Called from the mouse filter.
void RawMouse_ScaleCounts(int *dx, int *dy);

/// Pure helper: scales integer counts by `scale`, carrying the fraction in
/// `*remainder` so slow movement is not lost to rounding.
int ScaleCounts(int counts, float scale, float *remainder);

} // namespace tartarus
