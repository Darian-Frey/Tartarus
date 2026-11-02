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

#include "../raw_mouse.h"

using namespace tartarus;

TEST(RawMouse, IdentityAtOne) {
  float rem = 0.0f;
  EXPECT_EQ(ScaleCounts(7, 1.0f, &rem), 7);
  EXPECT_EQ(ScaleCounts(-3, 1.0f, &rem), -3);
  EXPECT_FLOAT_EQ(rem, 0.0f);
}

TEST(RawMouse, ScalesUp) {
  float rem = 0.0f;
  EXPECT_EQ(ScaleCounts(5, 2.5f, &rem), 12);
  EXPECT_NEAR(rem, 0.5f, 1e-6f);
  EXPECT_EQ(ScaleCounts(5, 2.5f, &rem), 13); // carries the half count
}

TEST(RawMouse, SlowMovementIsNotLost) {
  // 1 count per frame at 0.25: every fourth frame moves one count.
  float rem = 0.0f;
  int total = 0;
  for (int i = 0; i < 400; ++i)
    total += ScaleCounts(1, 0.25f, &rem);
  EXPECT_EQ(total, 100);
  rem = 0.0f;
  total = 0;
  for (int i = 0; i < 400; ++i)
    total += ScaleCounts(-1, 0.25f, &rem);
  EXPECT_EQ(total, -100);
}

TEST(RawMouse, BadScaleFallsBackToOne) {
  float rem = 0.0f;
  EXPECT_EQ(ScaleCounts(4, -2.0f, &rem), 4);
}
