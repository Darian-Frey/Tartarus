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

// The player cloak's state machine (F-020, SPEC.md §6, AV-007), without engine
// headers so every transition can be unit-tested. cloak.cpp drives it and
// turns the engine's invisibility on and off.

#pragma once

namespace tartarus {

enum class CloakState { Ready, Active, Cooldown };

/// How the cloak ended this frame, if it did: the cooldown that follows depends on it.
enum class CloakEnd { None, Voluntary, Forced };

struct CloakSettings {
  float cost = 30.0f;       // energy charged on activation
  float drain = 8.0f;       // energy a second while active
  float min_energy = 40.0f; // activation refused below this
  float max_time = 8.0f;    // seconds; then it drops by itself
  float cooldown = 3.0f;    // after a voluntary drop, a timeout or running out of energy
  float cooldown_forced = 6.0f; // after firing, a bump or a hit
  float floor = 20.0f;      // the drain stops here and the cloak drops: never down to 0
};

/// Energy needed to start: min_energy, and at least the cost, the floor and one
/// second of drain, so a cloak never starts already at the floor.
float CloakEnergyToStart(const CloakSettings &s);

class CloakMachine {
public:
  CloakState State() const { return state_; }
  /// Seconds left: of the cloak while active, of the cooldown while cooling down.
  float Left() const { return left_; }
  /// Left() as a share of the whole (1 just started, 0 about to end); 0 when ready.
  float Fraction() const;

  /// Ready and at least CloakEnergyToStart(): charges the cost and starts. False otherwise.
  bool TryActivate(float *energy, const CloakSettings &s);
  /// Per frame: drains energy while active (never below the floor), ends at
  /// max_time or at the floor, and counts the cooldown down to Ready.
  CloakEnd Tick(float dt, float *energy, const CloakSettings &s);
  /// Ends an active cloak now (forced: fired, bumped or hit). None if it was not active.
  CloakEnd Drop(bool forced, const CloakSettings &s);
  /// Straight back to Ready: death, a new level, Classic Mode.
  void Reset();

private:
  CloakEnd End(bool forced, const CloakSettings &s);

  CloakState state_ = CloakState::Ready;
  float left_ = 0.0f;
  float whole_ = 0.0f;
};

} // namespace tartarus
