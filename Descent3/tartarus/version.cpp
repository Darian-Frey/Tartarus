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

// Build identification for F-004. See version.h.

#include "version.h"

#include "game.h"
#include "gamefont.h"
#include "grdefs.h"
#include "grtext.h"
#include "display.h"
#include "tartarus_version.h"

namespace tartarus {

const char *BuildString() { return "Tartarus " TARTARUS_VERSION " " TARTARUS_GIT_HASH; }

const char *UpstreamString() { return "based on Descent 3 Open Source " TARTARUS_UPSTREAM_HASH; }

bool DrawMenuBuildString() {
  if (Display_NativeMenuActive())
    return true; // the deck prints these on its casing

  // Same font, colour and baseline as the upstream version line; right-aligned
  // because our text is longer than the fixed 164 px upstream allows.
  constexpr int kRightMargin = 8;
  constexpr int kBottomOffset = 29;
  constexpr int kLineGap = 2;

  grtext_SetFont(BRIEFING_FONT);
  grtext_SetAlpha(192);
  grtext_SetColor(GR_RGB(255, 32, 32));

  const char *lines[] = {BuildString(), UpstreamString()};
  const int line_h = grtext_GetTextHeight(lines[0]) + kLineGap;
  const int bottom_y = Max_window_h - kBottomOffset;

  auto print_all = [&] {
    for (int i = 0; i < 2; ++i) {
      const int x = Max_window_w - kRightMargin - grtext_GetTextLineWidth(lines[i]);
      const int y = bottom_y - (1 - i) * line_h;
      grtext_Printf(x, y, "%s", lines[i]);
    }
  };

  // Upstream prints once plain and once with a shadow; do the same.
  print_all();
  grtext_SetFlags(GRTEXTFLAG_SHADOW);
  print_all();

  grtext_Flush();
  return true;
}

} // namespace tartarus
