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

// Precise frame timing (F-061, IMP-007). The stock clock counts whole
// milliseconds: frame times are rounded, and the r_maxfps cap lands on
// 1000/n fps (144 gives about 166). With r_precise_timer on, Tartarus mode
// measures frames in microseconds and waits to the exact cap. Classic Mode
// keeps the stock clock.

#pragma once

namespace tartarus {

/// Registers r_precise_timer. Called from Tartarus_Init().
void FrameClock_Init();

/// CalcFrameTime() (GameLoop.cpp), after the stock measurement: refines it.
void FrameClock_Refine(float *frametime);

/// GameFrame() (GameLoop.cpp), before the stock limiter: waits out the cap.
void FrameClock_Limit();

} // namespace tartarus
