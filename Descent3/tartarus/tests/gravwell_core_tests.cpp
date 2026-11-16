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

#include "../gravwell_core.h"

using tartarus::DeflectDeltaV;
using tartarus::PulseDeltaV;
using tartarus::WellAcceleration;
using tartarus::WellDeltaV;
using tartarus::WellSettings;

namespace {
const WellSettings kSpec;
}

TEST(GravWell, FullPullAtTheCentreForALightObject) {
  EXPECT_FLOAT_EQ(WellAcceleration(0.0f, 1.5f, 2.0f, kSpec), 420.0f);
}

TEST(GravWell, LinearFalloffToNothingAtTheRadius) {
  EXPECT_FLOAT_EQ(WellAcceleration(30.0f, 1.5f, 2.0f, kSpec), 210.0f);
  EXPECT_FLOAT_EQ(WellAcceleration(60.0f, 1.5f, 2.0f, kSpec), 0.0f);
  EXPECT_FLOAT_EQ(WellAcceleration(90.0f, 1.5f, 2.0f, kSpec), 0.0f);
  EXPECT_FLOAT_EQ(WellAcceleration(-5.0f, 1.5f, 2.0f, kSpec), 420.0f); // overlapping the centre
}

TEST(GravWell, FadesAsTheWellExpires) {
  EXPECT_FLOAT_EQ(WellAcceleration(0.0f, 0.75f, 2.0f, kSpec), 210.0f);
  EXPECT_FLOAT_EQ(WellAcceleration(0.0f, 0.0f, 2.0f, kSpec), 0.0f);
}

TEST(GravWell, HeavyObjectsBudgeAndBossesIgnoreIt) {
  EXPECT_FLOAT_EQ(WellAcceleration(0.0f, 1.5f, 8.0f, kSpec), 210.0f);  // twice mass_ref: half
  EXPECT_FLOAT_EQ(WellAcceleration(0.0f, 1.5f, 39.0f, kSpec), 63.0f);  // floor of 0.15
  EXPECT_FLOAT_EQ(WellAcceleration(0.0f, 1.5f, 40.0f, kSpec), 0.0f);   // immune
  WellSettings none = kSpec;
  none.immune_mass = 0.0f;
  EXPECT_FLOAT_EQ(WellAcceleration(0.0f, 1.5f, 400.0f, none), 63.0f);
}

TEST(GravWell, NeverMoreThanTheForce) {
  WellSettings s = kSpec;
  s.mass_min_scale = 5.0f; // a bad setting cannot push it past force
  EXPECT_LE(WellAcceleration(0.0f, 3.0f, 0.1f, s), 420.0f);
}

TEST(GravWell, BadNumbersGiveNothing) {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float inf = std::numeric_limits<float>::infinity();
  EXPECT_FLOAT_EQ(WellAcceleration(nan, 1.5f, 2.0f, kSpec), 0.0f);
  EXPECT_FLOAT_EQ(WellAcceleration(0.0f, inf, 2.0f, kSpec), 0.0f);
  EXPECT_FLOAT_EQ(WellAcceleration(0.0f, 1.5f, 0.0f, kSpec), 0.0f);
  EXPECT_FLOAT_EQ(WellDeltaV(nan, 0.016f), 0.0f);
  EXPECT_FLOAT_EQ(WellDeltaV(420.0f, inf), 0.0f);
  EXPECT_FLOAT_EQ(WellDeltaV(420.0f, -1.0f), 0.0f);
  EXPECT_FLOAT_EQ(WellDeltaV(420.0f, 0.5f), 210.0f);
}

TEST(GravWell, SamePullOverASecondAtAnyFrameRate) {
  float v60 = 0.0f, v144 = 0.0f;
  for (int i = 0; i < 60; ++i)
    v60 += WellDeltaV(WellAcceleration(10.0f, 1.5f, 2.0f, kSpec), 1.0f / 60.0f);
  for (int i = 0; i < 144; ++i)
    v144 += WellDeltaV(WellAcceleration(10.0f, 1.5f, 2.0f, kSpec), 1.0f / 144.0f);
  EXPECT_NEAR(v60, v144, 1e-2f);
}

TEST(Repulsor, OneShoveFallingOffToTheRadius) {
  WellSettings s = kSpec;
  s.radius = 35.0f;
  s.force = 150.0f;
  s.time = 0.0f; // not used by the pulse
  EXPECT_FLOAT_EQ(PulseDeltaV(0.0f, 2.0f, s), 150.0f);
  EXPECT_FLOAT_EQ(PulseDeltaV(17.5f, 2.0f, s), 75.0f);
  EXPECT_FLOAT_EQ(PulseDeltaV(35.0f, 2.0f, s), 0.0f);
  EXPECT_FLOAT_EQ(PulseDeltaV(0.0f, 8.0f, s), 75.0f);  // heavy: half
  EXPECT_FLOAT_EQ(PulseDeltaV(0.0f, 40.0f, s), 0.0f);  // boss: immune
  EXPECT_FLOAT_EQ(PulseDeltaV(std::numeric_limits<float>::quiet_NaN(), 2.0f, s), 0.0f);
}

TEST(Repulsor, MissilesClosingAreTurnedRound) {
  EXPECT_FLOAT_EQ(DeflectDeltaV(-50.0f), 100.0f); // closing at 50, leaves at 50
  EXPECT_FLOAT_EQ(DeflectDeltaV(20.0f), 0.0f);    // already going away
  EXPECT_FLOAT_EQ(DeflectDeltaV(std::numeric_limits<float>::infinity()), 0.0f);
  EXPECT_FLOAT_EQ(DeflectDeltaV(-std::numeric_limits<float>::infinity()), 0.0f);
}
