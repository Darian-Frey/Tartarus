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

// The gravity well's pull (F-021, F-022, SPEC.md §7, AV-006), without engine
// headers so it can be unit-tested. gravwell.cpp applies it to objects.

#pragma once

namespace tartarus {

struct WellSettings {
  float radius = 60.0f;          // reach, measured to the object's surface
  float time = 1.5f;             // seconds the well lasts
  float force = 420.0f;          // peak acceleration toward the centre, units/s²
  float mass_ref = 4.0f;         // objects this heavy or lighter get the full pull
  float mass_min_scale = 0.15f;  // the heaviest still get this share
  float immune_mass = 40.0f;     // this heavy or more (bosses): no pull; 0 = none immune
};

/// Acceleration toward the well for an object whose surface is surface_dist
/// from the centre, with life_left seconds of the well remaining: linear
/// falloff to 0 at the radius, fading as the well expires, scaled down for
/// heavy objects. 0 when out of reach, immune or given bad numbers. Never more
/// than s.force.
float WellAcceleration(float surface_dist, float life_left, float mass, const WellSettings &s);

/// The velocity change for one frame, clamped against bad input: accel * dt,
/// or 0 if either is not a finite positive number (AV-006).
float WellDeltaV(float accel, float dt);

} // namespace tartarus
