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
using tartarus::BankCommand;
using tartarus::ClimbLabel;
using tartarus::LevelRate;
using tartarus::NearestUpright;
using tartarus::WrapDegrees;
using tartarus::DecodeReticleMask;
using tartarus::ReticleLines;
using tartarus::ClipLine;
using tartarus::ComputeFlightMarker;
using tartarus::FlightMarkerAlpha;
using tartarus::HeadingDegrees;
using tartarus::HeadingLabel;
using tartarus::HeadingMarks;
using tartarus::RollPointer;
using tartarus::SmoothVelocity;
using tartarus::ComputeLadderBar;
using tartarus::NoseHeading;
using tartarus::Point2;
using tartarus::ProjectDirection;
using tartarus::ScreenRect;
using tartarus::SmoothValue;
using tartarus::TapeMarks;
using tartarus::ViewCamera;
using tartarus::ViewCameraScale;
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

namespace {

// A 1280x720 view, FOV zoom 0.73, turned by pitch (nose up) and roll (right).
ViewCamera Camera(float pitch_deg = 0, float roll_deg = 0) {
  const float p = pitch_deg * kPi / 180, r = roll_deg * kPi / 180;
  const float f[3] = {0, std::sin(p), std::cos(p)};
  const float u0[3] = {0, std::cos(p), -std::sin(p)};
  const float r0[3] = {1, 0, 0};
  ViewCamera c{};
  for (int i = 0; i < 3; ++i) {
    c.forward[i] = f[i];
    c.right[i] = std::cos(r) * r0[i] - std::sin(r) * u0[i];
    c.up[i] = std::sin(r) * r0[i] + std::cos(r) * u0[i];
  }
  c.cx = 640;
  c.cy = 360;
  ViewCameraScale(&c, 1280, 720, 0.73f);
  return c;
}

constexpr ScreenRect kScreen{0, 0, 1280, 720};

} // namespace

TEST(OrientFlight, ScaleMatchesTheEngine) {
  ViewCamera c{};
  ViewCameraScale(&c, 1280, 720, 0.73f);
  EXPECT_FLOAT_EQ(c.fx, c.fy); // square pixels
  EXPECT_NEAR(c.fy, 360 / (0.73f * 0.75f), 1e-2f);
}

TEST(OrientFlight, ForwardProjectsToTheCentre) {
  const auto c = Camera();
  const float forward[3] = {0, 0, 1};
  Point2 p;
  ASSERT_TRUE(ProjectDirection(c, forward, &p));
  EXPECT_FLOAT_EQ(p.x, 640);
  EXPECT_FLOAT_EQ(p.y, 360);
}

TEST(OrientFlight, BehindIsRejected) {
  const float back[3] = {0, 0, -1};
  Point2 p;
  EXPECT_FALSE(ProjectDirection(Camera(), back, &p));
}

TEST(OrientFlight, ClipKeepsTheInsidePart) {
  Point2 a{-100, 360}, b{1400, 360};
  ASSERT_TRUE(ClipLine(kScreen, &a, &b));
  EXPECT_NEAR(a.x, 0, 0.01f);
  EXPECT_NEAR(b.x, 1280, 0.01f);
  Point2 c{-100, -100}, d{-50, 800};
  EXPECT_FALSE(ClipLine(kScreen, &c, &d));
}

TEST(OrientFlight, HeadingHeldNearVertical) {
  float x = 0, z = 1;
  const float east[3] = {1, 0, 0};
  NoseHeading(east, &x, &z);
  EXPECT_FLOAT_EQ(x, 1);
  const float up[3] = {0.01f, 1, 0};
  NoseHeading(up, &x, &z);
  EXPECT_FLOAT_EQ(x, 1); // unchanged
}

TEST(OrientFlight, LadderMarkAtTheNoseSitsInTheCentre) {
  tartarus::LadderBar bar;
  ASSERT_TRUE(ComputeLadderBar(Camera(20), 0, 1, 20, 3, 9, &bar));
  EXPECT_NEAR((bar.inner_left.x + bar.inner_right.x) / 2, 640, 0.01f);
  EXPECT_NEAR((bar.inner_left.y + bar.inner_right.y) / 2, 360, 2.0f); // the bar bows slightly
  EXPECT_LT(bar.outer_left.x, bar.inner_left.x);
  EXPECT_GT(bar.outer_right.x, bar.inner_right.x);
}

