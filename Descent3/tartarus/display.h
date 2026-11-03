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

// Display settings that survive restarts (BUG-004, BUG-005). Compatibility fix:
// active in Classic Mode too (D-004).

#pragma once

#include <string>
#include <utility>
#include <vector>

namespace tartarus {

/// Remembers the saved fullscreen preference before -windowed / -fullscreen
/// override it. Called from Tartarus_Init() (after LoadGameSettings, before
/// rend_InitWindowMode).
void Display_Init();

/// Called by ConfigureDisplayResolutions() once the display's mode list is
/// built: returns the index of the saved width x height, or `default_id`.
int Display_RestoreResolution(const std::vector<std::pair<int, int>> &modes, int default_id);

/// Field of view (F-060). Upstream allows 72-90 degrees; Tartarus mode allows
/// 60-120, Classic Mode keeps 72-90. The value is upstream's Render_FOV_setting
/// (saved as RS_fov); the `r_fov` cvar mirrors it for the console. Like
/// upstream's, it is the horizontal FOV of a 4:3 view; wider windows see more
/// at the sides (Hor+).
inline constexpr int kFovAbsMin = 60;
inline constexpr int kFovAbsMax = 120;
int Display_FovMin();
int Display_FovMax();
/// Clamps to the current mode's range and applies it (Options slider, r_fov).
void Display_SetFov(float degrees);

/// Render scale (F-068, r_renderscale 0.5-2.0): the game frame renders at
/// this multiple of the chosen resolution and is scaled to the window, so
/// above 1 is supersampling and below 1 is faster. Called from
/// SetScreenMode(SM_GAME) on the size it is about to use. Menus keep 640x480.
void Display_ScaleRenderSize(int *width, int *height);

/// Called from SetScreenMode(SM_GAME) before InitGameScreen(): the pilot's
/// saved 3D-window size is in frame pixels, so a size at or above the
/// unscaled resolution (or the frame) means "full window" at any scale.
void Display_AdjustGameWindow(int *width, int *height);

/// Display mode for the options deck (F-069): r_resolution (index into the
/// display's mode list) and r_fullscreen mirror the stock settings. Changes
/// wait here until Display_ApplyPending(), so stepping through the list does
/// not change the screen each time.
void Display_SyncModeCvars();
/// Copies a changed r_resolution / r_fullscreen into the stock settings and
/// rebuilds the screen, as the Video tab does. Returns true if anything changed.
bool Display_ApplyPending();
/// Resolutions offered in the deck: (label, index), small modes left out.
std::vector<std::pair<std::string, std::string>> Display_ModeChoices();

/// Called by SaveGameSettings() after upstream's writes: stores the resolution
/// by size and rewrites RS_fullscreen so command-line flags stay session-only.
void Display_SaveSettings(int width, int height, bool fullscreen_now);

} // namespace tartarus
