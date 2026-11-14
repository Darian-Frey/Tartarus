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

#include "../debug_core.h"

using tartarus::kDebugNear;
using tartarus::Point2;
using tartarus::ProjectedRadius;
using tartarus::ProjectPoint;
using tartarus::ProjectSegment;
using tartarus::ViewCamera;

namespace {

// Looking along +Z from the origin, 100 pixels per unit at depth 1, centre (400, 300).
ViewCamera Camera() {
  ViewCamera c{};
  c.right[0] = 1;
  c.up[1] = 1;
  c.forward[2] = 1;
  c.cx = 400;
  c.cy = 300;
  c.fx = c.fy = 100;
  return c;
}

const float kEye[3] = {0, 0, 0};

} // namespace

TEST(DebugCore, PointAheadLandsOnScreen) {
  const float p[3] = {1, 2, 10};
  Point2 s;
  float depth = 0;
  ASSERT_TRUE(ProjectPoint(Camera(), kEye, p, &s, &depth));
  EXPECT_FLOAT_EQ(s.x, 410.0f); // right is +x on screen
  EXPECT_FLOAT_EQ(s.y, 280.0f); // up is -y on screen
  EXPECT_FLOAT_EQ(depth, 10.0f);
}

TEST(DebugCore, PointBehindOrTooNearIsNotDrawn) {
  Point2 s;
  const float behind[3] = {0, 0, -5};
  const float close[3] = {0, 0, kDebugNear * 0.5f};
  EXPECT_FALSE(ProjectPoint(Camera(), kEye, behind, &s, nullptr));
  EXPECT_FALSE(ProjectPoint(Camera(), kEye, close, &s, nullptr));
}

TEST(DebugCore, EyeOffsetIsSubtracted) {
  const float eye[3] = {5, 5, 5};
  const float p[3] = {5, 5, 15};
  Point2 s;
  ASSERT_TRUE(ProjectPoint(Camera(), eye, p, &s, nullptr));
  EXPECT_FLOAT_EQ(s.x, 400.0f);
  EXPECT_FLOAT_EQ(s.y, 300.0f);
}

TEST(DebugCore, SegmentWhollyBehindIsNotDrawn) {
  const float a[3] = {0, 0, -1}, b[3] = {1, 1, -10};
  Point2 pa, pb;
  EXPECT_FALSE(ProjectSegment(Camera(), kEye, a, b, &pa, &pb));
}

TEST(DebugCore, SegmentCrossingTheEyeIsCutAtTheNearPlane) {
  // From behind the eye to 10 ahead, 2 units to the right all the way.
  const float a[3] = {2, 0, -10}, b[3] = {2, 0, 10};
  Point2 pa, pb;
  ASSERT_TRUE(ProjectSegment(Camera(), kEye, a, b, &pa, &pb));
  EXPECT_FLOAT_EQ(pa.x, 400.0f + 2.0f / kDebugNear * 100.0f); // the cut end, at the near plane
  EXPECT_FLOAT_EQ(pb.x, 420.0f);
  EXPECT_FLOAT_EQ(pa.y, 300.0f);
}

TEST(DebugCore, SegmentAheadIsUnchanged) {
  const float a[3] = {0, 0, 2}, b[3] = {1, 0, 4};
  Point2 pa, pb;
  ASSERT_TRUE(ProjectSegment(Camera(), kEye, a, b, &pa, &pb));
  EXPECT_FLOAT_EQ(pa.x, 400.0f);
  EXPECT_FLOAT_EQ(pb.x, 425.0f);
}

TEST(DebugCore, SphereShrinksWithDistance) {
  EXPECT_FLOAT_EQ(ProjectedRadius(Camera(), 10.0f, 2.0f), 20.0f);
  EXPECT_FLOAT_EQ(ProjectedRadius(Camera(), 20.0f, 2.0f), 10.0f);
  EXPECT_FLOAT_EQ(ProjectedRadius(Camera(), 0.0f, 2.0f), 0.0f);
}