TEST(OrientFlight, MarkerSitsOnTheDirectionOfTravel) {
  const auto c = Camera();
  const float velocity[3] = {5, 0, 50};
  tartarus::FlightMarker m;
  ASSERT_TRUE(ComputeFlightMarker(c, velocity, kScreen, &m));
  EXPECT_GT(m.position.x, 640); // sliding right
  EXPECT_FALSE(m.reversed);
  EXPECT_FALSE(m.clamped);
}

TEST(OrientFlight, MarkerFadesInWithSpeed) {
  const float slow[3] = {3, 0, 0}, mid[3] = {9, 0, 0}, fast[3] = {12, 0, 0};
  EXPECT_EQ(FlightMarkerAlpha(slow, 6), 0.0f);
  EXPECT_GT(FlightMarkerAlpha(mid, 6), 0.0f);
  EXPECT_LT(FlightMarkerAlpha(mid, 6), 1.0f);
  EXPECT_FLOAT_EQ(FlightMarkerAlpha(fast, 6), 1.0f);
}

TEST(OrientFlight, MarkerNothingWhenStopped) {
  const float velocity[3] = {0, 0, 0};
  tartarus::FlightMarker m;
  EXPECT_FALSE(ComputeFlightMarker(Camera(), velocity, kScreen, &m));
}

TEST(OrientFlight, SmoothingIgnoresAOneFrameJolt) {
  float smoothed[3] = {0, 0, 30};
  const float jolt[3] = {30, 0, 30};
  SmoothVelocity(smoothed, jolt, 1.0f / 60, 0.2f);
  EXPECT_LT(smoothed[0], 3.0f); // under a tenth of the jump
}

TEST(OrientFlight, SmoothingReachesTheTarget) {
  float smoothed[3] = {0, 0, 0};
  const float v[3] = {10, 0, 0};
  for (int i = 0; i < 120; ++i)
    SmoothVelocity(smoothed, v, 1.0f / 60, 0.2f);
  EXPECT_NEAR(smoothed[0], 10.0f, 0.01f);
}

TEST(OrientFlight, SmoothingDoesNotDependOnFrameRate) {
  float at30[3] = {0, 0, 0}, at120[3] = {0, 0, 0};
  const float v[3] = {10, 0, 0};
  for (int i = 0; i < 15; ++i)
    SmoothVelocity(at30, v, 1.0f / 30, 0.2f);
  for (int i = 0; i < 60; ++i)
    SmoothVelocity(at120, v, 1.0f / 120, 0.2f);
  EXPECT_NEAR(at30[0], at120[0], 0.001f);
}

TEST(OrientFlight, NoSmoothingFollowsAtOnce) {
  float smoothed[3] = {0, 0, 0};
  const float v[3] = {10, 0, 0};
  SmoothVelocity(smoothed, v, 1.0f / 60, 0.0f);
  EXPECT_FLOAT_EQ(smoothed[0], 10.0f);
}

TEST(OrientFlight, HorizonBarIsLevelWhenLevel) {
  tartarus::LadderBar bar;
  ASSERT_TRUE(ComputeLadderBar(Camera(), 0, 1, 0, 3, 20, &bar));
  EXPECT_NEAR(bar.outer_left.y, 360, 0.01f);
  EXPECT_NEAR(bar.outer_right.y, 360, 0.01f);
  EXPECT_LT(bar.outer_left.x, 640);
  EXPECT_GT(bar.outer_right.x, 640);
}

TEST(OrientFlight, MarkerReversedGoingBackwards) {
  const float velocity[3] = {0, 0, -30};
  tartarus::FlightMarker m;
  ASSERT_TRUE(ComputeFlightMarker(Camera(), velocity, kScreen, &m));
  EXPECT_TRUE(m.reversed);
  EXPECT_FLOAT_EQ(m.position.x, 640);
}

