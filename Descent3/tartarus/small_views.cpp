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

// Small-view windows (F-060). See small_views.h.

#include "small_views.h"

#include <cmath>
#include <string>

#include "classic.h"
#include "cvar.h"
#include "game.h"
#include "small_view_layout.h"

namespace tartarus {

namespace {

struct WindowCvars {
  Cvar *size = nullptr;
  Cvar *aspect = nullptr;
  Cvar *x = nullptr;
  Cvar *y = nullptr;
};

WindowCvars g_left, g_right;
Cvar *g_bigger = nullptr;
Cvar *g_mirror = nullptr;
bool g_syncing_mirror = false;

// The mirrors sit below the Score / Guide-Bot text.
struct MirrorPreset {
  const char *label;
  float size, aspect, x, y;
};
constexpr MirrorPreset kMirrors[kMirrorPresets] = {
    {"Stock (bottom left)", 0.25f, 1.0f, -1.0f, -1.0f},
    {"Top left", 0.18f, 2.0f, 0.18f, 0.15f},
    {"Top right", 0.18f, 2.0f, 0.82f, 0.15f},
};

bool Near(float a, float b) { return std::fabs(a - b) < 0.005f; }

WindowCvars Register(const std::string &w, const std::string &side) {
  const uint32_t flags = CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED;
  CvarRegistry &cv = Cvars();
  WindowCvars c;
  c.size = cv.RegisterFloat("hud_sv_" + w + "_size", 0.25f, 0.05f, 1.0f,
                            "Small view (" + side + "): height as a fraction of the 3D view height", flags);
  c.aspect = cv.RegisterFloat("hud_sv_" + w + "_aspect", 1.0f, 0.25f, 4.0f,
                              "Small view (" + side + "): width / height (1 = square, as upstream)", flags);
  c.x = cv.RegisterFloat("hud_sv_" + w + "_x", -1.0f, -1.0f, 1.0f,
                         "Small view (" + side + "): centre X, 0 = left edge, 1 = right edge, -1 = upstream", flags);
  c.y = cv.RegisterFloat("hud_sv_" + w + "_y", -1.0f, -1.0f, 1.0f,
                         "Small view (" + side + "): centre Y, 0 = top, 1 = bottom, -1 = upstream", flags);
  return c;
}

} // namespace

void SmallViews_Init() {
  g_left = Register("left", "left window");
  g_right = Register("right", "right window");
  g_bigger = Cvars().RegisterFloat("hud_sv_bigger_scale", 1.25f, 1.0f, 2.0f,
                                   "Small views: enlargement for 'bigger' views such as the guided missile (upstream 1.25)",
                                   CVAR_ARCHIVE | CVAR_CLASSIC_LOCKED);
  g_mirror = Cvars().RegisterInt("hud_sv_mirror", 0, 0, kMirrorCustom,
                                 "Rear-view mirror position for the left small window (F3 view; Shift+F1 picks what "
                                 "it shows): 0 stock, 1 top left, 2 top right, 3 custom",
                                 CVAR_CLASSIC_LOCKED);
  g_mirror->AddChangeCallback([](const Cvar &c) {
    if (!g_syncing_mirror)
      SmallViews_ApplyMirror(c.GetInt());
  });
}

const char *SmallViews_MirrorLabel(int preset) {
  return preset >= 0 && preset < kMirrorPresets ? kMirrors[preset].label : "Custom (console)";
}

int SmallViews_CurrentMirror() {
  for (int i = 0; i < kMirrorPresets; i++) {
    const MirrorPreset &p = kMirrors[i];
    if (Near(g_left.size->GetFloat(), p.size) && Near(g_left.aspect->GetFloat(), p.aspect) &&
        Near(g_left.x->GetFloat(), p.x) && Near(g_left.y->GetFloat(), p.y))
      return i;
  }
  return kMirrorCustom;
}

void SmallViews_ApplyMirror(int preset) {
  if (preset < 0 || preset >= kMirrorPresets)
    return;
  const MirrorPreset &p = kMirrors[preset];
  CvarRegistry &cv = Cvars();
  cv.Set("hud_sv_left_size", std::to_string(p.size), CvarSource::User);
  cv.Set("hud_sv_left_aspect", std::to_string(p.aspect), CvarSource::User);
  cv.Set("hud_sv_left_x", std::to_string(p.x), CvarSource::User);
  cv.Set("hud_sv_left_y", std::to_string(p.y), CvarSource::User);
}

void SmallViews_SyncMirrorCvar() {
  g_syncing_mirror = true;
  Cvars().Set("hud_sv_mirror", std::to_string(SmallViews_CurrentMirror()), CvarSource::Code);
  g_syncing_mirror = false;
}

void SmallViews_Adjust(int slot, bool bigger, int *left, int *top, int *right, int *bot) {
  if (IsClassic() || g_bigger == nullptr)
    return; // AV-001: upstream layout, untouched
  const WindowCvars *c = slot == 0 ? &g_left : slot == 2 ? &g_right : nullptr;
  if (c == nullptr)
    return;

  SvParams p;
  p.size = c->size->GetFloat();
  p.aspect = c->aspect->GetFloat();
  p.x = c->x->GetFloat();
  p.y = c->y->GetFloat();
  p.bigger_scale = g_bigger->GetFloat();
  const SvView view{Game_window_x, Game_window_y, Game_window_w, Game_window_h};
  const SvRect r = SmallViewRect(slot, view, bigger, p); // equals upstream's rectangle at the defaults
  *left = r.left;
  *top = r.top;
  *right = r.right;
  *bot = r.bot;
}

} // namespace tartarus
