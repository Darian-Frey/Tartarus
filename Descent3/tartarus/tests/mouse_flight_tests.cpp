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

#include "../mouse_flight_core.h"

using namespace tartarus;

TEST(VjCurve, DeadZoneIsZero) {
  EXPECT_EQ(VjCurve(0.0f, 0.04f, 1.8f), 0.0f);
  EXPECT_EQ(VjCurve(0.04f, 0.04f, 1.8f), 0.0f);
  EXPECT_EQ(VjCurve(-0.03f, 0.04f, 1.8f), 0.0f);
}

TEST(VjCurve, FullDeflectionIsOneAndSymmetric) {
  EXPECT_FLOAT_EQ(VjCurve(1.0f, 0.04f, 1.8f), 1.0f);
  EXPECT_FLOAT_EQ(VjCurve(-1.0f, 0.04f, 1.8f), -1.0f);
  EXPECT_FLOAT_EQ(VjCurve(5.0f, 0.04f, 1.8f), 1.0f); // clamped
  EXPECT_FLOAT_EQ(VjCurve(0.5f, 0.0f, 1.0f), 0.5f);  // linear without dead zone
  EXPECT_FLOAT_EQ(VjCurve(-0.5f, 0.0f, 1.0f), -0.5f);
}

TEST(VjCurve, CurveSoftensSmallDeflections) {
  const float lin = VjCurve(0.3f, 0.04f, 1.0f);
  const float curved = VjCurve(0.3f, 0.04f, 1.8f);
  EXPECT_GT(lin, curved);
  EXPECT_GT(curved, 0.0f);
}

TEST(VjCurve, RejectsNonFinite) {
  EXPECT_EQ(VjCurve(std::numeric_limits<float>::quiet_NaN(), 0.04f, 1.8f), 0.0f);
}

TEST(VirtualJoystick, AccumulatesAndScales) {
  VirtualJoystick vj;
  vj.AddDelta(20.0f, -10.0f, 100.0f, 1.0f);
  EXPECT_FLOAT_EQ(vj.X(), 0.2f);
  EXPECT_FLOAT_EQ(vj.Y(), -0.1f);
  vj.AddDelta(20.0f, 0.0f, 100.0f, 2.0f);
  EXPECT_FLOAT_EQ(vj.X(), 0.6f);
}

TEST(VirtualJoystick, StaysAtLastOffsetWithoutInput) {
  VirtualJoystick vj;
  vj.AddDelta(50.0f, 0.0f, 100.0f, 1.0f);
  vj.AddDelta(0.0f, 0.0f, 100.0f, 1.0f);
  vj.Decay(0.0f, 1.0f); // mode 1: no auto-centring
  EXPECT_FLOAT_EQ(vj.X(), 0.5f);
}

TEST(VirtualJoystick, ClampsToUnitCircle) {
  VirtualJoystick vj;
  vj.AddDelta(1000.0f, 1000.0f, 100.0f, 1.0f);
  EXPECT_NEAR(std::sqrt(vj.X() * vj.X() + vj.Y() * vj.Y()), 1.0f, 1e-5f);
  EXPECT_NEAR(vj.X(), vj.Y(), 1e-6f);
  // Coming back from the rim responds at once (no stored overshoot).
  vj.AddDelta(-100.0f * std::sqrt(0.5f), 0.0f, 100.0f, 1.0f);
  EXPECT_NEAR(vj.X(), 0.0f, 1e-5f);
}

TEST(VirtualJoystick, DecaysTowardCentreAndSnaps) {
  VirtualJoystick vj;
  vj.AddDelta(100.0f, 0.0f, 100.0f, 1.0f);
  vj.Decay(1.5f, 1.0f);
  EXPECT_NEAR(vj.X(), std::exp(-1.5f), 1e-5f);
  for (int i = 0; i < 100; ++i)
    vj.Decay(1.5f, 0.1f);
  EXPECT_TRUE(vj.IsCentred());
}

// AV-013: every reset trigger ends in a centred stick.
TEST(VirtualJoystick, ResetCentres) {
  VirtualJoystick vj;
  vj.AddDelta(80.0f, 40.0f, 100.0f, 1.0f);
  EXPECT_FALSE(vj.IsCentred());
  vj.Reset();
  EXPECT_TRUE(vj.IsCentred());
  EXPECT_EQ(VjCurve(vj.X(), 0.04f, 1.8f), 0.0f);
}

TEST(VirtualJoystick, IgnoresBadInput) {
  VirtualJoystick vj;
  vj.AddDelta(10.0f, 10.0f, 0.0f, 1.0f);
  vj.AddDelta(std::numeric_limits<float>::infinity(), 0.0f, 100.0f, 1.0f);
  EXPECT_TRUE(vj.IsCentred());
}
