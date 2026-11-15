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

#include "cloak_core.h"

#include <algorithm>

namespace tartarus {

float CloakEnergyToStart(const CloakSettings &s) {
  return std::max(s.min_energy, s.cost + s.floor + s.drain);
}

float CloakMachine::Fraction() const {
  if (state_ == CloakState::Ready || whole_ <= 0.0f)
    return 0.0f;
  return std::clamp(left_ / whole_, 0.0f, 1.0f);
}

bool CloakMachine::TryActivate(float *energy, const CloakSettings &s) {
  if (state_ != CloakState::Ready || *energy < CloakEnergyToStart(s) || s.max_time <= 0.0f)
    return false;
  *energy = std::max(0.0f, *energy - s.cost);
  state_ = CloakState::Active;
  left_ = whole_ = s.max_time;
  return true;
}

CloakEnd CloakMachine::End(bool forced, const CloakSettings &s) {
  const float cooldown = forced ? s.cooldown_forced : s.cooldown;
  if (cooldown > 0.0f) {
    state_ = CloakState::Cooldown;
    left_ = whole_ = cooldown;
  } else {
    Reset();
  }
  return forced ? CloakEnd::Forced : CloakEnd::Voluntary;
}

CloakEnd CloakMachine::Tick(float dt, float *energy, const CloakSettings &s) {
  if (dt <= 0.0f)
    return CloakEnd::None;
  if (state_ == CloakState::Cooldown) {
    left_ -= dt;
    if (left_ <= 0.0f)
      Reset();
    return CloakEnd::None;
  }
  if (state_ != CloakState::Active)
    return CloakEnd::None;
  *energy = std::max(std::min(*energy, s.floor), *energy - s.drain * dt);
  left_ -= dt;
  if (*energy <= s.floor || left_ <= 0.0f)
    return End(false, s);
  return CloakEnd::None;
}

CloakEnd CloakMachine::Drop(bool forced, const CloakSettings &s) {
  if (state_ != CloakState::Active)
    return CloakEnd::None;
  return End(forced, s);
}

void CloakMachine::Reset() {
  state_ = CloakState::Ready;
  left_ = whole_ = 0.0f;
}

} // namespace tartarus
