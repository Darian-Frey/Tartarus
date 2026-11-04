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

#include <gtest/gtest.h>

#include <cmath>

#include "../orient_core.h"

using tartarus::ComputeUpTick;

namespace {

constexpr float kPi = 3.14159265f;

// World up along the ship's axes after a roll, with no pitch.
tartarus::UpTick Rolled(float degrees, float fade = 8.0f) {
  const float r = degrees * kPi / 180.0f;
  return ComputeUpTick(-std::sin(r), std::cos(r), fade);
}

} // namespace

TEST(OrientCore, HiddenWhenUpright) { EXPECT_EQ(Rolled(0).alpha, 0.0f); }

TEST(OrientCore, HiddenInsideTheFadeAngle) {
  EXPECT_EQ(Rolled(8).alpha, 0.0f);
  EXPECT_EQ(Rolled(-8).alpha, 0.0f);
}

TEST(OrientCore, FullBeyondThreeTimesTheFadeAngle) {
  EXPECT_FLOAT_EQ(Rolled(24).alpha, 1.0f);
  EXPECT_FLOAT_EQ(Rolled(-90).alpha, 1.0f);
}

TEST(OrientCore, FadeRisesSteadily) {
  float last = 0;
  for (float d = 9; d < 24; d += 1) {
    const float alpha = Rolled(d).alpha;
    EXPECT_GT(alpha, last);
    last = alpha;
  }
}

TEST(OrientCore, RollingRightPutsUpOnTheLeft) {
  EXPECT_NEAR(Rolled(90).angle, -kPi / 2, 1e-4f);
  EXPECT_NEAR(Rolled(-90).angle, kPi / 2, 1e-4f);
}

TEST(OrientCore, InvertedPointsDown) {
  const auto tick = Rolled(180);
  EXPECT_NEAR(std::fabs(tick.angle), kPi, 1e-4f);
  EXPECT_FLOAT_EQ(tick.alpha, 1.0f);
}

TEST(OrientCore, HiddenWhenUpIsAlongTheView) {
  EXPECT_EQ(ComputeUpTick(0.0f, 0.0f, 8.0f).alpha, 0.0f);
  EXPECT_EQ(ComputeUpTick(0.02f, -0.02f, 8.0f).alpha, 0.0f);
}

TEST(OrientCore, PitchAloneShowsNothing) {
  // Nose 80 degrees up, wings level: up is straight up the screen, so no roll to show.
  EXPECT_EQ(ComputeUpTick(0.0f, std::cos(80 * kPi / 180), 8.0f).alpha, 0.0f);
}

TEST(OrientCore, ZeroFadeShowsAnyRoll) { EXPECT_FLOAT_EQ(Rolled(1, 0.0f).alpha, 1.0f); }
