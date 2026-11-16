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

// The precise frame clock's decisions (F-061, IMP-007) and the exact-rotation
// maths, without engine headers so they can be unit-tested. frame_clock.cpp
// and phys_exact.cpp apply them.

#pragma once

#include <cstdint>

namespace tartarus {

/// The frame time to use, in seconds. stock_s is the engine's measurement in
/// whole milliseconds, precise_s the microsecond one over the same interval.
/// The precise one is used when the two agree to within the millisecond
/// rounding (plus slack); otherwise the interval spans something the stock
/// clock leaves out (a pause), and the stock figure is kept. Never below
/// kMinFrametime unless stock itself is 0 and precise is unusable.
float RefineFrametime(float stock_s, float precise_s);

inline constexpr float kMinFrametime = 0.00005f; // 50 microseconds

/// How long to wait, in microseconds, for a frame started at start_us to last
/// 1/fps seconds, given the time now_us; 0 if no cap or already long enough.
int64_t FrameWaitUs(int64_t start_us, int64_t now_us, float fps);

/// The average turn speed over a frame for one axis, as PhysicsApplyConstRotForce()
/// moves it from v0 to v1 under a constant force with linear drag: the speed
/// tends to force/drag at rate drag/mass. With no drag or mass, or bad input,
/// the end speed v1 (stock's choice).
float RotationMeanSpeed(float v0, float v1, float force, float drag, float mass, float dt);

} // namespace tartarus
