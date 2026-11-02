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

#include "../small_view_layout.h"

using namespace tartarus;

namespace {

// Upstream's code from SmallViews.cpp DrawSmallViews(), copied verbatim as
// the oracle (NUM_SMALL_VIEWS = 3).
SvRect Oracle(int v, int Game_window_x, int Game_window_y, int Game_window_w, int Game_window_h, bool bigger) {
  int x, y, w, h, spacing;
  w = Game_window_h / 4;
  spacing = ((Game_window_w - (3) * w) / 3);
  x = ((spacing + w) / 2) + (v * (spacing + w)) + Game_window_x;
  h = w;
  y = Game_window_y + (Game_window_h - h / 2 - Game_window_h / 24);
  if (bigger) {
    w += w / 4;
    h += h / 4;
  }
  return {x - (w / 2), y - (h / 2), x + (w / 2), y + (h / 2)};
}

bool Inside(const SvRect &r, const SvView &v) {
  return r.left >= v.x && r.top >= v.y && r.right <= v.x + v.w && r.bot <= v.y + v.h;
}

} // namespace

TEST(SmallViewLayout, DefaultsArePixelIdenticalToUpstream) {
  const SvView views[] = {{0, 0, 640, 480},  {0, 0, 1280, 720},  {0, 0, 1920, 1080}, {0, 0, 2560, 1080},
                          {0, 0, 1280, 800}, {64, 48, 512, 384}, {0, 0, 1366, 768},  {0, 0, 803, 601}};
  for (const SvView &v : views) {
    for (int slot : {0, 2}) {
      for (bool bigger : {false, true}) {
        const SvRect want = Oracle(slot, v.x, v.y, v.w, v.h, bigger);
        EXPECT_EQ(UpstreamSmallViewRect(slot, v, bigger), want) << v.w << "x" << v.h << " slot " << slot;
        EXPECT_EQ(SmallViewRect(slot, v, bigger, SvParams{}), want) << v.w << "x" << v.h << " slot " << slot;
      }
    }
  }
}

TEST(SmallViewLayout, SizeAndAspect) {
  const SvView v{0, 0, 1280, 720};
  SvParams p;
  p.size = 0.3f;
  p.aspect = 1.6f;
  const SvRect r = SmallViewRect(0, v, false, p);
  EXPECT_EQ(r.bot - r.top, 216);
  EXPECT_EQ(r.right - r.left, 345);
  EXPECT_TRUE(Inside(r, v));
}

TEST(SmallViewLayout, CustomPositionAndOtherAxisStaysUpstream) {
  const SvView v{0, 0, 1280, 720};
  const SvRect up = Oracle(2, 0, 0, 1280, 720, false);
  SvParams p;
  p.y = 0.2f; // move up only
  const SvRect r = SmallViewRect(2, v, false, p);
  EXPECT_EQ((r.left + r.right) / 2, (up.left + up.right) / 2);
  EXPECT_EQ((r.top + r.bot) / 2, 144);
}

TEST(SmallViewLayout, ClampedInsideTheView) {
  const SvView v{10, 20, 1280, 720};
  SvParams p;
  p.size = 0.5f;
  p.aspect = 2.0f;
  for (float x : {0.0f, 0.5f, 1.0f}) {
    for (float y : {0.0f, 1.0f}) {
      p.x = x;
      p.y = y;
      for (bool bigger : {false, true})
        EXPECT_TRUE(Inside(SmallViewRect(0, v, bigger, p), v)) << x << "," << y;
    }
  }
}

TEST(SmallViewLayout, OversizeIsCappedToTheView) {
  const SvView v{0, 0, 640, 480};
  SvParams p;
  p.size = 1.0f;
  p.aspect = 4.0f;
  const SvRect r = SmallViewRect(0, v, true, p);
  EXPECT_TRUE(Inside(r, v));
  EXPECT_LE(r.right - r.left, 640);
}

TEST(SmallViewLayout, BiggerScale) {
  const SvView v{0, 0, 1280, 720};
  SvParams p;
  p.bigger_scale = 1.5f;
  const SvRect r = SmallViewRect(0, v, true, p);
  EXPECT_EQ(r.bot - r.top, 180 + 90);
}
