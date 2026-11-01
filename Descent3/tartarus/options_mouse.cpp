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

// Options menu "Mouse flight" tab. See options_mouse.h.

#include "options_mouse.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "classic.h"
#include "cvar.h"
#include "newui.h"

namespace tartarus {

namespace {

constexpr int16_t kOptionId = 0x2201; // clear of upstream's IDV_* option ids
constexpr float kSensMin = 0.25f;
constexpr float kSensMax = 4.0f;
constexpr int16_t kSensSteps = 15; // 0.25 per step
constexpr float kCentreMin = 0.25f; // m_vj_centre_rate, per second (used by the auto-centre mode)
constexpr float kCentreMax = 5.0f;
constexpr int16_t kCentreSteps = 19; // 0.25 per step

int16_t SliderPos(float v, float lo, float hi, int16_t steps) {
  v = std::clamp(v, lo, hi);
  return static_cast<int16_t>(std::lround((v - lo) / (hi - lo) * steps));
}
float SliderValue(int16_t pos, float lo, float hi, int16_t steps) {
  return lo + (hi - lo) * std::clamp<int>(pos, 0, steps) / steps;
}
std::string Fixed2(float v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.2f", v);
  return buf;
}

int CvarInt(const char *name, int fallback) {
  const Cvar *c = Cvars().Find(name);
  return c != nullptr ? c->GetInt() : fallback;
}
float CvarFloat(const char *name, float fallback) {
  const Cvar *c = Cvars().Find(name);
  return c != nullptr ? c->GetFloat() : fallback;
}
bool CvarBool(const char *name, bool fallback) {
  const Cvar *c = Cvars().Find(name);
  return c != nullptr ? c->GetBool() : fallback;
}

void SetCvar(const char *name, const std::string &value) {
  Cvars().Set(name, value, CvarSource::User); // User: validated and saved to tartarus.cfg
}

} // namespace

void MouseOptionsSheet::Setup(newuiMenu *menu) {
  sheet_ = menu->AddOption(kOptionId, "Mouse", NEWUIMENU_MEDIUM);
  classic_ = IsClassic();
  mode_ = nullptr;
  sensitivity_ = centre_rate_ = nullptr;
  invert_y_ = indicator_ = roll_x_ = nullptr;

  if (classic_) {
    sheet_->NewGroup(nullptr, 0, 0);
    sheet_->AddText("Classic Mode uses the stock Descent 3 mouse.");
    sheet_->AddText("");
    sheet_->AddText("For the virtual joystick, switch Classic Mode off:");
    sheet_->AddText("type 'classic 0' in the console (` key),");
    sheet_->AddText("or start the game with -tartarus.");
    return;
  }

  sheet_->NewGroup("Mouse mode", 0, 0);
  mode_ = sheet_->AddFirstLongRadioButton("Classic");
  sheet_->AddLongRadioButton("Virtual joystick");
  sheet_->AddLongRadioButton("Joystick, auto-centre");
  *mode_ = std::clamp(CvarInt("m_mode", 1), 0, 2);

  tSliderSettings slider{};
  slider.type = SLIDER_UNITS_FLOAT;
  slider.min_val.f = kSensMin;
  slider.max_val.f = kSensMax;
  sheet_->NewGroup("Sensitivity", 0, 80);
  sensitivity_ = sheet_->AddSlider(
      "Speed", kSensSteps, SliderPos(CvarFloat("m_vj_sensitivity", 1.0f), kSensMin, kSensMax, kSensSteps), &slider);
  slider.min_val.f = kCentreMin;
  slider.max_val.f = kCentreMax;
  centre_rate_ = sheet_->AddSlider("Centre speed", kCentreSteps,
                                   SliderPos(CvarFloat("m_vj_centre_rate", 1.5f), kCentreMin, kCentreMax, kCentreSteps),
                                   &slider);

  sheet_->NewGroup("Options", 0, 160);
  invert_y_ = sheet_->AddLongCheckBox("Invert pitch");
  indicator_ = sheet_->AddLongCheckBox("Show indicator");
  roll_x_ = sheet_->AddLongCheckBox("Left/right rolls");
  *invert_y_ = CvarBool("m_vj_invert_y", false);
  *indicator_ = CvarBool("m_vj_indicator", true);
  *roll_x_ = CvarBool("m_vj_roll_x", false);
}

void MouseOptionsSheet::Finish() {
  if (classic_ || mode_ == nullptr)
    return;
  SetCvar("m_mode", std::to_string(std::clamp(*mode_, 0, 2)));
  SetCvar("m_vj_sensitivity", Fixed2(SliderValue(*sensitivity_, kSensMin, kSensMax, kSensSteps)));
  SetCvar("m_vj_centre_rate", Fixed2(SliderValue(*centre_rate_, kCentreMin, kCentreMax, kCentreSteps)));
  SetCvar("m_vj_invert_y", *invert_y_ ? "1" : "0");
  SetCvar("m_vj_indicator", *indicator_ ? "1" : "0");
  SetCvar("m_vj_roll_x", *roll_x_ ? "1" : "0");
}

} // namespace tartarus