TEST(OrientFlight, MarkerClampedToTheEdgeWhenSliding) {
  const float velocity[3] = {30, 0, 0};
  tartarus::FlightMarker m;
  ASSERT_TRUE(ComputeFlightMarker(Camera(), velocity, kScreen, &m));
  EXPECT_TRUE(m.clamped);
  EXPECT_NEAR(m.position.x, 1280, 0.01f);
  EXPECT_NEAR(m.position.y, 360, 0.01f);
}

TEST(OrientFlight, HeadingFromGroundDirection) {
  EXPECT_NEAR(HeadingDegrees(0, 1), 0.0f, 1e-3f);
  EXPECT_NEAR(HeadingDegrees(1, 0), 90.0f, 1e-3f);
  EXPECT_NEAR(HeadingDegrees(0, -1), 180.0f, 1e-3f);
  EXPECT_NEAR(HeadingDegrees(-1, 0), 270.0f, 1e-3f);
}

TEST(OrientFlight, HeadingMarksCentreOnTheHeading) {
  const auto marks = HeadingMarks(100, 30, 10);
  ASSERT_EQ(marks.size(), 7u);
  EXPECT_FLOAT_EQ(marks.front().offset, -1.0f);
  EXPECT_FLOAT_EQ(marks.back().offset, 1.0f);
  EXPECT_EQ(marks[3].degrees, 100);
}

TEST(OrientFlight, HeadingMarksWrapThroughNorth) {
  const auto marks = HeadingMarks(5, 30, 10);
  bool saw_north = false, saw_350 = false;
  for (const auto &m : marks) {
    EXPECT_GE(m.degrees, 0);
    EXPECT_LT(m.degrees, 360);
    saw_north |= m.degrees == 0;
    saw_350 |= m.degrees == 350;
  }
  EXPECT_TRUE(saw_north);
  EXPECT_TRUE(saw_350);
}

TEST(OrientFlight, HeadingLabels) {
  char text[8];
  HeadingLabel(0, text, sizeof(text));
  EXPECT_STREQ(text, "N");
  HeadingLabel(270, text, sizeof(text));
  EXPECT_STREQ(text, "W");
  HeadingLabel(280, text, sizeof(text));
  EXPECT_STREQ(text, "28");
  HeadingLabel(30, text, sizeof(text));
  EXPECT_STREQ(text, "03");
}

TEST(OrientFlight, RollPointerPinsAtTheEndOfTheArc) {
  bool pinned = false;
  EXPECT_FLOAT_EQ(RollPointer(0.5f, 1.0f, &pinned), 0.5f);
  EXPECT_FALSE(pinned);
  EXPECT_FLOAT_EQ(RollPointer(-2.0f, 1.0f, &pinned), -1.0f);
  EXPECT_TRUE(pinned);
}

TEST(OrientFlight, TapeMarksCentreOnTheValue) {
  const auto marks = TapeMarks(50, 40, 10);
  ASSERT_EQ(marks.size(), 9u);
  EXPECT_FLOAT_EQ(marks.front().offset, -1.0f);
  EXPECT_FLOAT_EQ(marks.back().offset, 1.0f);
  EXPECT_EQ(marks[4].value, 50);
}

TEST(OrientFlight, TapeStopsAtZero) {
  for (const auto &mark : TapeMarks(5, 40, 10))
    EXPECT_GE(mark.value, 0);
}

TEST(OrientFlight, SmoothValueReachesTheTarget) {
  float value = 0;
  for (int i = 0; i < 120; ++i)
    SmoothValue(&value, 10, 1.0f / 60, 0.1f);
  EXPECT_NEAR(value, 10.0f, 0.01f);
}

TEST(OrientFlight, ClimbLabels) {
  char text[8];
  ClimbLabel(12.4f, text, sizeof(text));
  EXPECT_STREQ(text, "+12");
  ClimbLabel(-7.6f, text, sizeof(text));
  EXPECT_STREQ(text, "-8");
  ClimbLabel(0.3f, text, sizeof(text));
  EXPECT_STREQ(text, "0");
}

TEST(OrientReticle, StockHasNoLines) { EXPECT_TRUE(ReticleLines(tartarus::kReticleStock).empty()); }

