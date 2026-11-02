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

// Small-view window layout. See small_view_layout.h.

#include "small_view_layout.h"

#include <algorithm>
#include <cmath>

namespace tartarus {

namespace {

// Upstream's centre for a slot, before any size change.
void UpstreamCentre(int slot, const SvView &v, int w, int *cx, int *cy) {
  const int spacing = (v.w - kSvSlots * w) / 3;
  *cx = ((spacing + w) / 2) + (slot * (spacing + w)) + v.x;
  const int h = w;
  *cy = v.y + (v.h - h / 2 - v.h / 24);
}

SvRect FromCentre(int cx, int cy, int w, int h) { return {cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2}; }

} // namespace

SvRect UpstreamSmallViewRect(int slot, const SvView &v, bool bigger) {
  // Same integer arithmetic as upstream, step for step.
  int w = v.h / 4;
  int cx = 0, cy = 0;
  UpstreamCentre(slot, v, w, &cx, &cy);
  int h = w;
  if (bigger) {
    w += w / 4;
    h += h / 4;
  }
  return FromCentre(cx, cy, w, h);
}

SvRect SmallViewRect(int slot, const SvView &v, bool bigger, const SvParams &p) {
  const SvParams def;
  const bool is_default = p.size == def.size && p.aspect == def.aspect && p.x < 0.0f && p.y < 0.0f &&
                          (!bigger || p.bigger_scale == def.bigger_scale);
  if (is_default)
    return UpstreamSmallViewRect(slot, v, bigger);

  // Base size (before SVF_BIGGER), as upstream: height first, then width from the aspect.
  const int base_h = std::max(8, static_cast<int>(v.h * std::clamp(p.size, 0.05f, 1.0f)));
  const int base_w = std::max(8, static_cast<int>(base_h * std::clamp(p.aspect, 0.25f, 4.0f)));

  // Centre: upstream's slot position for the axes left at -1. Upstream's
  // centre depends on its own square size; keep that so moving only X (or
  // only Y) leaves the other axis where upstream would put it.
  int ucx = 0, ucy = 0;
  UpstreamCentre(slot, v, v.h / 4, &ucx, &ucy);
  int cx = p.x < 0.0f ? ucx : v.x + static_cast<int>(std::lround(std::clamp(p.x, 0.0f, 1.0f) * v.w));
  int cy = p.y < 0.0f ? ucy : v.y + static_cast<int>(std::lround(std::clamp(p.y, 0.0f, 1.0f) * v.h));

  int w = base_w, h = base_h;
  if (bigger) {
    const float s = std::clamp(p.bigger_scale, 1.0f, 2.0f);
    w += static_cast<int>(w * (s - 1.0f));
    h += static_cast<int>(h * (s - 1.0f));
  }
  w = std::min(w, v.w);
  h = std::min(h, v.h);

  // Keep the whole window inside the 3D view.
  cx = std::clamp(cx, v.x + w / 2, v.x + v.w - (w - w / 2));
  cy = std::clamp(cy, v.y + h / 2, v.y + v.h - (h - h / 2));
  return {cx - w / 2, cy - h / 2, cx - w / 2 + w, cy - h / 2 + h};
}

} // namespace tartarus
