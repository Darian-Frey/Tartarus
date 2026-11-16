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

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include "../frame_clock_core.h"

using tartarus::FrameWaitUs;
using tartarus::kMinFrametime;
using tartarus::RefineFrametime;

TEST(FrameClock, PreciseTimeReplacesTheRoundedOne) {
  EXPECT_FLOAT_EQ(RefineFrametime(0.006f, 0.006944f), 0.006944f); // 144 fps measured as 6 ms
  EXPECT_FLOAT_EQ(RefineFrametime(0.001f, 0.0016f), 0.0016f);
}

TEST(FrameClock, NeverZero) {
  EXPECT_FLOAT_EQ(RefineFrametime(0.0f, 0.0004f), 0.0004f); // a sub-millisecond frame the stock clock calls 0
  EXPECT_FLOAT_EQ(RefineFrametime(0.0f, 0.0f), kMinFrametime);
}

TEST(FrameClock, KeepsStockAcrossAPause) {
  EXPECT_FLOAT_EQ(RefineFrametime(0.016f, 5.3f), 0.016f); // the stock clock leaves the pause out
  EXPECT_FLOAT_EQ(RefineFrametime(0.016f, std::numeric_limits<float>::quiet_NaN()), 0.016f);
  EXPECT_FLOAT_EQ(RefineFrametime(0.016f, -0.001f), 0.016f);
}

TEST(FrameClock, WaitsToTheExactCap) {
  EXPECT_EQ(FrameWaitUs(1000, 1000, 144.0f), 6944);
  EXPECT_EQ(FrameWaitUs(1000, 5000, 144.0f), 2944);
  EXPECT_EQ(FrameWaitUs(1000, 9000, 144.0f), 0);
  EXPECT_EQ(FrameWaitUs(1000, 1000, 0.0f), 0); // no cap
  EXPECT_EQ(FrameWaitUs(1000, 1000, std::numeric_limits<float>::infinity()), 0);
}

TEST(ExactRotation, MeanSpeedOfAFrameUnderDrag) {
  using tartarus::RotationMeanSpeed;
  // From rest toward force / drag = 100 at rate drag / mass = 2: over 0.5 s,
  // v1 = 100 (1 - e^-1); the mean is 100 (1 - (1 - e^-1) / 1).
  const float v1 = 100.0f * (1.0f - std::exp(-1.0f));
  EXPECT_NEAR(RotationMeanSpeed(0.0f, v1, 200.0f, 2.0f, 1.0f, 0.5f), 100.0f * std::exp(-1.0f), 1e-3f);
  EXPECT_FLOAT_EQ(RotationMeanSpeed(50.0f, 50.0f, 100.0f, 2.0f, 1.0f, 0.016f), 50.0f); // steady: the same
  EXPECT_FLOAT_EQ(RotationMeanSpeed(10.0f, 20.0f, 100.0f, 0.0f, 1.0f, 0.016f), 20.0f); // no drag: stock
}

TEST(ExactRotation, SameTurnHoweverTheTimeIsSplit) {
  using tartarus::RotationMeanSpeed;
  auto turn = [](int frames) {
    const float dt = 1.0f / frames, k = 2.0f, terminal = 100.0f;
    double angle = 0.0, v = 0.0;
    for (int i = 0; i < frames; ++i) {
      const double v1 = terminal + (v - terminal) * std::exp(-k * dt);
      angle += RotationMeanSpeed(static_cast<float>(v), static_cast<float>(v1), 200.0f, 2.0f, 1.0f, dt) * dt;
      v = v1;
    }
    return angle;
  };
  EXPECT_NEAR(turn(30), turn(500), 1e-2);
}
