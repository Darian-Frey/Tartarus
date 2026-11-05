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

using tartarus::BallOffset;
using tartarus::ComputeGizmo;
using tartarus::ComputeUpTick;
using tartarus::DiscAbove;
using tartarus::DiscBelow;
using tartarus::PitchDegrees;

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

TEST(OrientCore, PitchFromForwardAxis) {
  EXPECT_NEAR(PitchDegrees(0.0f), 0.0f, 1e-4f);
  EXPECT_NEAR(PitchDegrees(1.0f), 90.0f, 1e-3f);
  EXPECT_NEAR(PitchDegrees(-1.0f), -90.0f, 1e-3f);
  EXPECT_NEAR(PitchDegrees(1.5f), 90.0f, 1e-3f); // rounding error in a matrix must not give NaN
}

TEST(OrientCore, NoseUpMovesTheHorizonDown) {
  EXPECT_LT(BallOffset(0, 20, 45), 0.0f);
  EXPECT_GT(BallOffset(0, -20, 45), 0.0f);
  EXPECT_NEAR(BallOffset(30, 30, 45), 0.0f, 1e-6f); // the mark at the nose sits in the centre
}

TEST(OrientCore, DiscSplitsAtTheLine) {
  for (float h : {-0.6f, 0.0f, 0.6f}) {
    for (const auto &p : DiscAbove(h, 32))
      EXPECT_GE(p.y, h - 1e-4f);
    for (const auto &p : DiscBelow(h, 32))
      EXPECT_LE(p.y, h + 1e-4f);
  }
}

TEST(OrientCore, LineOutsideTheDiscGivesAllOrNothing) {
  EXPECT_TRUE(DiscAbove(1.2f, 32).empty());
  EXPECT_EQ(DiscAbove(-1.2f, 32).size(), 32u);
  EXPECT_EQ(DiscBelow(1.2f, 32).size(), 32u);
  EXPECT_TRUE(DiscBelow(-1.2f, 32).empty());
}

TEST(OrientCore, DiscPolygonsFitTheRenderer) {
  EXPECT_LT(DiscAbove(0.3f, 32).size(), 100u); // rend_DrawPolygon2D asserts nv < 100
}

TEST(OrientCore, GizmoWhenLevelShowsUpUpward) {
  const float right[3] = {1, 0, 0}, up[3] = {0, 1, 0}, forward[3] = {0, 0, 1};
  bool found = false;
  for (const auto &axis : ComputeGizmo(right, up, forward)) {
    if (axis.axis == 1) {
      EXPECT_FLOAT_EQ(axis.y, 1.0f);
      EXPECT_FLOAT_EQ(axis.x, 0.0f);
      found = true;
    }
  }
  EXPECT_TRUE(found);
}

TEST(OrientCore, GizmoDrawsFarthestFirst) {
  const float right[3] = {0.6f, 0, 0.8f}, up[3] = {0, 1, 0}, forward[3] = {-0.8f, 0, 0.6f};
  const auto axes = ComputeGizmo(right, up, forward);
  EXPECT_GE(axes[0].depth, axes[1].depth);
  EXPECT_GE(axes[1].depth, axes[2].depth);
}
