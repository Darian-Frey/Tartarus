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

// Small-view window layout (F-060, SPEC.md §14): rear view, Guide-Bot and
// missile cameras in the HUD (non-cockpit) layout. Pure standard C++ so the
// "defaults are pixel-identical to upstream" promise is unit-tested;
// small_views.cpp reads the cvars.

#pragma once

namespace tartarus {

struct SvRect {
  int left = 0, top = 0, right = 0, bot = 0;
  bool operator==(const SvRect &o) const {
    return left == o.left && top == o.top && right == o.right && bot == o.bot;
  }
};

struct SvView {
  int x = 0, y = 0, w = 0, h = 0; // the 3D view (Game_window_*)
};

struct SvParams {
  float size = 0.25f;         // window height / view height
  float aspect = 1.0f;        // window width / window height
  float x = -1.0f;            // centre X as a fraction of the view width; < 0 = upstream slot
  float y = -1.0f;            // centre Y as a fraction of the view height; < 0 = upstream
  float bigger_scale = 1.25f; // SVF_BIGGER multiplier (upstream: + 1/4)
};

inline constexpr int kSvSlots = 3; // upstream NUM_SMALL_VIEWS (left, centre (unused), right)

/// Upstream's rectangle (SmallViews.cpp DrawSmallViews, dormant-cockpit branch).
SvRect UpstreamSmallViewRect(int slot, const SvView &view, bool bigger);

/// The rectangle for `slot` with `p`; equals UpstreamSmallViewRect when `p`
/// is all defaults. Custom placement is clamped inside the view.
SvRect SmallViewRect(int slot, const SvView &view, bool bigger, const SvParams &p);

} // namespace tartarus
