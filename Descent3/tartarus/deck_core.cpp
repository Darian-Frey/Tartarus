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

// Options deck model. See deck_core.h.

#include "deck_core.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>

namespace tartarus {

namespace {

bool HasPrefix(const std::string &s, const char *p) { return s.rfind(p, 0) == 0; }

std::string Number(double v, CvarType type) {
  char buf[32];
  if (type == CvarType::Int)
    std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::lround(v)));
  else
    std::snprintf(buf, sizeof(buf), "%.2f", v);
  return buf;
}

double Value(const Cvar &cv) { return cv.Type() == CvarType::Int ? cv.GetInt() : cv.GetFloat(); }

DeckRow MakeRow(const Cvar &cv, const char *label, double step) {
  DeckRow r;
  r.label = label != nullptr ? label : cv.Name();
  r.cvar = cv.Name();
  r.help = cv.Description();
  r.min = cv.Min();
  r.max = cv.Max();
  if (cv.Type() == CvarType::Bool) {
    r.kind = DeckKind::Toggle;
  } else {
    r.kind = DeckKind::Slider;
    r.step = step > 0 ? step : (cv.Type() == CvarType::Int ? 1.0 : (r.max - r.min) / 20.0);
  }
  return r;
}

} // namespace

const std::vector<DeckPageDef> &DefaultDeckPages() {
  static const std::vector<DeckPageDef> pages = {
      {"GRAPHICS",
       {{"r_resolution", "Resolution", 0, {}, true},
        {"r_fullscreen", "Fullscreen", 0, {}, true},
        {"r_renderscale", "Render scale", 0.25, {}, true},
        {"r_fov", "Field of view", 5},
        {"r_vsync", "Vertical sync"},
        {"r_vsync_adaptive", "Adaptive sync"},
        {"r_maxfps", "Frame limit", 0, {{"60", "60"}, {"120", "120"}, {"144", "144"}, {"240", "240"}, {"MAX", "0"}}},
        {"r_msaa", "Anti-aliasing", 0, {{"OFF", "0"}, {"2X", "2"}, {"4X", "4"}, {"8X", "8"}}, true},
        {"r_anisotropy", "Anisotropic", 0, {{"OFF", "1"}, {"2X", "2"}, {"4X", "4"}, {"8X", "8"}, {"16X", "16"}}},
        {"r_trilinear", "Trilinear"},
        {"r_depthbits", "Depth buffer", 0, {{"16 BIT", "16"}, {"24 BIT", "24"}}, true},
        {"r_gamma", "Brightness", 0.1},
        {"r_bar_fill", "Menu side bars", 0, {{"BLACK", "0"}, {"BLURRED", "1"}, {"ALWAYS", "2"}}}},
       {"r_"}},
      {"CONTROLS",
       {{"m_mode", "Mouse mode", 0, {{"CLASSIC", "0"}, {"JOYSTICK", "1"}, {"AUTO-CENTRE", "2"}}},
        {"m_vj_sensitivity", "Speed", 0.25},
        {"m_vj_centre_rate", "Centre speed", 0.25},
        {"m_vj_invert_y", "Invert pitch"},
        {"m_vj_indicator", "Show indicator"},
        {"m_vj_roll_x", "Left/right rolls"},
        {"m_raw", "Raw stock mouse"},
        {"m_sensitivity", "Stock mouse speed", 0.25},
        {"m_accel", "Desktop acceleration"},
        {"m_vj_curve", "Response curve", 0.1},
        {"m_vj_deadzone", "Dead zone", 0.01},
        {"m_vj_radius", "Full-turn travel", 0.02},
        {"m_vj_recentre_button", "Recentre button", 0,
         {{"NONE", "0"}, {"LEFT", "1"}, {"RIGHT", "2"}, {"MIDDLE", "3"}, {"BUTTON 4", "4"}, {"BUTTON 5", "5"}}}},
       {"m_"},
       {{"Key bindings", DeckAction::KeyBindings}}},
      {"AUDIO", {{"s_sfx_volume", "Sound volume", 0.1}, {"s_music_volume", "Music volume", 0.1}}, {"s_"}},
      {"GAMEPLAY",
       {{"hud_sv_mirror", "Rear-view mirror", 0,
         {{"STOCK", "0"}, {"TOP LEFT", "1"}, {"TOP RIGHT", "2"}, {"CUSTOM", "3"}}},
        {"con_pause", "Pause in menus"}},
       {"g_"}},
      {"TARTARUS",
       {{"classic", "Classic Mode"},
        {"ui_scanlines", "Scanlines", 4},
        {"ui_phosphor", "Phosphor", 0, {{"AMBER", "0"}, {"GREEN", "1"}, {"ICE", "2"}, {"PLAIN", "3"}}},
        {"ui_casing", "Casing", 0, {{"GUNMETAL", "0"}, {"PUTTY", "1"}}}},
       {"ui_"},
       {{"Stock options", DeckAction::StockOptions}, {"Done", DeckAction::Done}}},
  };
  return pages;
}

