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

// Orientation aids on the HUD. See orient_hud.h.

#include "orient_hud.h"

#include <algorithm>
#include <cmath>

#include "3d.h"
#include "classic.h"
#include "cvar.h"
#include "game.h"
#include "grdefs.h"
#include "mouse_flight.h"
#include "object.h"
#include "orient_core.h"
#include "renderer.h"

namespace tartarus {

namespace {

Cvar *hud_uptick = nullptr;
Cvar *hud_uptick_len = nullptr;
Cvar *hud_uptick_fade_deg = nullptr;

const ddgr_color kTickColour = GR_RGB(255, 225, 120);

// rend_DrawLine has no alpha; this one does. Depth testing is off here.
void AlphaLine(float x1, float y1, float x2, float y2) {
  g3Point a{}, b{};
  a.p3_sx = x1;
  a.p3_sy = y1;
  b.p3_sx = x2;
  b.p3_sy = y2;
  a.p3_z = b.p3_z = 1.0f;
  rend_DrawSpecialLine(&a, &b);
}

void DrawUpTick() {
  const matrix &orient = Player_object->orient;
  // World up is +Y everywhere: gravity is always along that axis (SPEC.md §9).
  const UpTick tick = ComputeUpTick(orient.rvec.y(), orient.uvec.y(), hud_uptick_fade_deg->GetFloat());
  if (tick.alpha <= 0.0f)
    return;

  const float cx = Game_window_x + Game_window_w / 2.0f;
  const float cy = Game_window_y + Game_window_h / 2.0f;
  const float scale = Game_window_h / 480.0f;
  const float inner = std::max(24.0f, Game_window_h * 0.12f); // on the stick ring, with or without it
  const float length = hud_uptick_len->GetFloat() * scale;
  const float dir_x = std::sin(tick.angle), dir_y = -std::cos(tick.angle);

  // A wedge pointing up, as a fan of lines (lines are one pixel wide and the
  // renderer has no flat polygon for this). The dark one behind it keeps it
  // readable against bright walls.
  auto wedge = [&](float base, float half_width, float tip) {
    const float tip_x = cx + dir_x * tip, tip_y = cy + dir_y * tip;
    const int steps = std::max(1, static_cast<int>(half_width * 2.0f));
    for (int i = -steps; i <= steps; ++i) {
      const float side = half_width * i / steps;
      AlphaLine(cx + dir_x * base - dir_y * side, cy + dir_y * base + dir_x * side, tip_x, tip_y);
    }
  };

  StartFrame(0, 0, Max_window_w, Max_window_h, false);
  rend_SetZBufferState(0);
  rend_SetTextureType(TT_FLAT);
  rend_SetLighting(LS_NONE);
  rend_SetAlphaType(AT_CONSTANT);
  rend_SetAlphaValue(static_cast<uint8_t>(tick.alpha * 255.0f));
  rend_SetFlatColor(GR_BLACK);
  wedge(inner - 2.0f * scale, length * 0.5f + 2.0f * scale, inner + length + 3.0f * scale);
  rend_SetFlatColor(kTickColour);
  wedge(inner, length * 0.5f, inner + length);
  rend_SetAlphaType(AT_ALWAYS);
  rend_SetZBufferState(1);
  EndFrame();
}

} // namespace

void OrientHud_Init() {
  const uint32_t flags = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  CvarRegistry &cv = Cvars();
  hud_uptick = cv.RegisterBool("hud_uptick", true, "Tick on the reticle pointing to world up; fades out when upright", flags);
  hud_uptick_len = cv.RegisterFloat("hud_uptick_len", 14.0f, 4.0f, 40.0f, "Up-tick length, in pixels of a 480-line view", flags);
  hud_uptick_fade_deg = cv.RegisterFloat("hud_uptick_fade_deg", 8.0f, 0.0f, 45.0f,
                                         "Up-tick: hidden within this many degrees of upright, full at three times that", flags);
}

void OrientHud_Render() {
  if (IsClassic() || !hud_uptick->GetBool() || !Flight_ContextActive() || Player_object == nullptr ||
      Viewer_object != Player_object)
    return;
  DrawUpTick();
}

} // namespace tartarus
