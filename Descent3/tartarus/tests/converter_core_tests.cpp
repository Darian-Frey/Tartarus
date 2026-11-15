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

#include "../converter_core.h"

using tartarus::ConvertStep;

TEST(ConvertStep, MovesRateTimesDtAtTheRatio) {
  const auto r = ConvertStep(100.0f, 50.0f, 200.0f, 25.0f, 0.5f, 20.0f, 0.1f);
  EXPECT_FLOAT_EQ(r.source, 97.5f);
  EXPECT_FLOAT_EQ(r.destination, 51.25f);
}

TEST(ConvertStep, NeverTakesTheSourceBelowTheFloor) {
  const auto r = ConvertStep(21.0f, 50.0f, 200.0f, 25.0f, 0.5f, 20.0f, 1.0f);
  EXPECT_FLOAT_EQ(r.source, 20.0f);
  EXPECT_FLOAT_EQ(r.destination, 50.5f);
  const auto at = ConvertStep(20.0f, 50.0f, 200.0f, 25.0f, 0.5f, 20.0f, 1.0f);
  EXPECT_FLOAT_EQ(at.source, 20.0f);
  EXPECT_FLOAT_EQ(at.destination, 50.0f);
  const auto under = ConvertStep(10.0f, 50.0f, 200.0f, 25.0f, 0.5f, 20.0f, 1.0f); // already under: nothing
  EXPECT_FLOAT_EQ(under.source, 10.0f);
}

TEST(ConvertStep, NeverFillsTheDestinationPastMaxAndTakesOnlyWhatFits) {
  const auto r = ConvertStep(100.0f, 199.0f, 200.0f, 25.0f, 0.5f, 20.0f, 1.0f);
  EXPECT_FLOAT_EQ(r.destination, 200.0f);
  EXPECT_FLOAT_EQ(r.source, 98.0f); // 2 taken for 1 given
  const auto full = ConvertStep(100.0f, 200.0f, 200.0f, 25.0f, 0.5f, 20.0f, 1.0f);
  EXPECT_FLOAT_EQ(full.source, 100.0f);
  const auto over = ConvertStep(100.0f, 210.0f, 200.0f, 25.0f, 0.5f, 20.0f, 1.0f); // a powerup took it past max
  EXPECT_FLOAT_EQ(over.destination, 210.0f);
  EXPECT_FLOAT_EQ(over.source, 100.0f);
}

TEST(ConvertStep, NothingWithoutTimeOrRate) {
  for (const auto &r : {ConvertStep(100.0f, 50.0f, 200.0f, 25.0f, 0.5f, 20.0f, 0.0f),
                        ConvertStep(100.0f, 50.0f, 200.0f, 0.0f, 0.5f, 20.0f, 1.0f),
                        ConvertStep(100.0f, 50.0f, 200.0f, 25.0f, 0.0f, 20.0f, 1.0f)}) {
    EXPECT_FLOAT_EQ(r.source, 100.0f);
    EXPECT_FLOAT_EQ(r.destination, 50.0f);
  }
}

TEST(ConvertStep, SameTotalAtAnyFrameRate) {
  float a = 100.0f, b = 50.0f, c = 100.0f, d = 50.0f;
  for (int i = 0; i < 60; ++i) {
    const auto r = ConvertStep(a, b, 200.0f, 25.0f, 0.5f, 20.0f, 1.0f / 60.0f);
    a = r.source;
    b = r.destination;
  }
  for (int i = 0; i < 144; ++i) {
    const auto r = ConvertStep(c, d, 200.0f, 25.0f, 0.5f, 20.0f, 1.0f / 144.0f);
    c = r.source;
    d = r.destination;
  }
  EXPECT_NEAR(a, c, 1e-3f);
  EXPECT_NEAR(b, d, 1e-3f);
}