TEST(OrientReticle, EveryStyleIsDrawnAndSymmetric) {
  for (int style = tartarus::kReticleGunsight; style < tartarus::kReticleStyles; ++style) {
    const auto lines = ReticleLines(style);
    ASSERT_FALSE(lines.empty());
    float sum_x = 0;
    for (const auto &line : lines) {
      sum_x += line.x1 + line.x2;
      EXPECT_LE(std::fabs(line.x1), 1.01f);
      EXPECT_LE(std::fabs(line.y1), 1.01f);
    }
    EXPECT_NEAR(sum_x, 0.0f, 1e-3f);
  }
}

TEST(OrientReticle, MiddleStaysClearToAim) {
  for (int style = tartarus::kReticleGunsight; style < tartarus::kReticleStyles; ++style) {
    for (const auto &line : ReticleLines(style)) {
      EXPECT_GE(std::hypot(line.x1, line.y1), 0.3f);
      EXPECT_GE(std::hypot(line.x2, line.y2), 0.3f);
    }
  }
}

TEST(OrientReticle, GunpointBitsMapToSides) {
  const auto bank = DecodeReticleMask((1u << 1) | (1u << 2) | (1u << 3) | (1u << 4), 1u << 2);
  EXPECT_TRUE(bank.exists[tartarus::kSlotRight1]);
  EXPECT_TRUE(bank.exists[tartarus::kSlotLeft1]);
  EXPECT_TRUE(bank.exists[tartarus::kSlotRight2]);
  EXPECT_TRUE(bank.exists[tartarus::kSlotLeft2]);
  EXPECT_FALSE(bank.exists[tartarus::kSlotCentre]);
  EXPECT_TRUE(bank.ready[tartarus::kSlotLeft1]);
  EXPECT_FALSE(bank.ready[tartarus::kSlotRight1]);
}

TEST(OrientReticle, MassDriverBitIsTheCentre) {
  const auto bank = DecodeReticleMask(1u << 8, 1u << 8);
  EXPECT_TRUE(bank.exists[tartarus::kSlotCentre]);
  EXPECT_TRUE(bank.ready[tartarus::kSlotCentre]);
}

TEST(OrientLevel, WrapKeepsTheShortWay) {
  EXPECT_FLOAT_EQ(WrapDegrees(190), -170.0f);
  EXPECT_FLOAT_EQ(WrapDegrees(-190), 170.0f);
  EXPECT_FLOAT_EQ(WrapDegrees(30), 30.0f);
}

TEST(OrientLevel, NearestOfFourUprights) {
  EXPECT_FLOAT_EQ(NearestUpright(30), 0.0f);
  EXPECT_FLOAT_EQ(NearestUpright(50), 90.0f);
  EXPECT_FLOAT_EQ(NearestUpright(-100), -90.0f);
  EXPECT_FLOAT_EQ(std::fabs(NearestUpright(170)), 180.0f);
}

TEST(OrientLevel, RateFollowsTheErrorUpToTheLimit) {
  EXPECT_FLOAT_EQ(LevelRate(9, 0.1f, 180), 90.0f);
  EXPECT_FLOAT_EQ(LevelRate(90, 0.1f, 180), 180.0f);
  EXPECT_FLOAT_EQ(LevelRate(-90, 0.1f, 180), -180.0f);
}

TEST(OrientLevel, RolledRightCommandsBankLeft) {
  // Rolled right is a negative roll; the bank-left key is positive.
  EXPECT_GT(BankCommand(-45, 12000, 120), 0.0f);
  EXPECT_LT(BankCommand(45, 12000, 120), 0.0f);
}

TEST(OrientLevel, CommandIsLimitedToFullBank) {
  EXPECT_FLOAT_EQ(BankCommand(-100000, 12000, 120), 1.0f);
  EXPECT_FLOAT_EQ(BankCommand(100000, 12000, 120), -1.0f);
}

TEST(OrientLevel, NoRateNoCommand) { EXPECT_EQ(BankCommand(0, 12000, 120), 0.0f); }

TEST(OrientLevel, NoDragIsSafe) { EXPECT_EQ(BankCommand(45, 12000, 0), 0.0f); }