std::vector<DeckPage> BuildDeckPages(const CvarRegistry &reg, const std::vector<DeckPageDef> &defs) {
  std::vector<DeckPage> out;
  for (const DeckPageDef &def : defs) {
    DeckPage page;
    page.name = def.name;
    std::set<std::string> seen;
    for (const DeckRowDef &d : def.rows) {
      const Cvar *cv = reg.Find(d.cvar);
      if (cv == nullptr)
        continue;
      DeckRow row = MakeRow(*cv, d.label, d.step);
      row.needs_restart = d.needs_restart;
      if (!d.choices.empty()) {
        row.kind = DeckKind::Choice;
        row.choices = d.choices;
      }
      seen.insert(d.cvar);
      page.rows.push_back(std::move(row));
    }
    for (const Cvar *cv : reg.List()) {
      if (seen.count(cv->Name()) || !cv->HasFlag(CVAR_ARCHIVE) || cv->HasFlag(CVAR_READONLY) ||
          cv->HasFlag(CVAR_CHEAT) || cv->Type() == CvarType::String)
        continue;
      const bool match = std::any_of(def.auto_prefixes.begin(), def.auto_prefixes.end(),
                                     [&](const char *p) { return HasPrefix(cv->Name(), p); });
      if (match)
        page.rows.push_back(MakeRow(*cv, nullptr, 0));
    }
    for (const auto &[label, action] : def.actions) {
      DeckRow row;
      row.label = label;
      row.kind = DeckKind::Action;
      row.action = action;
      page.rows.push_back(std::move(row));
    }
    out.push_back(std::move(page));
  }
  return out;
}

void DeckSetChoices(std::vector<DeckPage> &pages, const std::string &cvar,
                    std::vector<std::pair<std::string, std::string>> choices) {
  for (DeckPage &p : pages) {
    for (DeckRow &r : p.rows) {
      if (r.cvar == cvar) {
        r.kind = DeckKind::Choice;
        r.choices = std::move(choices);
        return;
      }
    }
  }
}

namespace {
int ChoiceIndex(const DeckRow &row, const Cvar &cv) {
  const double v = Value(cv);
  int best = 0;
  double best_d = 1e30;
  for (size_t i = 0; i < row.choices.size(); ++i) {
    const double d = std::fabs(std::atof(row.choices[i].second.c_str()) - v);
    if (d < best_d) {
      best_d = d;
      best = static_cast<int>(i);
    }
  }
  return best;
}
} // namespace

std::string DeckValueText(const DeckRow &row, const Cvar &cv) {
  switch (row.kind) {
  case DeckKind::Toggle:
    return cv.GetBool() ? "ON" : "OFF";
  case DeckKind::Choice:
    return row.choices.empty() ? "" : row.choices[static_cast<size_t>(ChoiceIndex(row, cv))].first;
  case DeckKind::Slider:
    return Number(Value(cv), cv.Type());
  case DeckKind::Action:
    break;
  }
  return "";
}

double DeckFraction(const DeckRow &row, const Cvar &cv) {
  if (row.max <= row.min)
    return 0.0;
  return std::clamp((Value(cv) - row.min) / (row.max - row.min), 0.0, 1.0);
}

std::string DeckNextValue(const DeckRow &row, const Cvar &cv, int dir) {
  switch (row.kind) {
  case DeckKind::Toggle:
    return cv.GetBool() ? "0" : "1";
  case DeckKind::Choice: {
    const int n = static_cast<int>(row.choices.size());
    if (n == 0)
      return cv.GetString();
    return row.choices[static_cast<size_t>((ChoiceIndex(row, cv) + dir + n) % n)].second;
  }
  case DeckKind::Slider: {
    // Snap to the step grid, so odd stored values rejoin it.
    const double steps = std::round((Value(cv) - row.min) / row.step) + dir;
    return Number(std::clamp(row.min + steps * row.step, row.min, row.max), cv.Type());
  }
  case DeckKind::Action:
    break;
  }
  return cv.GetString();
}

} // namespace tartarus
