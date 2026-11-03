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

// Options deck (F-069, D-016, IMP-006): the Tartarus options screen, opened
// with F2 in a single-player level in Tartarus mode. Classic Mode and
// multiplayer keep the stock Options menu. Drawn as an overlay from
// rectangles, lines and scaled text in a 1280x720 layout, so it is the same
// size on screen at any resolution. Pages come from deck_core.

#pragma once

namespace tartarus {

/// Registers ui_* and the audio mirror cvars. Called from Tartarus_Init().
void Deck_Init();

/// Offer a key after the console. F2 opens; while open the deck takes every key.
bool Deck_HandleKey(int key);

/// Draws the deck if open and handles the mouse. Called from RenderOverlays().
void Deck_Render();

bool Deck_IsOpen();
void Deck_Close();

} // namespace tartarus
