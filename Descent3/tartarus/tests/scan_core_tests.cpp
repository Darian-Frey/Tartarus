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

#include "../scan_core.h"

using tartarus::FaceInReach;
using tartarus::ScanTable;

TEST(ScanCore, NothingScannedAtFirst) {
  ScanTable t;
  EXPECT_FALSE(t.IsScanned(3, 7));
  EXPECT_EQ(t.Total(), 0u);
}

TEST(ScanCore, MarkIsTrueOnlyTheFirstTime) {
  ScanTable t;
  EXPECT_TRUE(t.Mark(3, 7));
  EXPECT_FALSE(t.Mark(3, 7));
  EXPECT_TRUE(t.IsScanned(3, 7));
  EXPECT_EQ(t.Total(), 1u);
}

TEST(ScanCore, CountsPerRoom) {
  ScanTable t;
  t.Mark(1, 0);
  t.Mark(1, 5);
  t.Mark(2, 0);
  EXPECT_EQ(t.ScannedInRoom(1), 2u);
  EXPECT_EQ(t.ScannedInRoom(2), 1u);
  EXPECT_EQ(t.ScannedInRoom(9), 0u);
  EXPECT_EQ(t.Total(), 3u);
}

TEST(ScanCore, ClearForgetsEverything) {
  ScanTable t;
  t.Mark(1, 0);
  t.Clear();
  EXPECT_FALSE(t.IsScanned(1, 0));
  EXPECT_EQ(t.Total(), 0u);
}

TEST(ScanCore, BadIndexesAreIgnored) {
  ScanTable t;
  EXPECT_FALSE(t.Mark(-1, 0));
  EXPECT_FALSE(t.Mark(0, -1));
  EXPECT_FALSE(t.IsScanned(-1, 0));
}

TEST(ScanCore, InReachWhenCloseAndFacing) {
  const float centre[3] = {0, 0, 100}, normal[3] = {0, 0, -1}, from[3] = {0, 0, 0};
  EXPECT_TRUE(FaceInReach(centre, normal, from, 200));
}

TEST(ScanCore, OutOfRange) {
  const float centre[3] = {0, 0, 300}, normal[3] = {0, 0, -1}, from[3] = {0, 0, 0};
  EXPECT_FALSE(FaceInReach(centre, normal, from, 200));
}

TEST(ScanCore, BackOfASurfaceIsNotInReach) {
  const float centre[3] = {0, 0, 100}, normal[3] = {0, 0, 1}, from[3] = {0, 0, 0};
  EXPECT_FALSE(FaceInReach(centre, normal, from, 200));
}
