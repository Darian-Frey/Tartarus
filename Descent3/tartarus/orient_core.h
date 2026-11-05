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

// Orientation maths (F-040 - F-042, F-046, SPEC.md §9). Pure standard C++, unit-tested; the
// drawing is in orient_hud.cpp.

#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace tartarus {

struct UpTick {
  float angle = 0; // radians from the top of the screen, clockwise
  float alpha = 0; // 0 hidden, 1 full
};

/// up_right and up_up are world up measured along the ship's right and up
/// axes. The tick fades in between fade_deg and three times that from upright,
/// and fades out when up points along the view, where its direction means
/// nothing.
UpTick ComputeUpTick(float up_right, float up_up, float fade_deg);

struct Point2 {
  float x = 0, y = 0;
};

/// Pitch of the nose above the horizon, from world up along the ship's forward axis.
float PitchDegrees(float up_forward);

/// Where a pitch mark sits on the attitude ball, in ball radii along the
/// screen direction of world up. The horizon is the mark at 0 degrees.
float BallOffset(float mark_deg, float pitch_deg, float range_deg);

/// The part of the unit disc above the line y = h, as a convex polygon.
/// Empty if the line is above the disc; the whole disc if below it.
std::vector<Point2> DiscAbove(float h, int segments);

/// The part below it: DiscAbove() turned over.
std::vector<Point2> DiscBelow(float h, int segments);

struct GizmoAxis {
  float x = 0, y = 0;  // on screen, y up, length 1 at most
  float depth = 0;     // positive points away from the viewer
  int axis = 0;        // 0 X, 1 Y, 2 Z
};

/// World axes as the ship sees them, farthest first so nearer ones draw over
/// them. right, up and forward are the ship's axes in world space.
std::array<GizmoAxis, 3> ComputeGizmo(const float right[3], const float up[3], const float forward[3]);

/// The world view's camera, so that lines drawn on the HUD sit on the real
/// world. fx and fy are pixels per unit of x/z and y/z.
struct ViewCamera {
  float right[3], up[3], forward[3];
  float cx, cy, fx, fy;
};

/// Pixels per unit for a window and the engine's zoom, as g3_StartFrame().
void ViewCameraScale(ViewCamera *camera, float window_w, float window_h, float zoom);

/// A world direction on screen. False if it points behind the camera.
bool ProjectDirection(const ViewCamera &camera, const float direction[3], Point2 *out);

struct ScreenRect {
  float x1, y1, x2, y2;
};

/// The part of a line inside the rectangle (Liang-Barsky). False if none.
bool ClipLine(const ScreenRect &rect, Point2 *a, Point2 *b);

/// The nose's direction on the ground, as x and z. Near straight up or down
/// that has no meaning, so the previous value is kept.
void NoseHeading(const float forward[3], float *heading_x, float *heading_z);

struct LadderBar {
  Point2 outer_left, inner_left, inner_right, outer_right;
};

/// A pitch mark in line with the nose, on the real sky. Each half runs from
/// inner_deg to outer_deg either side. False if any of it is behind the camera.
bool ComputeLadderBar(const ViewCamera &camera, float heading_x, float heading_z, float mark_deg, float inner_deg,
                      float outer_deg, LadderBar *bar);

/// Compass bearing of a ground direction, 0 to 360, with +Z as north.
float HeadingDegrees(float heading_x, float heading_z);

struct HeadingMark {
  float offset = 0; // -1 at the left end of the tape, 1 at the right
  int degrees = 0;  // 0 to 359
};

/// The marks on a tape span degrees each side of heading, every step degrees.
std::vector<HeadingMark> HeadingMarks(float heading, float span, int step);

/// N, E, S and W, otherwise tens of degrees: "28" for 280.
void HeadingLabel(int degrees, char *text, size_t size);

/// Where the roll pointer sits, limited to the arc. *pinned is set if rolled past it.
float RollPointer(float roll, float limit, bool *pinned);

struct TapeMark {
  float offset = 0; // -1 at the bottom of the tape, 1 at the top
  int value = 0;
};

/// The marks on a vertical tape span either side of value, every step, none below zero.
std::vector<TapeMark> TapeMarks(float value, float span, int step);

/// Eases *value toward target, as SmoothVelocity().
void SmoothValue(float *value, float target, float frame_time, float time_constant);

/// Climb rate with its sign, and no sign at zero.
void ClimbLabel(float climb, char *text, size_t size);

/// Reticle styles (F-047). Stock is upstream's bitmaps.
enum ReticleStyle { kReticleStock, kReticleGunsight, kReticleBrackets, kReticleChevrons, kReticleCross, kReticleStyles };

struct ReticleLine {
  float x1, y1, x2, y2; // in reticle radii, y down
};

/// A style's lines. Empty for stock.
std::vector<ReticleLine> ReticleLines(int style);

/// True if the style has a dot at the centre.
bool ReticleHasDot(int style);

/// Weapon slots across the ship, from upstream's gunpoint bits.
enum ReticleSlot { kSlotLeft2, kSlotLeft1, kSlotCentre, kSlotRight1, kSlotRight2, kReticleSlots };

struct ReticleBank {
  bool exists[kReticleSlots] = {};
  bool ready[kReticleSlots] = {};
};

/// Decodes upstream's masks: gunpoint 0 and the mass driver's bit 8 are the
/// centre, 1 and 3 the right, 2 and 4 the left (Ret_prim_wb in hud.cpp).
ReticleBank DecodeReticleMask(unsigned exists_mask, unsigned ready_mask);

/// An angle wrapped to -180 to 180.
float WrapDegrees(float degrees);

/// The nearest of the four uprights (F-043): 0, 90, 180 or -90, as a roll in degrees.
float NearestUpright(float roll_deg);

/// Roll rate toward upright for an error in degrees: error / time_constant, limited.
float LevelRate(float error_deg, float time_constant, float limit);

/// That rate as bank input: positive rolls left, as the bank-left key. Full
/// thrust against the ship's drag gives its top rate, in 65536ths of a turn.
float BankCommand(float rate_deg, float full_rotthrust, float rotdrag);

/// What the flight HUD's status words report.
struct FlightStatus {
  bool slide = false, bank = false;                       // flying modes
  bool hold = false, snap = false, boost = false, light = false; // assists and systems
};

/// The words to show, in order: flying modes for the left of the tape,
/// assists and systems for the right. Only what is active.
std::vector<const char *> StatusWordsLeft(const FlightStatus &status);
std::vector<const char *> StatusWordsRight(const FlightStatus &status);

struct FlightMarker {
  Point2 position;
  bool reversed = false; // moving backwards: this is the opposite of the direction of travel
  bool clamped = false;  // really outside the rectangle
};

/// Eases *smoothed toward velocity with the given time constant, whatever the
/// frame rate. Raw velocity jumps with every small correction and wall bump.
void SmoothVelocity(float smoothed[3], const float velocity[3], float frame_time, float time_constant);

/// Marker strength: 0 up to min_speed, 1 from twice that. Direction means
/// little when barely moving, and a fade cannot flicker at a threshold.
float FlightMarkerAlpha(const float velocity[3], float min_speed);

/// Where the ship is going. False if not moving at all.
bool ComputeFlightMarker(const ViewCamera &camera, const float velocity[3], const ScreenRect &rect, FlightMarker *marker);

} // namespace tartarus
