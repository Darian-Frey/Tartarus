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

// Small-view windows (F-060, SPEC.md §14): size, shape and position of the
// rear-view / Guide-Bot / missile-camera windows in the HUD layout, from the
// hud_sv_* cvars. CLASSIC_LOCKED; cockpit monitors are untouched.

#pragma once

namespace tartarus {

/// Registers the hud_sv_* cvars. Called from Tartarus_Init().
void SmallViews_Init();

/// Rear-view mirror presets for the left window (F-060): 0 stock, 1 top left,
/// 2 top right. SmallViews_CurrentMirror() returns kMirrorCustom when the
/// hud_sv_left_* values match no preset.
inline constexpr int kMirrorPresets = 3;
inline constexpr int kMirrorCustom = kMirrorPresets;
const char *SmallViews_MirrorLabel(int preset);
int SmallViews_CurrentMirror();
void SmallViews_ApplyMirror(int preset);
/// Refreshes the hud_sv_mirror cvar from the hud_sv_left_* values.
void SmallViews_SyncMirrorCvar();

/// Called from DrawSmallViews() (HUD layout) after upstream computes a
/// window's rectangle. Leaves it alone in Classic Mode or when the window's
/// cvars are at their defaults; otherwise replaces it.
void SmallViews_Adjust(int slot, bool bigger, int *left, int *top, int *right, int *bot);

} // namespace tartarus
