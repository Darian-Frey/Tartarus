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

// Virtual-joystick offset model (F-067, SPEC.md §13, AV-013). Pure standard
// C++ so it can be unit-tested; mouse_flight.cpp feeds it mouse counts and
// turns its output into rotation rates.

#pragma once

namespace tartarus {

/// Response curve for one axis: 0 inside the dead zone, then
/// sign(v) * ((|v| - dz) / (1 - dz))^curve, clamped to [-1, 1].
float VjCurve(float v, float deadzone, float curve);

class VirtualJoystick {
public:
  /// Mouse counts since the last call. `radius_counts` is the full-rate
  /// radius in counts (m_vj_radius * screen height); the offset vector is
  /// clamped to length 1.
  void AddDelta(float dx, float dy, float radius_counts, float sensitivity);

  /// Auto-centring: offset *= exp(-rate * dt). No-op for rate <= 0.
  void Decay(float rate, float dt);

  void Reset() { x_ = y_ = 0.0f; }

  /// Offset in [-1, 1] per axis; y is positive when the mouse moved back
  /// (towards the player), as a joystick axis.
  float X() const { return x_; }
  float Y() const { return y_; }
  bool IsCentred() const { return x_ == 0.0f && y_ == 0.0f; }

private:
  float x_ = 0.0f;
  float y_ = 0.0f;
};

} // namespace tartarus
